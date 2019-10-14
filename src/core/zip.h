#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#pragma once

typedef struct {
  uint64_t offset;
} zip_entry;

typedef struct {
  bool (*read)(void* context, void* buffer, size_t* bytes);
  bool (*seek)(void* context, int64_t offset, int origin);
} zip_io;

typedef struct {
  void* ctx;
  zip_io io;
} zip_t;

bool zip_open(zip_t* zip, zip_io io, void* context);
bool zip_next(zip_t* zip, zip_entry* entry);
bool zip_read(zip_t* zip, zip_entry* entry);
