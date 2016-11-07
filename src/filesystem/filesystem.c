#include "filesystem.h"
#include "../util.h"
#include <physfs.h>
#include <stdio.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static FilesystemState state;

void lovrFilesystemInit(const char* arg0) {
  if (!PHYSFS_init(arg0)) {
    error("Could not initialize filesystem: %s", PHYSFS_getLastError());
  }

  state.gameSource = NULL;
  state.identity = NULL;
}

void lovrFilesystemDestroy() {
  PHYSFS_deinit();
}

int lovrFilesystemExists(const char* path) {
  return PHYSFS_exists(path);
}

int lovrFilesystemGetExecutablePath(char* dest, unsigned int size) {
#ifdef __APPLE__
  if (_NSGetExecutablePath(dest, &size) == 0) {
    return 0;
  }
#endif

  return 1;
}

const char* lovrFilesystemGetIdentity() {
  return state.identity;
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  if (!PHYSFS_isInit()) {
    return NULL;
  }

  return PHYSFS_getRealDir(path);
}

const char* lovrFilesystemGetSource() {
  return state.gameSource;
}

const char* lovrFilesystemGetUserDirectory() {
  if (!PHYSFS_isInit()) {
    return NULL;
  }

  return PHYSFS_getUserDir();
}

int lovrFilesystemIsDirectory(const char* path) {
  return PHYSFS_isDirectory(path);
}

int lovrFilesystemIsFile(const char* path) {
  return lovrFilesystemExists(path) && !lovrFilesystemIsDirectory(path);
}

void* lovrFilesystemRead(const char* path, int* bytesRead) {
  if (!PHYSFS_isInit()) {
    return NULL;
  }

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

int lovrFilesystemSetIdentity(const char* identity) {
  state.identity = identity;

  // Unmount old write directory
  if (state.savePathFull && state.savePathRelative) {
    PHYSFS_removeFromSearchPath(state.savePathRelative);
  } else {
    state.savePathRelative = malloc(LOVR_PATH_MAX);
    state.savePathFull = malloc(LOVR_PATH_MAX);
  }

  // Set new write directory
#ifdef __APPLE__
  const char* userDir = PHYSFS_getUserDir();
  PHYSFS_setWriteDir(userDir);

  snprintf(state.savePathRelative, LOVR_PATH_MAX, "Library/Application Support/LOVR/%s", identity);
  PHYSFS_mkdir(state.savePathRelative);

  snprintf(state.savePathFull, LOVR_PATH_MAX, "%s%s", userDir, state.savePathRelative);
  if (PHYSFS_setWriteDir(state.savePathFull)) {
    PHYSFS_mount(state.savePathFull, NULL, 0);
    return 0;
  }
#endif

  return 1;
}

int lovrFilesystemSetSource(const char* source) {
  if (state.gameSource) {
    return 1;
  }

  if (PHYSFS_mount(source, NULL, 0)) {
    state.gameSource = source;
    return 0;
  }

  return 1;
}
