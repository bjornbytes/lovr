#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

// Error handling
static LOVR_THREAD_LOCAL errorFn* lovrErrorCallback;
static LOVR_THREAD_LOCAL void* lovrErrorUserdata;

void lovrSetErrorCallback(errorFn* callback, void* userdata) {
  lovrErrorCallback = callback;
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

// Dynamic Array
// Default malloc-based allocator for arr_t (like realloc except well-defined when size is 0)
void* arr_alloc(void* data, size_t size) {
  if (size > 0) {
    return realloc(data, size);
  } else {
    free(data);
    return NULL;
  }
}

// Hashmap
static uint32_t prevpo2(uint32_t x) {
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x - (x >> 1);
}

static void map_rehash(map_t* map) {
  map_t old = *map;
  map->size <<= 1;
  map->hashes = malloc(2 * map->size * sizeof(uint64_t));
  map->values = map->hashes + map->size;
  lovrAssert(map->size && map->hashes, "Out of memory");
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
    free(old.hashes);
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
  map->size = prevpo2(n) + !n;
  map->used = 0;
  map->hashes = NULL;
  map_rehash(map);
}

void map_free(map_t* map) {
  free(map->hashes);
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

void map_remove(map_t* map, uint64_t hash) {
  uint64_t h = map_find(map, hash);

  if (map->hashes[h] == MAP_NIL) {
    return;
  }

  uint64_t mask = map->size - 1;
  uint64_t i = h;

  do {
    i = (i + 1) & mask;
    uint64_t x = map->hashes[i] & mask;
    // Removing a key from an open-addressed hash table is complicated
    if ((i > h && (x <= h || x > i)) || (i < h && (x <= h && x > i))) {
      map->hashes[h] = map->hashes[i];
      map->values[h] = map->values[i];
      h = i;
    }
  } while (map->hashes[i] != MAP_NIL);

  map->hashes[i] = MAP_NIL;
  map->values[i] = MAP_NIL;
  map->used--;
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
