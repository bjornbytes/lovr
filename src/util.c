#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <stdio.h>

// Allocation

void* lovrMalloc(size_t size) {
  void* data = malloc(size);
  if (!data) abort();
  lovrProfileAlloc(data, size);
  return data;
}

void* lovrCalloc(size_t size) {
  void* data = calloc(1, size);
  if (!data) abort();
  lovrProfileAlloc(data, size);
  return data;
}

void* lovrRealloc(void* old, size_t size) {
  lovrProfileFree(old);
  void* data = realloc(old, size);
  if (!data) abort();
  lovrProfileAlloc(data, size);
  return data;
}

void lovrFree(void* data) {
  lovrProfileFree(data);
  free(data);
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

// Errors

static thread_local char error[1024];

const char* lovrGetError(void) {
  return error;
}

int lovrSetError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(error, sizeof(error), format, args);
  va_end(args);
  return false;
}

// Logging

static fn_log* lovrLogCallback;
static void* lovrLogUserdata;

void lovrSetLogCallback(fn_log* callback, void* userdata) {
  lovrLogCallback = callback;
  lovrLogUserdata = userdata;
}

void lovrLog(int level, const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  lovrLogCallback(lovrLogUserdata, level, tag, format, args);
  va_end(args);
}

// Hashmap

static void map_rehash(map_t* map) {
  map_t old = *map;
  map->size <<= 1;
  if (map->size == 0) abort();
  map->hashes = lovrMalloc(2 * map->size * sizeof(uint64_t));
  map->values = map->hashes + map->size;
  memset(map->hashes, 0xff, 2 * map->size * sizeof(uint64_t));

  if (old.hashes) {
    uint64_t mask = map->size - 1;
    for (uint32_t i = 0; i < old.size; i++) {
      if (old.hashes[i] != MAP_NIL) {
        uint64_t index = old.hashes[i] & mask;
        while (map->hashes[index] != MAP_NIL) {
          index = (index + 1) & mask;
        }
        map->hashes[index] = old.hashes[i];
        map->values[index] = old.values[i];
      }
    }
    lovrFree(old.hashes);
  }
}

static inline uint64_t map_find(map_t* map, uint64_t hash) {
  uint64_t mask = map->size - 1;
  uint64_t h = hash & mask;

  while (map->hashes[h] != hash && map->hashes[h] != MAP_NIL) {
    h = (h + 1) & mask;
  }

  return h;
}

void map_init(map_t* map, uint32_t n) {
  map->size = 1;
  while (map->size + (map->size >> 1) < n) {
    map->size <<= 1;
  }
  map->used = 0;
  map->hashes = NULL;
  map_rehash(map);
}

void map_free(map_t* map) {
  if (map) {
    lovrFree(map->hashes);
    map->hashes = NULL;
  }
}

uint64_t map_get(map_t* map, uint64_t hash) {
  return map->values[map_find(map, hash)];
}

void map_set(map_t* map, uint64_t hash, uint64_t value) {
  if (map->used >= (map->size >> 1) + (map->size >> 2)) {
    map_rehash(map);
  }

  uint64_t h = map_find(map, hash);
  map->used += map->hashes[h] == MAP_NIL;
  map->hashes[h] = hash;
  map->values[h] = value;
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

// f16
// http://fox-toolkit.org/ftp/fasthalffloatconversion.pdf

static bool float16Initialized = false;

// f32 to f16 tables
static uint16_t base[512];
static uint8_t shift[512];

// f16 to f32 tables
static uint32_t mantissa[2048];
static uint32_t exponent[64];
static uint16_t offset[64];

void float16Init(void) {
  if (float16Initialized) return;
  float16Initialized = true;

  for (uint32_t i = 0; i < 256; i++) {
    int e = i - 127;
    if (e < -24) {
      base[i | 0x000] = 0x0000;
      base[i | 0x100] = 0x8000;
      shift[i | 0x000] = 24;
      shift[i | 0x100] = 24;
    } else if (e < -14) {
      base[i | 0x000] = (0x0400 >> (-e - 14));
      base[i | 0x100] = (0x0400 >> (-e - 14)) | 0x8000;
      shift[i | 0x000] = -e - 1;
      shift[i | 0x100] = -e - 1;
    } else if (e <= 15) {
      base[i | 0x000] = ((e + 15) << 10);
      base[i | 0x100] = ((e + 15) << 10) | 0x8000;
      shift[i | 0x000] = 13;
      shift[i | 0x100] = 13;
    } else if (e < 128) {
      base[i | 0x000] = 0x7C00;
      base[i | 0x100] = 0xFC00;
      shift[i | 0x000] = 24;
      shift[i | 0x100] = 24;
    } else {
      base[i | 0x000] = 0x7C00;
      base[i | 0x100] = 0xFC00;
      shift[i | 0x000] = 13;
      shift[i | 0x100] = 13;
    }
  }

  for (uint32_t i = 0; i < 2048; i++) {
    if (i == 0) {
      mantissa[i] = 0;
    } else if (i < 1024) {
      uint32_t m = i << 13;
      uint32_t e = 0;
      while ((m & 0x00800000) == 0) {
        e -= 0x00800000;
        m <<= 1;
      }
      e += 0x38800000;
      m &= ~0x00800000;
      mantissa[i] = e | m;
    } else {
      mantissa[i] = 0x38000000 + ((i - 1024) << 13);
    }
  }

  for (uint32_t i = 0; i < 64; i++) {
    if (i == 0) exponent[i] = 0;
    else if (i < 31) exponent[i] = i << 23;
    else if (i == 31) exponent[i] = 0x47800000;
    else if (i == 32) exponent[i] = 0x80000000;
    else if (i < 63) exponent[i] = 0x80000000 + ((i - 32) << 23);
    else exponent[i] = 0xC7800000;
  }

  for (uint32_t i = 0; i < 64; i++) {
    offset[i] = (i == 0 || i == 32) ? 0 : 1024;
  }
}

float16 float32to16(float32 f) {
  uint32_t u = ((union { float f; uint32_t u; }) { f }).u;
  return base[(u >> 23) & 0x1ff] + ((u & 0x7fffff) >> shift[(u >> 23) & 0x1ff]);
}

float32 float16to32(float16 f) {
  uint32_t u = mantissa[offset[f >> 10] + (f & 0x3ff)] + exponent[f >> 10];
  return ((union { uint32_t u; float f; }) { u }).f;
}
