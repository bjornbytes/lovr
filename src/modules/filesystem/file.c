#include "filesystem/file.h"
#include "util.h"
#include <physfs.h>

File* lovrFileInit(File* file ,const char* path) {
  file->path = path;
  file->handle = NULL;
  file->mode = 0;
  return file;
}

void lovrFileDestroy(void* ref) {
  File* file = ref;
  if (file->handle) {
    PHYSFS_close(file->handle);
  }
}

bool lovrFileOpen(File* file, FileMode mode) {
  lovrAssert(!file->handle, "File is already open");
  file->mode = mode;

  switch (mode) {
    case OPEN_READ: file->handle = PHYSFS_openRead(file->path); break;
    case OPEN_WRITE: file->handle = PHYSFS_openWrite(file->path); break;
    case OPEN_APPEND: file->handle = PHYSFS_openAppend(file->path); break;
  }

  return file->handle != NULL;
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

size_t lovrFileWrite(File* file, const void* data, size_t bytes) {
  lovrAssert(file->handle && (file->mode == OPEN_WRITE || file->mode == OPEN_APPEND), "File must be open for writing");
  return PHYSFS_writeBytes(file->handle, data, bytes);
}

size_t lovrFileGetSize(File* file) {
  lovrAssert(file->handle, "File must be open to get its size");
  return PHYSFS_fileLength(file->handle);
}

bool lovrFileSeek(File* file, size_t position) {
  lovrAssert(file->handle, "File must be open to seek");
  return PHYSFS_seek(file->handle, position);
}

size_t lovrFileTell(File* file) {
  lovrAssert(file->handle, "File must be open to tell");
  return PHYSFS_tell(file->handle);
}
