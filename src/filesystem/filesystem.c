#include "filesystem.h"
#include "../util.h"
#include <physfs.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

void lovrFilesystemInit(const char* arg0) {
  if (!PHYSFS_init(arg0)) {
    error("Could not initialize filesystem: %s", PHYSFS_getLastError());
  }
}

void lovrFilesystemDestroy() {
  PHYSFS_deinit();
}

int lovrFilesystemExists(const char* path) {
  return PHYSFS_exists(path);
}

const char* lovrFilesystemGetExecutablePath() {
#ifdef __APPLE__
  char* path = malloc(1024);
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    return path;
  }
#endif

  return NULL;
}

int lovrFilesystemIsDirectory(const char* path) {
  return PHYSFS_isDirectory(path);
}

int lovrFilesystemIsFile(const char* path) {
  return lovrFilesystemExists(path) && !lovrFilesystemIsDirectory(path);
}

void* lovrFilesystemRead(const char* path, int* bytesRead) {

  // Open file
  PHYSFS_file* handle = PHYSFS_openRead(path);
  if (!handle) {
    return NULL;
  }

  // Get file size
  int size = PHYSFS_fileLength(handle);
  if (size < 0) {
    return NULL;
  }

  // Allocate buffer
  void* data = malloc(size);
  if (!data) {
    return NULL;
  }

  // Perform read
  *bytesRead = PHYSFS_read(handle, data, 1, size);
  PHYSFS_close(handle);

  // Make sure we got everything
  if (*bytesRead != size) {
    free(data);
    return NULL;
  }

  return data;
}

int lovrFilesystemSetSource(const char* source) {
  return PHYSFS_mount(source, NULL, 0);
}
