#include "filesystem/file.h"
#include "filesystem/filesystem.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>

// Currently only read operations are supported by File and all files are read into memory fully on open

typedef struct {
  uint8_t* data;
  size_t offset;
  size_t size;
} FileInner;

File* lovrFileInit(File* file ,const char* path) {
  file->path = path;
  file->handle = NULL;
  file->mode = 0;
  return file;
}

void lovrFileDestroy(void* ref) {
  File* file = ref;
  if (file->handle) {
    lovrFileClose(file);
  }
  free(file);
}

bool lovrFileOpen(File* file, FileMode mode) {
  lovrAssert(!file->handle, "File is already open");
  file->mode = mode;

  if (mode == OPEN_WRITE || mode == OPEN_APPEND)
    return false;

  FileInner *fileInner = malloc(sizeof(FileInner));
  fileInner->offset = 0;
  fileInner->data = lovrFilesystemRead(file->path, -1, &fileInner->size);
  file->handle = fileInner;

  if (!fileInner->data) {
    fileInner->size = 0;
    return false;
  }

  return true;
}

void lovrFileClose(File* file) {
  lovrAssert(file->handle, "File must be open to close it");
  FileInner *fileInner = (FileInner *)file->handle;
  free(fileInner->data);
  free(file->handle);
  file->handle = NULL;
}

size_t lovrFileRead(File* file, void* data, size_t bytes) {
  lovrAssert(file->handle && file->mode == OPEN_READ, "File must be open for reading");
  FileInner *fileInner = (FileInner *)file->handle;
  if (fileInner->offset + bytes > fileInner->size)
    return 0;
  memcpy(data, fileInner->data + fileInner->offset, bytes);
  fileInner->offset += bytes;
  return bytes;
}

size_t lovrFileWrite(File* file, const void* data, size_t bytes) {
  lovrThrow("Writing not supported");
}

size_t lovrFileGetSize(File* file) {
  lovrAssert(file->handle, "File must be open to get its size");
  FileInner *fileInner = (FileInner *)file->handle;
  return fileInner->size;
}

bool lovrFileSeek(File* file, size_t position) {
  lovrAssert(file->handle, "File must be open to seek");
  FileInner *fileInner = (FileInner *)file->handle;
  if (position >= fileInner->size) // FIXME: Should seeking to fileInner->size exactly be allowed?
    return false;
  fileInner->offset = position;
  return true;
}

size_t lovrFileTell(File* file) {
  lovrAssert(file->handle, "File must be open to tell");
  FileInner *fileInner = (FileInner *)file->handle;
  return fileInner->offset;
}
