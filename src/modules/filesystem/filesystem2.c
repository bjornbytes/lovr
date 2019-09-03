#include "filesystem/filesystem.h"
#include "core/fs.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char lovrDirSep = '/';

#define MAX_ARCHIVES 8
#define FOREACH_ARCHIVE(a) for (Archive* a = state.archives; a != NULL; a = a->next)

// This check is a little too strict (.. can be valid), but for now it's good enough
static bool validate(const char* path) {
  const char* p = path;
  while (*p) {
    if (*p == ':' || *p == '\\' || (p[0] == '.' && p[1] == '.')) {
      return false;
    }
    p++;
  }
  return true;
}

static bool joinPaths(char* buffer, const char* p1, const char* p2) {
  return snprintf(buffer, LOVR_PATH_MAX, "%s/%s", p1, p2) < LOVR_PATH_MAX;
}

typedef struct Archive {
  struct Archive* next;
  char path[FS_PATH_MAX];
  char mountpoint[64];
  char root[64];
} Archive;

bool dir_read(Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** data) {
  char resolved[LOVR_PATH_MAX];
  if (!joinPaths(resolved, archive->path, path)) {
    return false;
  }

  FileInfo info;
  if (!fs_stat(resolved, &info)) {
    return false;
  }

  fs_handle file;
  if (!fs_open(resolved, OPEN_READ, &file)) {
    *data = NULL;
    return true;
  }

  if (bytes == (size_t) -1) {
    bytes = (size_t) info.size;
  }

  *data = malloc(bytes);
  if (!*data) {
    fs_close(file);
    return true;
  }

  if (!fs_read(file, *data, &bytes)) {
    fs_close(file);
    free(*data);
    *data = NULL;
    return true;
  }

  fs_close(file);
  *bytesRead = bytes;
  return true;
}

bool dir_stat(Archive* archive, const char* path, FileInfo *info) {
  char resolved[LOVR_PATH_MAX];
  return joinPaths(resolved, archive->path, path) && fs_stat(resolved, info);
}

void dir_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  char resolved[LOVR_PATH_MAX];
  if (joinPaths(resolved, archive->path, path)) {
    fs_list(resolved, callback, context);
  }
}

static struct {
  bool initialized;
  Archive* archives;
  Archive* waitlist;
  Archive pool[MAX_ARCHIVES];
  char savePath[LOVR_PATH_MAX];
  char requirePath[2][LOVR_PATH_MAX];
  char source[LOVR_PATH_MAX];
  char* identity;
  bool fused;
} state;

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot) {
  if (state.initialized) return false;
  state.initialized = true;

  lovrFilesystemSetRequirePath("?.lua;?/init.lua;lua_modules/?.lua;lua_modules/?/init.lua;deps/?.lua;deps/?/init.lua");
  lovrFilesystemSetCRequirePath("??;lua_modules/??;deps/??");

  state.waitlist = state.pool;
  for (size_t i = 0; i < MAX_ARCHIVES - 1; i++) {
    state.pool[i].next = &state.pool[i + 1];
  }

  // First, try to mount a source archive fused to the executable
  if (lovrFilesystemGetExecutablePath(state.source, LOVR_PATH_MAX) && lovrFilesystemMount(state.source, NULL, true, argRoot)) {
    state.fused = true;
    return true;
  }

  // If that didn't work, try mounting an archive passed in from the command line
  if (argGame) {
    state.source[LOVR_PATH_MAX - 1] = '\0';
    strncpy(state.source, argGame, LOVR_PATH_MAX - 1);
    if (lovrFilesystemMount(state.source, NULL, true, argRoot)) {
      return true;
    }
  }

  // Otherwise, there is no source
  state.source[0] = '\0';
  return true;
}

void lovrFilesystemDestroy() {
  if (!state.initialized) return;
  memset(&state, 0, sizeof(state));
}

const char* lovrFilesystemGetSource() {
  return state.source;
}

bool lovrFilesystemIsFused() {
  return state.fused;
}

// Known paths
// TODO wtf are these function sigs

bool lovrFilesystemGetApplicationId(char* dest, size_t size) {
  return false;
}

bool lovrFilesystemGetAppdataDirectory(char* dest, unsigned int size) {
  return fs_getDataDirectory(dest, size) > 0;
}

int lovrFilesystemGetExecutablePath(char* path, uint32_t size) {
  return !fs_getExecutablePath(path, size);
}

bool lovrFilesystemGetUserDirectory(char* buffer, size_t size) {
  return fs_getUserDirectory(buffer, size) > 0;
}

bool lovrFilesystemGetWorkingDirectory(char* buffer, unsigned int size) {
  return fs_getWorkingDirectory(buffer, size);
}

// Archives

bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* root) {
  FOREACH_ARCHIVE(archive) {
    if (!strcmp(archive->path, path)) {
      return false;
    }
  }

  size_t length = strlen(path);
  if (length > FS_PATH_MAX - 1) {
    return false;
  }

  FileInfo info;
  if (!fs_stat(path, &info) || info.type != FILE_DIRECTORY) {
    return false;
  }

  lovrAssert(state.waitlist, "Too many mounted archives (up to %d are supported)", MAX_ARCHIVES);
  Archive* archive = state.waitlist;
  state.waitlist = state.waitlist->next;
  memcpy(archive->path, path, length);
  archive->path[length] = '\0';

  // TODO mountpoint
  // TODO root

  if (!state.archives) {
    state.archives = archive;
  } else if (append) {
    Archive* tail = state.archives;
    while (tail->next) tail = tail->next;
    tail->next = archive;
  } else {
    archive->next = state.archives;
    state.archives = archive;
  }

  return true;
}

