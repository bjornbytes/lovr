#include "filesystem/filesystem.h"
#include "filesystem/file.h"
#include "platform.h"
#include "util.h"
#include <physfs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <objc/objc-runtime.h>
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

static struct {
  bool initialized;
  bool fused;
  char* source;
  const char* identity;
  char* savePathRelative;
  char* savePathFull;
  char requirePath[2][1024];
} state;

// Return the path to a LÖVR archive bundled to the executable.
// On most platforms, the zip is appended to the executable file itself.
// On macOS, we have to use Objective C to find it inside the .app folder.
static bool getBundlePath(char* buffer, size_t size) {
#ifdef __APPLE__
  id extension = ((id(*)(Class, SEL, char*))objc_msgSend)(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "lovr");
  id bundle = ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSBundle"), sel_registerName("mainBundle"));
  id path = ((id(*)(id, SEL, char*, id))objc_msgSend)(bundle, sel_registerName("pathForResource:ofType:"), nil, extension);
  if (path == nil) {
    return false;
  }

  const char* cpath = ((const char*(*)(id, SEL))objc_msgSend)(path, sel_registerName("UTF8String"));
  if (!cpath) {
    return false;
  }

  size_t length = strlen(cpath);
  if (length >= size) {
    return false;
  }

  memcpy(buffer, cpath, length);
  buffer[length] = '\0';
  return true;
#else
  return lovrFilesystemGetExecutablePath(buffer, size) == 0;
#endif
}

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot) {
  if (state.initialized) return false;
  state.initialized = true;

  if (!PHYSFS_init(argExe)) {
    lovrThrow("Could not initialize filesystem: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
  }

  PHYSFS_permitSymbolicLinks(1);
  state.source = malloc(LOVR_PATH_MAX * sizeof(char));
  lovrAssert(state.source, "Out of memory");
  state.identity = NULL;
  state.fused = true;
  lovrFilesystemSetRequirePath("?.lua;?/init.lua;lua_modules/?.lua;lua_modules/?/init.lua;deps/?.lua;deps/?/init.lua");
  lovrFilesystemSetCRequirePath("??;lua_modules/??;deps/??");

  // 1. Try loading an archive from the command line
  if (argGame) {
    strncpy(state.source, argGame, LOVR_PATH_MAX);
    if (lovrFilesystemMount(state.source, NULL, 1, argRoot)) {
      state.fused = false;
      return true;
    }
  }

  // 2. Try to mount an archive fused to the executable
  if (getBundlePath(state.source, LOVR_PATH_MAX) && lovrFilesystemMount(state.source, NULL, 1, argRoot)) {
    state.fused = true;
    return true;
  }

  // No game in arg nor in executable? Give up.
  free(state.source);
  state.source = NULL;
  return false;
}

void lovrFilesystemDestroy() {
  if (!state.initialized) return;
  free(state.source);
  free(state.savePathFull);
  free(state.savePathRelative);
  PHYSFS_deinit();
  memset(&state, 0, sizeof(state));
}

bool lovrFilesystemCreateDirectory(const char* path) {
  return PHYSFS_mkdir(path);
}

bool lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size) {
#ifdef __APPLE__
  const char* home;
  if ((home = getenv("HOME")) == NULL) {
    home = getpwuid(getuid())->pw_dir;
  }

  snprintf(dest, size, "%s/Library/Application Support", home);
  return true;
#elif _WIN32
  PWSTR appData = NULL;
  SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &appData);
  PHYSFS_utf8FromUtf16(appData, dest, size);
  CoTaskMemFree(appData);
  return true;
#elif EMSCRIPTEN
  strncpy(dest, "/home/web_user", size);
  return true;
#elif LOVR_USE_OCULUS_MOBILE
  strncpy(dest, lovrOculusMobileWritablePath, size);
  return true;
#elif __linux__
  const char* home;
  if ((home = getenv("HOME")) == NULL) {
    home = getpwuid(getuid())->pw_dir;
  }

  snprintf(dest, size, "%s/.config", home);
  return true;
