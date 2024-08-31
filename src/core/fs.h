#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

typedef enum {
  FS_OK,
  FS_UNKNOWN_ERROR,
  FS_PERMISSION,
  FS_READ_ONLY,
  FS_TOO_LONG,
  FS_NOT_FOUND,
  FS_EXISTS,
  FS_IS_DIR,
  FS_NOT_DIR,
  FS_NOT_EMPTY,
  FS_LOOP,
  FS_FULL,
  FS_BUSY,
  FS_IO
} fs_error;

typedef enum {
  FILE_DIRECTORY,
  FILE_REGULAR
} fs_type;

typedef struct {
  uint64_t size;
  uint64_t lastModified;
  fs_type type;
} fs_info;

typedef void fs_list_cb(void*, const char*);
typedef union { int fd; void* handle; } fs_handle;

fs_error fs_open(const char* path, char mode, fs_handle* file);
fs_error fs_close(fs_handle file);
fs_error fs_read(fs_handle file, void* data, size_t size, size_t* count);
fs_error fs_write(fs_handle file, const void* data, size_t size, size_t* count);
fs_error fs_seek(fs_handle file, uint64_t offset);
fs_error fs_fstat(fs_handle file, fs_info* info);
fs_error fs_map(const char* path, void** pointer, size_t* size);
fs_error fs_unmap(void* data, size_t size);
fs_error fs_stat(const char* path, fs_info* info);
fs_error fs_remove(const char* path);
fs_error fs_mkdir(const char* path);
fs_error fs_list(const char* path, fs_list_cb* callback, void* context);
