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

int lovrFilesystemSetSource(const char* source) {
  return PHYSFS_mount(source, NULL, 0);
}
