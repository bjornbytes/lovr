#include "filesystem/filesystem.h"
#include "filesystem/file.h"
#include "platform.h"
#include "util.h"
#include <physfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#if LOVR_USE_OCULUS_MOBILE
#include "headset/oculus_mobile.h"
#endif

#ifdef _WIN32
const char lovrDirSep = '\\';
#else
const char lovrDirSep = '/';
#endif

static FilesystemState state;

void lovrFilesystemInit(const char* argExe, const char* argGame, const char* argGameInner) {
  if (state.initialized) return;
  state.initialized = true;

  if (!PHYSFS_init(argExe)) {
    lovrThrow("Could not initialize filesystem: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
  }

  state.source = malloc(LOVR_PATH_MAX * sizeof(char));
  state.identity = NULL;
  state.isFused = true;
  vec_init(&state.requirePattern[0]);
  vec_init(&state.requirePattern[1]);
  lovrFilesystemSetRequirePath("?.lua;?/init.lua;lua_modules/?.lua;lua_modules/?/init.lua;deps/?.lua;deps/?/init.lua");
  lovrFilesystemSetCRequirePath("??;lua_modules/??;deps/??");

  // Try to mount either an archive fused to the executable or an archive from the command line
  lovrFilesystemGetExecutablePath(state.source, LOVR_PATH_MAX);
  if (lovrFilesystemMount(state.source, NULL, 1, argGameInner)) { // Attempt to load fused. If that fails...
    state.isFused = false;

    if (argGame) {
      strncpy(state.source, argGame, LOVR_PATH_MAX);
      if (!lovrFilesystemMount(state.source, NULL, 1, argGameInner)) { // Attempt to load from arg. If success, init is done
        return;
      }
    }

    free(state.source); // Couldn't load from argProject, so apparently it isn't the source
    state.source = NULL;
  }
}

void lovrFilesystemDestroy() {
  if (!state.initialized) return;
  free(state.source);
  free(state.savePathFull);
  free(state.savePathRelative);
  for (int i = 0; i < 2; i++) {
    free(state.requirePath[i]);
    vec_deinit(&state.requirePattern[i]);
  }
  PHYSFS_deinit();
  memset(&state, 0, sizeof(FilesystemState));
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
#elif LOVR_USE_OCULUS_MOBILE
  strncpy(dest, lovrOculusMobileWritablePath, size);
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

int lovrFilesystemGetExecutablePath(char* path, uint32_t size) {
  return lovrGetExecutablePath(path, size);
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

vec_str_t* lovrFilesystemGetRequirePath() {
  return &state.requirePattern[0];
}

vec_str_t* lovrFilesystemGetCRequirePath() {
  return &state.requirePattern[1];
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

int lovrFilesystemGetWorkingDirectory(char* dest, unsigned int size) {
#ifdef _WIN32
  WCHAR w_cwd[LOVR_PATH_MAX];
  _wgetcwd(w_cwd, LOVR_PATH_MAX);
  PHYSFS_utf8FromUtf16(w_cwd, dest, size);
  return 0;
#else
  if (getcwd(dest, size)) {
    return 0;
  }
#endif
  return 1;
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

// Returns zero on success, nonzero on failure
int lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* mountpointInner) {
  bool success = PHYSFS_mount(path, mountpoint, append);
  if (success && mountpointInner) {
    success = PHYSFS_setRoot(path, mountpointInner);
  }
  return !success;
}

void* lovrFilesystemRead(const char* path, size_t* bytesRead) {

  // Create file
  File* file = lovrFileCreate(path);
  if (!file) {
    lovrRelease(file);
    return NULL;
  }

  // Open it
  if (lovrFileOpen(file, OPEN_READ)) {
    lovrRelease(file);
    return NULL;
  }

  // Get file size
  size_t size = lovrFileGetSize(file);
  if (size == (unsigned int) -1) {
    lovrRelease(file);
    return NULL;
  }

  // Allocate buffer
  void* data = malloc(size);
  if (!data) {
    lovrRelease(file);
    return NULL;
  }

  // Perform read
  *bytesRead = lovrFileRead(file, data, size);
  lovrFileClose(file);
  lovrRelease(file);

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

static void setRequirePath(int i, const char* requirePath) {
  if (state.requirePath[i]) {
    free(state.requirePath[i]);
    vec_clear(&state.requirePattern[i]);
  }

  char* p = state.requirePath[i] = strdup(requirePath);

  while (1) {
    vec_push(&state.requirePattern[i], p);
    if ((p = strchr(p, ';')) != NULL) {
      *p++ = '\0';
    } else {
      break;
    }
  }
}

void lovrFilesystemSetRequirePath(const char* requirePath) {
  setRequirePath(0, requirePath);
}

void lovrFilesystemSetCRequirePath(const char* requirePath) {
  setRequirePath(1, requirePath);
}

int lovrFilesystemUnmount(const char* path) {
  return !PHYSFS_unmount(path);
}

size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  File* file = lovrFileCreate(path);
  if (!file) {
    return 0;
  }

  if (lovrFileOpen(file, append ? OPEN_APPEND : OPEN_WRITE)) {
    return 0;
  }

  size_t bytesWritten = lovrFileWrite(file, (void*) content, size);
  lovrFileClose(file);
  lovrRelease(file);
  return bytesWritten;
}
