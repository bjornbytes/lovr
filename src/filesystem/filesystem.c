#include "filesystem.h"
#include "../util.h"
#include <physfs.h>
#ifdef APPLE
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
#ifdef APPLE
  char path[1024];
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

char* lovrFilesystemRead(const char* path, int* size) {
  PHYSFS_file* handle = PHYSFS_openRead(path);

  if (!handle) {
    return NULL;
  }

  int length = PHYSFS_fileLength(handle);

  if (length < 0) {
    return NULL;
  }

  char* data = malloc(length * sizeof(char));
  *size = PHYSFS_read(handle, data, sizeof(char), length);
  return data;
}

int lovrFilesystemSetSource(const char* source) {
  int res = PHYSFS_mount(source, NULL, 0);
  return res;
}
