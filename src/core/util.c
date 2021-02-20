#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

// Error handling
static void defaultErrorCallback(void* p, const char* format, va_list args) {
  fprintf(stderr, "Error: ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
}

LOVR_THREAD_LOCAL errorFn* lovrErrorCallback = defaultErrorCallback;
LOVR_THREAD_LOCAL void* lovrErrorUserdata = NULL;

void lovrSetErrorCallback(errorFn* callback, void* userdata) {
  lovrErrorCallback = callback ? callback : defaultErrorCallback;
  lovrErrorUserdata = userdata;
}

void lovrThrow(const char* format, ...) {
  va_list args;
  va_start(args, format);
  lovrErrorCallback(lovrErrorUserdata, format, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

// Logging
logFn* lovrLogCallback;
void* lovrLogUserdata;

void lovrSetLogCallback(logFn* callback, void* userdata) {
  lovrLogCallback = callback;
  lovrLogUserdata = userdata;
}

void lovrLog(int level, const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  lovrLogCallback(lovrLogUserdata, level, tag, format, args);
  va_end(args);
}

// Refcounting
#if ATOMIC_INT_LOCK_FREE != 2
#error "Lock-free integer atomics are not supported on this platform, but are required for refcounting"
#endif

void lovrRetain(void* object) {
  if (object) {
    atomic_fetch_add((atomic_uint*) object, 1);
  }
}

void lovrRelease(void* object, void (*destructor)(void*)) {
  if (object && atomic_fetch_sub((atomic_uint*) object, 1) == 1) {
    destructor(object);
  }
}

// UTF-8
// https://github.com/starwing/luautf8
size_t utf8_decode(const char *s, const char *e, unsigned *pch) {
  unsigned ch;

  if (s >= e) {
    *pch = 0;
    return 0;
  }

  ch = (unsigned char)s[0];
  if (ch < 0xC0) goto fallback;
  if (ch < 0xE0) {
    if (s+1 >= e || (s[1] & 0xC0) != 0x80)
      goto fallback;
    *pch = ((ch   & 0x1F) << 6) |
            (s[1] & 0x3F);
    return 2;
  }
  if (ch < 0xF0) {
    if (s+2 >= e || (s[1] & 0xC0) != 0x80
                 || (s[2] & 0xC0) != 0x80)
      goto fallback;
    *pch = ((ch   & 0x0F) << 12) |
           ((s[1] & 0x3F) <<  6) |
            (s[2] & 0x3F);
    return 3;
  }
  {
    int count = 0; /* to count number of continuation bytes */
    unsigned res = 0;
    while ((ch & 0x40) != 0) { /* still have continuation bytes? */
      int cc = (unsigned char)s[++count];
      if ((cc & 0xC0) != 0x80) /* not a continuation byte? */
        goto fallback; /* invalid byte sequence, fallback */
      res = (res << 6) | (cc & 0x3F); /* add lower 6 bits from cont. byte */
      ch <<= 1; /* to test next bit */
    }
    if (count > 5)
      goto fallback; /* invalid byte sequence */
    res |= ((ch & 0x7F) << (count * 5)); /* add first byte */
    *pch = res;
    return count+1;
  }

fallback:
  *pch = ch;
  return 1;
}

void utf8_encode(uint32_t c, char s[4]) {
  if (c <= 0x7f) {
    s[0] = c;
  } else if (c <= 0x7ff) {
    s[0] = (0xc0 | ((c >> 6) & 0x1f));
    s[1] = (0x80 | (c & 0x3f));
  } else if (c <= 0xffff) {
    s[0] = (0xe0 | ((c >> 12) & 0x0f));
    s[1] = (0x80 | ((c >> 6) & 0x3f));
    s[2] = (0x80 | (c & 0x3f));
  } else if (c <= 0x10ffff) {
    s[1] = (0xf0 | ((c >> 18) & 0x07));
    s[1] = (0x80 | ((c >> 12) & 0x3f));
    s[2] = (0x80 | ((c >> 6) & 0x3f));
    s[3] = (0x80 | (c & 0x3f));
  }
}
