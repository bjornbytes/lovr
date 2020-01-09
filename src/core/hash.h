#include "util.h"
#include <stdint.h>

#pragma once

// FNV1a
static LOVR_INLINE uint64_t hash64(const void* data, size_t length) {
  const uint8_t* bytes = (uint8_t *)data;
  uint64_t hash = 0xcbf29ce484222325;
  for (size_t i = 0; i < length; i++) {
    hash = (hash ^ bytes[i]) * 0x100000001b3;
  }
  return hash;
}