#else
#error "This platform is missing an implementation for lovrFilesystemGetAppdataDirectory"
#endif

  return false;
}

bool lovrFilesystemGetApplicationId(char* dest, size_t size) {
#ifdef __ANDROID__
  pid_t pid = getpid();
  char path[32];
  snprintf(path, LOVR_PATH_MAX, "/proc/%i/cmdline", (int) pid);
  FILE* file = fopen(path, "r");
  if (file) {
    size_t read = fread(dest, 1, size, file);
    fclose(file);
    return true;
  }
  return false;
#else
  return false;
#endif
}

void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* userdata) {
  PHYSFS_enumerate(path, (PHYSFS_EnumerateCallback) callback, userdata);
}

int lovrFilesystemGetExecutablePath(char* path, uint32_t size) {
  return lovrPlatformGetExecutablePath(path, size);
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

const char* lovrFilesystemGetRequirePath() {
  return state.requirePath[0];
}

const char* lovrFilesystemGetCRequirePath() {
  return state.requirePath[1];
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

bool lovrFilesystemGetWorkingDirectory(char* dest, unsigned int size) {
#ifdef _WIN32
  WCHAR w_cwd[LOVR_PATH_MAX];
  _wgetcwd(w_cwd, LOVR_PATH_MAX);
  PHYSFS_utf8FromUtf16(w_cwd, dest, size);
  return true;
#else
  if (getcwd(dest, size)) {
    return true;
  }
#endif
  return false;
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
  return state.fused;
}

// Returns zero on success, nonzero on failure
bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* root) {
  bool success = PHYSFS_mount(path, mountpoint, append);
  if (success && root) {
    success = PHYSFS_setRoot(path, root);
  }
  return success;
}

void* lovrFilesystemRead(const char* path, size_t bytes, size_t* bytesRead) {
  File file;
  lovrFileInit(&file, path);

  if (!lovrFileOpen(&file, OPEN_READ)) {
    return NULL;
  }

  // Get file size if no size was specified
  if (bytes == (size_t) -1) {
    bytes = lovrFileGetSize(&file);
    if (bytes == (size_t) -1) {
      lovrFileDestroy(&file);
      return NULL;
    }
  }

  // Allocate buffer
  void* data = malloc(bytes);
  if (!data) {
    lovrFileDestroy(&file);
    return NULL;
  }

  // Perform read
  *bytesRead = lovrFileRead(&file, data, bytes);
  lovrFileDestroy(&file);
  return data;
}

bool lovrFilesystemRemove(const char* path) {
  return PHYSFS_delete(path);
}

bool lovrFilesystemSetIdentity(const char* identity) {
  state.identity = identity;

  // Unmount old write directory
  if (state.savePathFull && state.savePathRelative) {
    PHYSFS_unmount(state.savePathRelative);
  } else {
    state.savePathRelative = malloc(LOVR_PATH_MAX);
    state.savePathFull = malloc(LOVR_PATH_MAX);
    lovrAssert(state.savePathRelative && state.savePathFull, "Out of memory");
    if (!state.savePathRelative || !state.savePathFull) {
      return false;
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

  return PHYSFS_mount(state.savePathFull, NULL, 0);
}

void lovrFilesystemSetRequirePath(const char* requirePath) {
  strncpy(state.requirePath[0], requirePath, sizeof(state.requirePath[0]) - 1);
}

void lovrFilesystemSetCRequirePath(const char* requirePath) {
  strncpy(state.requirePath[1], requirePath, sizeof(state.requirePath[1]) - 1);
}

bool lovrFilesystemUnmount(const char* path) {
  return PHYSFS_unmount(path);
}

size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  File file;
  lovrFileInit(memset(&file, 0, sizeof(File)), path);

  if (!lovrFileOpen(&file, append ? OPEN_APPEND : OPEN_WRITE)) {
    return 0;
  }

  size_t bytesWritten = lovrFileWrite(&file, (void*) content, size);
  lovrFileDestroy(&file);
  return bytesWritten;
}
