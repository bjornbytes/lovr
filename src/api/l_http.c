#include "api.h"
#include "http/http.h"
#include "data/blob.h"
#include "util.h"
#include <stdlib.h>

static bool isunreserved(char c) {
  switch (c) {
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i':
    case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
    case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0':
    case '-': case '_': case '.': case '~':
      return true;
    default:
      return false;
  }
}

static size_t urlencode(char* dst, const char* str, size_t len) {
  size_t res = 0;
  for (size_t i = 0; i < len; i++, str++) {
    if (isunreserved(*str)) {
      dst[res++] = *str;
    } else {
      dst[res++] = '%';
      dst[res++] = '0' + *str / 16;
      dst[res++] = '0' + *str % 16;
    }
  }
  return res;
}

static void onHeader(void* userdata, const char* name, size_t nameLength, const char* value, size_t valueLength) {
  lua_State* L = userdata;
  lua_pushlstring(L, name, nameLength);
  lua_pushlstring(L, value, valueLength);
  lua_settable(L, 3);
}

static int l_lovrHttpRequest(lua_State* L) {
  Request request = { 0 };
  Blob* blob = NULL;

  arr_t(char) body;
  arr_init(&body, arr_alloc);

  request.url = luaL_checkstring(L, 1);

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "data");
    if (lua_type(L, -1) == LUA_TSTRING) {
      request.data = lua_tolstring(L, -1, &request.size);
    } else if (lua_type(L, -1) == LUA_TTABLE) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1)) {
          size_t keyLength, valLength;
          const char* key = lua_tolstring(L, -2, &keyLength);
          const char* val = lua_tolstring(L, -1, &valLength);
          arr_expand(&body, 3 * keyLength + 1 + 3 * valLength + 1);
          body.length += urlencode(body.data, key, keyLength);
          arr_push(&body, '=');
          body.length += urlencode(body.data, val, valLength);
          arr_push(&body, '&');
        }
        lua_pop(L, 1);
      }
      request.data = body.data;
      request.size = body.length - 1;
    } else if ((blob = luax_totype(L, -1, Blob)) != NULL) {
      request.data = blob->data;
      request.size = blob->size;
    } else if (!lua_isnil(L, -1)) {
      lovrThrow("Expected string, table, or Blob for request data");
      return 0;
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "method");
    if (!lua_isnil(L, -1)) request.method = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "headers");
    if (lua_istable(L, -1)) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1)) {
          request.headers = realloc(request.headers, (request.headerCount + 1) * 2 * sizeof(char*));
          lovrAssert(request.headers, "Out of memory");
          request.headers[request.headerCount * 2 + 0] = lua_tostring(L, -2);
          request.headers[request.headerCount * 2 + 1] = lua_tostring(L, -1);
          request.headerCount++;
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  Response response = {
    .onHeader = onHeader,
    .userdata = L
  };

  lua_settop(L, 2);
  lua_newtable(L);
  bool success = lovrHttpRequest(&request, &response);
  free(request.headers);
  arr_free(&body);

  if (!success) {
    lua_pushinteger(L, 0);
    return 1;
  }

  lua_pushinteger(L, response.status);
  lua_pushlstring(L, response.data, response.size);
  lua_pushvalue(L, -3);
  free(response.data);
  return 3;
}

static const luaL_Reg lovrHttp[] = {
  { "request", l_lovrHttpRequest },
  { NULL, NULL }
};

int luaopen_lovr_http(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrHttp);
  if (lovrHttpInit()) {
    luax_atexit(L, lovrHttpDestroy);
  }
  return 1;
}
