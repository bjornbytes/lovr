#include "util.h"
#include "platform.h"
#include <stdlib.h>

_Thread_local lovrErrorHandler lovrErrorCallback = NULL;
_Thread_local void* lovrErrorUserdata = NULL;

void lovrSetErrorCallback(lovrErrorHandler callback, void* userdata) {
  lovrErrorCallback = callback;
  lovrErrorUserdata = userdata;
}

void lovrThrow(const char* format, ...) {
  if (lovrErrorCallback) {
    va_list args;
    va_start(args, format);
    lovrErrorCallback(lovrErrorUserdata, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
  } else {
    va_list args;
    va_start(args, format);
    lovrWarn("Error: ");
    lovrWarnv(format, args);
    lovrWarn("\n");
    va_end(args);
    exit(EXIT_FAILURE);
  }
}

void* _lovrAlloc(const char* type, size_t size, void (*destructor)(void*)) {
  Ref* ref = calloc(1, size);
  if (!ref) return lovrThrow("Out of memory"), NULL;
  ref->free = destructor;
  ref->type = type;
  ref->count = 1;
  return ref;
}

void lovrRetain(void* object) {
  if (object) ((Ref*) object)->count++;
}

void lovrRelease(void* object) {
  Ref* ref = object;
  if (ref && --ref->count == 0) ref->free(object);
}

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

uint32_t nextPo2(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
}
