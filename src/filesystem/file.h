#include "util.h"

#pragma once

typedef enum {
  OPEN_READ,
  OPEN_WRITE,
  OPEN_APPEND
} FileMode;

typedef struct {
  Ref ref;
  const char* path;
  void* handle;
  FileMode mode;
} File;

File* lovrFileCreate(const char* filename);
void lovrFileDestroy(void* ref);
int lovrFileOpen(File* file, FileMode mode);
void lovrFileClose(File* file);
size_t lovrFileRead(File* file, void* data, size_t bytes);
size_t lovrFileWrite(File* file, const void* data, size_t bytes);
size_t lovrFileGetSize(File* file);
int lovrFileSeek(File* file, size_t position);
size_t lovrFileTell(File* file);
