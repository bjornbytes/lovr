#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#pragma once

typedef enum {
  OPEN_READ,
  OPEN_WRITE,
  OPEN_APPEND
} OpenMode;

typedef enum {
  FILE_DIRECTORY,
  FILE_REGULAR
} FileType;

typedef struct {
  uint64_t size;
  uint64_t lastModified;
  FileType type;
} FileInfo;

typedef void fs_list_cb(void*, char*);
typedef struct { int handle; } fs_handle;

bool fs_open(const char* path, OpenMode mode, fs_handle* file);
bool fs_close(fs_handle file);
bool fs_read(fs_handle file, void* buffer, size_t* bytes);
bool fs_write(fs_handle file, const void* buffer, size_t* bytes);
bool fs_stat(const char* path, FileInfo* info);
bool fs_remove(const char* path);
bool fs_mkdir(const char* path);
bool fs_list(const char* path, fs_list_cb* callback, void* context);

// Returns length written to buffer
size_t fs_getHomeDir(char* buffer, size_t size);
size_t fs_getDataDir(char* buffer, size_t size);
size_t fs_getWorkDir(char* buffer, size_t size);
size_t fs_getExecutablePath(char* buffer, size_t size);
