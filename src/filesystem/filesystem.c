#include "filesystem/filesystem.h"
#include "util.h"
#include <physfs.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#if _WIN32
#include <windows.h>
#include <initguid.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

static FilesystemState state;

void lovrFilesystemInit(const char* arg0, const char* arg1) {
  if (!PHYSFS_init(arg0)) {
    error("Could not initialize filesystem: %s", PHYSFS_getLastError());
  }

  state.source = malloc(LOVR_PATH_MAX * sizeof(char));
  state.identity = NULL;
  state.isFused = 1;

  // Try to mount either an archive fused to the executable or an archive from the command line
  lovrFilesystemGetExecutablePath(state.source, LOVR_PATH_MAX);
  if (lovrFilesystemMount(state.source, NULL, 1)) {
    state.isFused = 0;
    strncpy(state.source, arg1, LOVR_PATH_MAX);
    if (!state.source || lovrFilesystemMount(state.source, NULL, 1)) {
      free(state.source);
      state.source = NULL;
    }
  }

  atexit(lovrFilesystemDestroy);
}

void lovrFilesystemDestroy() {
  free(state.source);
  free(state.savePathFull);
  free(state.savePathRelative);
  PHYSFS_deinit();
}

int lovrFilesystemCreateDirectory(const char* path) {
  return !PHYSFS_mkdir(path);
}

int lovrFilesystemExists(const char* path) {
  return PHYSFS_exists(path);
}

int lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size) {
#ifdef __APPLE__
  const char* home;
  if ((home = getenv("HOME")) == NULL) {
    home = getpwuid(getuid())->pw_dir;
  }

  snprintf(dest, size, "%s/Library/Application Support", home);
  return 0;
#elif _WIN32
  PWSTR appData = NULL;
  SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &appData);
  wcstombs(dest, appData, size);
  CoTaskMemFree(appData);
  return 0;
#else
#error "This platform is missing an implementation for lovrFilesystemGetAppdataDirectory"
#endif

  return 1;
}

void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata) {
  PHYSFS_enumerateFilesCallback(path, callback, userdata);
}

int lovrFilesystemGetExecutablePath(char* dest, unsigned int size) {
#ifdef __APPLE__
  if (_NSGetExecutablePath(dest, &size) == 0) {
    return 0;
  }
#elif _WIN32
  return !GetModuleFileName(NULL, dest, size);
#else
#error "This platform is missing an implementation for lovrFilesystemGetExecutablePath"
#endif

  return 1;
}

const char* lovrFilesystemGetIdentity() {
  return state.identity;
}

long lovrFilesystemGetLastModified(const char* path) {
  return PHYSFS_getLastModTime(path);
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  return PHYSFS_getRealDir(path);
}

const char* lovrFilesystemGetSaveDirectory() {
  return state.savePathFull;
}

int lovrFilesystemGetSize(const char* path) {
  PHYSFS_file* handle = PHYSFS_openRead(path);
  if (!handle) {
    return 1;
  }

  int length = PHYSFS_fileLength(handle);
  PHYSFS_close(handle);
  return length;
}

const char* lovrFilesystemGetSource() {
  return state.source;
}

const char* lovrFilesystemGetUserDirectory() {
  return PHYSFS_getUserDir();
}

int lovrFilesystemIsDirectory(const char* path) {
  return PHYSFS_isDirectory(path);
}

int lovrFilesystemIsFile(const char* path) {
  return lovrFilesystemExists(path) && !lovrFilesystemIsDirectory(path);
}

int lovrFilesystemIsFused() {
  return state.isFused;
}

int lovrFilesystemMount(const char* path, const char* mountpoint, int append) {
  return !PHYSFS_mount(path, mountpoint, append);
}

void* lovrFilesystemRead(const char* path, size_t* bytesRead) {

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
  if (*bytesRead != (size_t) size) {
    free(data);
    return NULL;
  }

  return data;
}

int lovrFilesystemRemove(const char* path) {
  return !PHYSFS_delete(path);
}

int lovrFilesystemSetIdentity(const char* identity) {
  state.identity = identity;

  // Unmount old write directory
  if (state.savePathFull && state.savePathRelative) {
    PHYSFS_removeFromSearchPath(state.savePathRelative);
  } else {
    state.savePathRelative = malloc(LOVR_PATH_MAX);
    state.savePathFull = malloc(LOVR_PATH_MAX);
    if (!state.savePathRelative || !state.savePathFull) {
      return 1;
    }
  }

  const char* sep = PHYSFS_getDirSeparator();

  lovrFilesystemGetAppdataDirectory(state.savePathFull, LOVR_PATH_MAX);
  PHYSFS_setWriteDir(state.savePathFull);
  snprintf(state.savePathRelative, LOVR_PATH_MAX, "LOVR%s%s", sep, identity);
  snprintf(state.savePathFull, LOVR_PATH_MAX, "%s%s%s", state.savePathFull, sep, state.savePathRelative);
  PHYSFS_mkdir(state.savePathRelative);
  PHYSFS_setWriteDir(state.savePathRelative);
  PHYSFS_mount(state.savePathFull, NULL, 0);

  return 0;
}

int lovrFilesystemUnmount(const char* path) {
  return !PHYSFS_removeFromSearchPath(path);
}

int lovrFilesystemWrite(const char* path, const char* content, size_t size, int append) {
  PHYSFS_file* handle = append ? PHYSFS_openAppend(path) : PHYSFS_openWrite(path);
  if (!handle) {
    return 0;
  }

  int bytesWritten = PHYSFS_write(handle, content, 1, size);
  PHYSFS_close(handle);
  return bytesWritten;
}
