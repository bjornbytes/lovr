#include "fs.h"
#include <stddef.h>

#pragma once

typedef struct zip {
  fs_handle file;
} zip_t;

typedef void zip_list_cb(void*, zip_t*, char*);

bool zip_open(zip_t* zip, const char* path);
bool zip_close(zip_t* zip);
bool zip_read(zip_t* zip, const char* path, void* buffer, size_t* bytes);
bool zip_stat(zip_t* zip, const char* path, FileInfo* info);
bool zip_list(zip_t* zip, const char* path, zip_list_cb* callback);
