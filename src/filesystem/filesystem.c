#include "filesystem/filesystem.h"
#include "filesystem/file.h"
#include "util.h"
#include <physfs.h>
#include <stdio.h>
#include <stdlib.h>
#include "lovr.h"
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

bool filesystemAlreadyInit = false;

void lovrFilesystemInit(const char* arg0, const char* arg1) {
  if (filesystemAlreadyInit) // Do not change settings during a reload, and don't try to initialize PhysFS twice
    return;
  filesystemAlreadyInit = true;

  if (!PHYSFS_init(arg0)) {
    lovrThrow("Could not initialize filesystem: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
  }

  state.source = malloc(LOVR_PATH_MAX * sizeof(char));
  state.identity = NULL;
  state.isFused = true;

  // Try to mount either an archive fused to the executable or an archive from the command line
  lovrFilesystemGetExecutablePath(state.source, LOVR_PATH_MAX);
  if (lovrFilesystemMount(state.source, NULL, 1)) {
    state.isFused = false;

    if (arg1) {
      strncpy(state.source, arg1, LOVR_PATH_MAX);
      if (!lovrFilesystemMount(state.source, NULL, 1)) {
        goto mounted;
      }
    }

    free(state.source);
    state.source = NULL;
  }

mounted:
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
  PHYSFS_utf8FromUtf16(appData, dest, size);
  CoTaskMemFree(appData);
  return 0;
#elif EMSCRIPTEN
  strncpy(dest, "/home/web_user", size);
  return 0;
#elif __linux__
  const char* home;
  if ((home = getenv("HOME")) == NULL) {
    home = getpwuid(getuid())->pw_dir;
  }

  snprintf(dest, size, "%s/.config", home);
#else
#error "This platform is missing an implementation for lovrFilesystemGetAppdataDirectory"
#endif

  return 1;
}

void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata) {
  PHYSFS_enumerate(path, callback, userdata);
}

int lovrFilesystemGetExecutablePath(char* dest, unsigned int size) {
#ifdef __APPLE__
  if (_NSGetExecutablePath(dest, &size) == 0) {
    return 0;
  }
#elif _WIN32
  return !GetModuleFileName(NULL, dest, size);
#elif EMSCRIPTEN
  return 1;
#elif __linux__
  memset(dest, 0, size);
  if (readlink("/proc/self/exe", dest, size) == -1) {
    return 1;
  }
#else
#error "This platform is missing an implementation for lovrFilesystemGetExecutablePath"
#endif

  return 1;
}

const char* lovrFilesystemGetIdentity() {
  return state.identity;
}

long lovrFilesystemGetLastModified(const char* path) {
  PHYSFS_Stat stat;
  return PHYSFS_stat(path, &stat) ? stat.modtime : -1;
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  return PHYSFS_getRealDir(path);
}

const char* lovrFilesystemGetSaveDirectory() {
  return state.savePathFull;
}

size_t lovrFilesystemGetSize(const char* path) {
  PHYSFS_Stat stat;
  return PHYSFS_stat(path, &stat) ? stat.filesize : -1;
}

const char* lovrFilesystemGetSource() {
  return state.source;
}

const char* lovrFilesystemGetUserDirectory() {
#if defined(__APPLE__) || defined(__linux__)
  const char* home;
  if ((home = getenv("HOME")) == NULL) {
    home = getpwuid(getuid())->pw_dir;
  }
  return home;
#elif _WIN32
  return getenv("USERPROFILE");
#elif EMSCRIPTEN
  return "/home/web_user";
#else
#error "This platform is missing an implementation for lovrFilesystemGetUserDirectory"
#endif
}

bool lovrFilesystemIsDirectory(const char* path) {
  PHYSFS_Stat stat;
  return PHYSFS_stat(path, &stat) ? stat.filetype == PHYSFS_FILETYPE_DIRECTORY : false;
}

bool lovrFilesystemIsFile(const char* path) {
  PHYSFS_Stat stat;
  return PHYSFS_stat(path, &stat) ? stat.filetype == PHYSFS_FILETYPE_REGULAR : false;
}

bool lovrFilesystemIsFused() {
  return state.isFused;
}

int lovrFilesystemMount(const char* path, const char* mountpoint, bool append) {
  return !PHYSFS_mount(path, mountpoint, append);
}

void* lovrFilesystemRead(const char* path, size_t* bytesRead) {

  // Create file
  File* file = lovrFileCreate(path);
  if (!file) {
    return NULL;
  }

  // Open it
  if (lovrFileOpen(file, OPEN_READ)) {
    return NULL;
  }

  // Get file size
  size_t size = lovrFileGetSize(file);
  if (size == (unsigned int) -1) {
    return NULL;
  }

  // Allocate buffer
  void* data = malloc(size);
  if (!data) {
    return NULL;
  }

  // Perform read
  *bytesRead = lovrFileRead(file, data, size);
  lovrFileClose(file);

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
    PHYSFS_unmount(state.savePathRelative);
  } else {
    state.savePathRelative = malloc(LOVR_PATH_MAX);
    state.savePathFull = malloc(LOVR_PATH_MAX);
    if (!state.savePathRelative || !state.savePathFull) {
      return 1;
    }
  }

  lovrFilesystemGetAppdataDirectory(state.savePathFull, LOVR_PATH_MAX);
  if (!PHYSFS_setWriteDir(state.savePathFull)) {
    const char* error = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
    lovrThrow("Could not set write directory: %s (%s)", error, state.savePathFull);
  }

  snprintf(state.savePathRelative, LOVR_PATH_MAX, "LOVR/%s", identity ? identity : "default");
  char fullPathBuffer[LOVR_PATH_MAX];
  snprintf(fullPathBuffer, LOVR_PATH_MAX, "%s/%s", state.savePathFull, state.savePathRelative);
  strncpy(state.savePathFull, fullPathBuffer, LOVR_PATH_MAX);
  PHYSFS_mkdir(state.savePathRelative);
  if (!PHYSFS_setWriteDir(state.savePathFull)) {
    const char* error = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
    lovrThrow("Could not set write directory: %s (%s)", error, state.savePathRelative);
  }

  PHYSFS_mount(state.savePathFull, NULL, 0);

  return 0;
}

int lovrFilesystemUnmount(const char* path) {
  return !PHYSFS_unmount(path);
}

size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  File* file = lovrFileCreate(path);
  if (!file) {
    return 0;
  }

  lovrFileOpen(file, append ? OPEN_APPEND : OPEN_WRITE);
  size_t bytesWritten = lovrFileWrite(file, (void*) content, size);
  lovrFileClose(file);
  lovrRelease(&file->ref);
  return bytesWritten;
}
