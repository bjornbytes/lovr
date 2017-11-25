#include "filesystem/file.h"
#include <physfs.h>

File* lovrFileCreate(const char* path) {
  File* file = lovrAlloc(sizeof(File), lovrFileDestroy);
  if (!file) return NULL;

  file->path = path;
  file->handle = NULL;

  return file;
}

void lovrFileDestroy(const Ref* ref) {
  File* file = containerof(ref, File);
  if (file->handle) {
    PHYSFS_close(file->handle);
  }
  free(file);
}

int lovrFileOpen(File* file, FileMode mode) {
  lovrAssert(!file->handle, "File is already open");
  file->mode = mode;

  switch (mode) {
    case OPEN_READ: file->handle = PHYSFS_openRead(file->path); break;
    case OPEN_WRITE: file->handle = PHYSFS_openWrite(file->path); break;
    case OPEN_APPEND: file->handle = PHYSFS_openAppend(file->path); break;
  }

  return file->handle == NULL;
}

void lovrFileClose(File* file) {
  lovrAssert(file->handle, "File must be open to close it");
  PHYSFS_close(file->handle);
  file->handle = NULL;
}

size_t lovrFileRead(File* file, void* data, size_t bytes) {
  lovrAssert(file->handle && file->mode == OPEN_READ, "File must be open for reading");
  return PHYSFS_readBytes(file->handle, data, bytes);
}

size_t lovrFileWrite(File* file, void* data, size_t bytes) {
  lovrAssert(file->handle && (file->mode == OPEN_READ || file->mode == OPEN_WRITE), "File must be open for writing");
  return PHYSFS_writeBytes(file->handle, data, bytes);
}

size_t lovrFileGetSize(File* file) {
  lovrAssert(file->handle, "File must be open to get its size");
  return PHYSFS_fileLength(file->handle);
}

int lovrFileSeek(File* file, size_t position) {
  lovrAssert(file->handle, "File must be open to seek");
  return !PHYSFS_seek(file->handle, position);
}

size_t lovrFileTell(File* file) {
  lovrAssert(file->handle, "File must be open to tell");
  return PHYSFS_tell(file->handle);
}
