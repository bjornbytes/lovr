#include "http/http.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>

static struct {
  bool initialized;
  HINTERNET handle;
} state;

bool lovrHttpInit(void) {
  if (state.initialized) return false;
  state.handle = InternetOpen("LOVR", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
  state.initialized = true;
  return true;
}

void lovrHttpDestroy(void) {
  if (!state.initialized) return;
  if (state.handle) InternetCloseHandle(state.handle);
  memset(&state, 0, sizeof(state));
}

bool lovrHttpRequest(Request* req, Response* res) {
  if (!state.handle) {
    return false;
  }

  if (req->size > UINT32_MAX) {
    return false;
  }

  // parse URL (rejects <username>[:<password>]@ and :<port>)
  size_t length = strlen(req->url);
  const char* url = req->url;
  bool https = false;

  if (length > 8 && !memcmp(url, "https://", 8)) {
    https = true;
    length -= 8;
    url += 8;
  } else if (length > 7 && !memcmp(url, "http://", 7)) {
    length -= 7;
    url += 7;
  } else {
    return false;
  }

  if (strchr(url, '@') || strchr(url, ':')) {
    return false;
  }

  char host[256];
  char* path = strchr(url, '/');
  size_t hostLength = path ? path - url : length;
  if (sizeof(host) > hostLength) {
    memcpy(host, url, hostLength);
    host[hostLength] = '\0';
  } else {
    return false;
  }

  // connection
  INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
  HINTERNET connection = InternetConnectA(state.handle, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!connection) {
    return false;
  }

  // setup request
  const char* method = req->method ? req->method : (req->data ? "POST" : "GET");
  DWORD flags = 0;
  flags |= INTERNET_FLAG_NO_AUTH;
  flags |= INTERNET_FLAG_NO_CACHE_WRITE;
  flags |= INTERNET_FLAG_NO_COOKIES;
  flags |= INTERNET_FLAG_NO_UI;
  flags |= https ? INTERNET_FLAG_SECURE : 0;
  HINTERNET request = HttpOpenRequestA(connection, method, path, NULL, NULL, NULL, flags, 0);
  if (!request) {
    InternetCloseHandle(connection);
    return false;
  }

  // request headers
  if (req->headerCount >= 0) {
    arr_t(char) header;
    arr_init(&header, arr_alloc);
    for (uint32_t i = 0; i < req->headerCount; i++) {
      const char* name = *req->headers++;
      const char* value = *req->headers++;
      arr_clear(&header);
      arr_append(&header, name, strlen(name));
      arr_append(&header, ": ", 2);
      arr_append(&header, value, strlen(value));
      arr_append(&header, "\r\n", 2);
      if (header.length > UINT32_MAX) continue;
      HttpAddRequestHeadersA(request, header.data, (DWORD) header.length, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    }
    arr_free(&header);
  }

  // do the thing
  bool success = HttpSendRequestA(request, NULL, 0, (void*) req->data, (DWORD) req->size);
  if (!success) {
    InternetCloseHandle(connection);
    return false;
  }

  // status
  DWORD status;
  DWORD bufferSize = sizeof(status);
  DWORD index = 0;
  HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &bufferSize, &index);
  res->status = status;
  index = 0;

  // response headers
  char stack[1024];
  char* buffer = stack;
  bufferSize = sizeof(stack);
  success = HttpQueryInfoA(request, HTTP_QUERY_RAW_HEADERS, buffer, &bufferSize, &index);
  if (!success) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      buffer = malloc(bufferSize);
      lovrAssert(buffer, "Out of memory");
      success = HttpQueryInfoA(request, HTTP_QUERY_RAW_HEADERS, buffer, &bufferSize, &index);
    }

    if (!success) {
      if (buffer != stack) free(buffer);
      InternetCloseHandle(request);
      InternetCloseHandle(connection);
      return false;
    }
  }

  char* header = buffer;
  while (*header) {
    size_t length = strlen(header);
    char* colon = strchr(header, ':');
    if (colon && colon != header && length >= (size_t) (colon - header + 2)) {
      char* name = header;
      char* value = colon + 2;
      size_t nameLength = colon - header;
      size_t valueLength = length - (colon - header + 2);
      res->onHeader(res->userdata, name, nameLength, value, valueLength);
    }
    header += length + 1;
  }

  if (buffer != stack) {
    free(buffer);
  }

  // body
  res->data = NULL;
  res->size = 0;

  for (;;) {
    DWORD bytes = 0;
    if (!InternetQueryDataAvailable(request, &bytes, 0, 0)) {
      free(res->data);
      InternetCloseHandle(request);
      InternetCloseHandle(connection);
      return false;
    }

    if (bytes == 0) {
      break;
    }

    res->data = realloc(res->data, res->size + bytes);
    lovrAssert(res->data, "Out of memory");

    if (InternetReadFile(request, res->data + res->size, bytes, &bytes)) {
      res->size += bytes;
    } else {
      free(res->data);
      InternetCloseHandle(request);
      InternetCloseHandle(connection);
      return false;
    }
  }

  InternetCloseHandle(request);
  InternetCloseHandle(connection);
  return true;
}

#elif defined(__linux__)
#include <curl/curl.h>
#include <dlfcn.h>

