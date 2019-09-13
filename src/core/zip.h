#include "fs.h"
#include <stddef.h>

#pragma once

typedef struct zip {
  fs_handle file;
} zip;

typedef void zip_list_cb(void*, zip*, char*);

bool zip_open(zip* self, const char* path);
bool zip_close(zip* self);
bool zip_read(zip* self, const char* path, void* buffer, size_t* bytes);
bool zip_stat(zip* self, const char* path, FileInfo* info);
bool zip_list(zip* self, const char* path, zip_list_cb* callback);
