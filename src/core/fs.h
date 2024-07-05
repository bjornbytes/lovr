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
} FileError;

typedef enum {
  FILE_DIRECTORY,
  FILE_REGULAR
} FileType;

typedef struct {
  uint64_t size;
  uint64_t lastModified;
  FileType type;
} FileInfo;

typedef void fs_list_cb(void*, const char*);
typedef union { int fd; void* handle; } fs_handle;

int fs_open(const char* path, char mode, fs_handle* file);
int fs_close(fs_handle file);
int fs_read(fs_handle file, void* data, size_t size, size_t* count);
int fs_write(fs_handle file, const void* data, size_t size, size_t* count);
int fs_seek(fs_handle file, uint64_t offset);
int fs_fstat(fs_handle file, FileInfo* info);
int fs_map(const char* path, void** pointer, size_t* size);
int fs_unmap(void* data, size_t size);
int fs_stat(const char* path, FileInfo* info);
int fs_remove(const char* path);
int fs_mkdir(const char* path);
int fs_list(const char* path, fs_list_cb* callback, void* context);