bool lovrFilesystemUnmount(const char* path) {
  Archive** prev = &state.archives;

  FOREACH_ARCHIVE(archive) {
    if (!strncmp(archive->path, path, sizeof(archive->path))) {
      *prev = archive->next;
      archive->next = state.waitlist;
      state.waitlist = archive;
      return true;
    }

    prev = &archive->next;
  }

  return false;
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  if (validate(path)) {
    FileInfo info;
    FOREACH_ARCHIVE(archive) {
      if (dir_stat(archive, path, &info)) {
        return archive->path;
      }
    }
  }
  return NULL;
}

bool lovrFilesystemIsFile(const char* path) {
  if (validate(path)) {
    FileInfo info;
    FOREACH_ARCHIVE(archive) {
      if (dir_stat(archive, path, &info)) {
        return info.type == FILE_REGULAR;
      }
    }
  }
  return false;
}

bool lovrFilesystemIsDirectory(const char* path) {
  if (validate(path)) {
    FileInfo info;
    FOREACH_ARCHIVE(archive) {
      if (dir_stat(archive, path, &info)) {
        return info.type == FILE_DIRECTORY;
      }
    }
  }
  return false;
}

// TODO return a uint64, this isn't memory
size_t lovrFilesystemGetSize(const char* path) {
  if (validate(path)) {
    FileInfo info;
    FOREACH_ARCHIVE(archive) {
      if (dir_stat(archive, path, &info)) {
        return info.size;
      }
    }
  }
  return -1;
}

// TODO lol long
long lovrFilesystemGetLastModified(const char* path) {
  if (validate(path)) {
    FileInfo info;
    FOREACH_ARCHIVE(archive) {
      if (dir_stat(archive, path, &info)) {
        return info.lastModified;
      }
    }
  }
  return -1;
}

void* lovrFilesystemRead(const char* path, size_t bytes, size_t* bytesRead) {
  if (validate(path)) {
    void* data;
    FOREACH_ARCHIVE(archive) {
      if (dir_read(archive, path, bytes, bytesRead, &data)) {
        return data;
      }
    }
  }
  return NULL;
}

// TODO callback casting
// TODO dedupe / sort (or use Lua for it)
void lovrFilesystemGetDirectoryItems(const char* path, getDirectoryItemsCallback callback, void* context) {
  if (validate(path)) {
    FOREACH_ARCHIVE(archive) {
      dir_list(archive, path, (fs_list_cb*) callback, context);
    }
  }
}

// Writes

const char* lovrFilesystemGetIdentity() {
  return state.identity;
}

bool lovrFilesystemSetIdentity(const char* identity) {
  size_t length = strlen(identity);

  // Identity can only be set once
  if (state.identity) {
    return false;
  }

  // Initialize the save path to the data path
  size_t cursor = fs_getDataDirectory(state.savePath, sizeof(state.savePath));

  // If the data path was too long or unavailable, fail
  if (cursor == 0) {
    return false;
  }

  // Make sure there is enough room to tack on /LOVR/<identity>
  if (cursor + strlen("/LOVR") + 1 + strlen(identity) >= sizeof(state.savePath)) {
    return false;
  }

  // Append /LOVR, mkdir
  memcpy(state.savePath + cursor, "/LOVR", strlen("/LOVR"));
  cursor += strlen("/LOVR");
  state.savePath[cursor] = '\0';
  fs_mkdir(state.savePath);

  // Append /<identity>, mkdir
  state.savePath[cursor++] = '/';
  memcpy(state.savePath + cursor, identity, length);
  cursor += length;
  state.savePath[cursor] = '\0';
  fs_mkdir(state.savePath);

  // Mount the fully resolved and created save path
  if (!lovrFilesystemMount(state.savePath, "/", false, NULL)) {
    return false;
  }

  // Stash a pointer for the identity string (the leaf of the save path)
  state.identity = state.savePath + cursor - length;
  return true;
}

const char* lovrFilesystemGetSaveDirectory() {
  return state.savePath;
}

bool lovrFilesystemCreateDirectory(const char* path) {
  char resolved[LOVR_PATH_MAX];

  if (!validate(path) || !joinPaths(resolved, state.savePath, path)) {
    return false;
  }

  char* cursor = resolved + strlen(state.savePath);
  while (*cursor == '/') cursor++;

  while (*cursor) {
    if (*cursor == '/') {
      *cursor = '\0';
      fs_mkdir(resolved);
      *cursor = '/';
    }
    cursor++;
  }

  return fs_mkdir(resolved);
}

bool lovrFilesystemRemove(const char* path) {
  char resolved[LOVR_PATH_MAX];
  return validate(path) && joinPaths(resolved, state.savePath, path) && fs_remove(resolved);
}

size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  char resolved[LOVR_PATH_MAX];
  if (!validate(path) || !joinPaths(resolved, state.savePath, path)) {
    return 0;
  }

  fs_handle file;
  if (!fs_open(resolved, append ? OPEN_APPEND : OPEN_WRITE, &file)) {
    return 0;
  }

  fs_write(file, content, &size);
  fs_close(file);
  return size;
}

// Require path

const char* lovrFilesystemGetRequirePath() {
  return state.requirePath[0];
}

const char* lovrFilesystemGetCRequirePath() {
  return state.requirePath[1];
}

void lovrFilesystemSetRequirePath(const char* requirePath) {
  strncpy(state.requirePath[0], requirePath, sizeof(state.requirePath[0]) - 1);
}

void lovrFilesystemSetCRequirePath(const char* requirePath) {
  strncpy(state.requirePath[1], requirePath, sizeof(state.requirePath[1]) - 1);
}