typedef CURLcode fn_global_init(long flags);
typedef void fn_global_cleanup(void);
typedef CURL* fn_easy_init(void);
typedef CURLcode fn_easy_setopt(CURL *curl, CURLoption option, ...);
typedef CURLcode fn_easy_perform(CURL *curl);
typedef void fn_easy_cleanup(CURL* curl);
typedef CURLcode fn_easy_getinfo(CURL* curl, CURLINFO info, ...);
typedef struct curl_slist *fn_slist_append(struct curl_slist *list, const char *string);
typedef void fn_slist_free_all(struct curl_slist *list);

#define FN_DECLARE(f) fn_##f* f;
#define FN_LOAD(f) curl.f = (fn_##f*) dlsym(state.library, "curl_"#f);
#define FN_FOREACH(X)\
  X(global_init)\
  X(global_cleanup)\
  X(easy_init)\
  X(easy_setopt)\
  X(easy_perform)\
  X(easy_cleanup)\
  X(easy_getinfo)\
  X(slist_append)\
  X(slist_free_all)

static struct {
  FN_FOREACH(FN_DECLARE)
} curl;

static struct {
  bool initialized;
  void* library;
} state;

bool lovrHttpInit(void) {
  if (state.initialized) return false;

  state.library = dlopen("libcurl.so", RTLD_LAZY);

  if (state.library) {
    FN_FOREACH(FN_LOAD)
    if (curl.global_init(CURL_GLOBAL_DEFAULT)) {
      dlclose(state.library);
      state.library = NULL;
    }
  }

  state.initialized = true;
  return true;
}

void lovrHttpDestroy(void) {
  if (!state.initialized) return;
  if (state.library) {
    curl.global_cleanup();
    dlclose(state.library);
  }
  memset(&state, 0, sizeof(state));
}

static size_t reader(char* buffer, size_t size, size_t count, void* userdata) {
  char** data = userdata;
  memcpy(buffer, *data, size * count);
  *data += size * count;
  return size * count;
}

static size_t writer(void* buffer, size_t size, size_t count, void* userdata) {
  Response* response = userdata;
  response->data = realloc(response->data, response->size + size * count);
  lovrAssert(response->data, "Out of memory");
  memcpy(response->data + response->size, buffer, size * count);
  response->size += size * count;
  return size * count;
}

// would rather use curl_easy_nextheader, but it's too new right now
static size_t onHeader(char* buffer, size_t size, size_t count, void* userdata) {
  Response* response = userdata;
  char* colon = memchr(buffer, ':', size * count);
  if (colon) {
    char* name = buffer;
    char* value = colon + 1;
    size_t nameLength = colon - buffer;
    size_t valueLength = size * count - (nameLength + 1);
    while (valueLength > 0 && (*value == ' ' || *value == '\t')) value++, valueLength--;
    while (valueLength > 0 && (value[valueLength - 1] == '\n' || value[valueLength - 1] == '\r')) valueLength--;
    response->onHeader(response->userdata, name, nameLength, value, valueLength);
  }
  return size * count;
}

bool lovrHttpRequest(Request* request, Response* response) {
  if (!state.library) return false;

  CURL* handle = curl.easy_init();
  if (!handle) return false;

  curl.easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
  curl.easy_setopt(handle, CURLOPT_URL, request->url);

  if (request->method) {
    curl.easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request->method);
    if (!strcmp(request->method, "HEAD")) curl.easy_setopt(handle, CURLOPT_NOBODY, 1);
  }

  if (request->data && (!request->method || (strcmp(request->method, "GET") && strcmp(request->method, "HEAD")))) {
    const char* data = request->data;
    curl_off_t size = request->size;
    curl.easy_setopt(handle, CURLOPT_POST, 1);
    curl.easy_setopt(handle, CURLOPT_READDATA, &data);
    curl.easy_setopt(handle, CURLOPT_READFUNCTION, reader);
    curl.easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, size);
  }

  struct curl_slist* headers = NULL;
  if (request->headerCount > 0) {
    arr_t(char) header;
    arr_init(&header, arr_alloc);
    for (uint32_t i = 0; i < request->headerCount; i++) {
      const char* name = *request->headers++;
      const char* value = *request->headers++;
      arr_clear(&header);
      arr_append(&header, name, strlen(name));
      arr_append(&header, ": ", 2);
      arr_append(&header, value, strlen(value));
      arr_push(&header, '\0');
      headers = curl.slist_append(headers, header.data);
    }
    curl.easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    arr_free(&header);
  }

  curl.easy_setopt(handle, CURLOPT_TIMEOUT, request->timeout);
  curl.easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);

  response->size = 0;
  response->data = NULL;
  curl.easy_setopt(handle, CURLOPT_WRITEDATA, response);
  curl.easy_setopt(handle, CURLOPT_WRITEFUNCTION, writer);

  curl.easy_setopt(handle, CURLOPT_HEADERDATA, response);
  curl.easy_setopt(handle, CURLOPT_HEADERFUNCTION, onHeader);

  curl.easy_perform(handle);

  long status;
  curl.easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
  response->status = status;

  curl.easy_cleanup(handle);
  curl.slist_free_all(headers);
  return true;
}

#else
#error "Unsupported HTTP platform"
#endif
