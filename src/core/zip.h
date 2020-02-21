#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Status:
//  - Little endian only
//  - Zip64 is not supported
//  - Self-extracting archives are supported
//  - Supports store and deflate compression
//  - No comment allowed at the end of archive (file comments are okay)
//  - No multi-disk archives
//  - No encryption

#pragma once

typedef struct {
  uint8_t* data;
  size_t size;
  size_t base;
  size_t cursor;
  uint64_t count;
} zip_state;

typedef struct {
  uint64_t offset;
  uint64_t csize;
  uint64_t size;
  const char* name;
  uint16_t length;
  uint16_t mdate;
  uint16_t mtime;
} zip_file;

bool zip_open(zip_state* zip);
bool zip_next(zip_state* zip, zip_file* info);
void* zip_load(zip_state* zip, size_t offset, bool* compressed);
