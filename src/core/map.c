#include "map.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

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
