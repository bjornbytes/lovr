#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define MAX_ERROR_LENGTH 1024

char lovrErrorMessage[MAX_ERROR_LENGTH];
jmp_buf* lovrCatch = NULL;

void lovrThrow(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(lovrErrorMessage, MAX_ERROR_LENGTH, format, args);
  va_end(args);
  if (lovrCatch) {
    longjmp(*lovrCatch, 0);
  } else {
    fprintf(stderr, "Error: %s\n", lovrErrorMessage);
    exit(EXIT_FAILURE);
  }
}

void lovrSleep(double seconds) {
#ifdef _WIN32
  Sleep((unsigned int)(seconds * 1000));
#else
  usleep((unsigned int)(seconds * 1000000));
#endif
}

void* lovrAlloc(size_t size, void (*destructor)(const Ref* ref)) {
  void* object = malloc(size);
  if (!object) return NULL;
  *((Ref*) object) = (Ref) { destructor, 1 };
  return object;
}

void lovrRetain(const Ref* ref) {
  ((Ref*) ref)->count++;
}

void lovrRelease(const Ref* ref) {
  if (--((Ref*) ref)->count == 0 && ref->free) ref->free(ref);
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
