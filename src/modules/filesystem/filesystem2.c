#include "filesystem/filesystem.h"
#include "core/fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// TODO no
const char lovrDirSep = '/';

static bool isValidPath(const char* path) {
  return !strstr(path, ".."); // TODO check it's bounded by slashes / \0s
}

static bool joinPaths(char buffer[/* static LOVR_PATH_MAX */], const char* p1, const char* p2) {
  // TODO maybe do some normalization (trailing/leading slashes)
  // TODO do we need snprintf? maybe just memcpys, try to avoid stdio
  return snprintf(buffer, LOVR_PATH_MAX, "%s/%s", p1, p2) >= LOVR_PATH_MAX;
}

typedef struct Archive {
  struct Archive* next;
  char* path;
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
  char source[LOVR_PATH_MAX];
  char requirePath[2][LOVR_PATH_MAX];
  char savePath[LOVR_PATH_MAX];
  char* identity;
  Archive* head;
  bool fused;
} state;

#define FOREACH_ARCHIVE(a) for (Archive* a = state.head; a != NULL; a = a->next)

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot) {
  if (state.initialized) return false;
  state.initialized = true;

  // TODO this is Lua's problem
  lovrFilesystemSetRequirePath("?.lua;?/init.lua;lua_modules/?.lua;lua_modules/?/init.lua;deps/?.lua;deps/?/init.lua");
  lovrFilesystemSetCRequirePath("??;lua_modules/??;deps/??");

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
  // TODO
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
  //return fs_getDataDirectory(dest);
  return false;
}

int lovrFilesystemGetExecutablePath(char* path, uint32_t size) {
  return 1;
}

const char* lovrFilesystemGetUserDirectory() {
  return NULL;
}

bool lovrFilesystemGetWorkingDirectory(char* dest, unsigned int size) {
  return false;
}

// Archives

bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* root) {
  FOREACH_ARCHIVE(archive) {
    if (!strcmp(archive->path, path)) {
      return false;
    }
  }

  FileInfo info;
  if (!fs_stat(path, &info) || info.type != FILE_DIRECTORY) {
    return false;
  }

  Archive* archive = malloc(sizeof(Archive)); // TODO no allocation
  if (!archive) {
    return false;
  }

  archive->path = strdup(path); // TODO no allocation
  if (!archive->path) {
    return false;
  }

  // TODO mountpoint
  // TODO root

  // TODO append (actually append is too complicated / not useful?  known ordering is very nice)
  archive->next = state.head;
  state.head = archive;
  return true;
}

bool lovrFilesystemUnmount(const char* path) {
  Archive* prev = NULL;

  FOREACH_ARCHIVE(archive) {
    if (!strcmp(archive->path, path)) {
      if (prev) {
        prev->next = archive->next;
      }

      if (archive == state.head) {
        state.head = archive->next;
      }

      // TODO no allocation
      free(archive->path);
      free(archive);
      return true;
    }

    prev = archive;
  }

  return false;
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  if (isValidPath(path)) {
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
  if (isValidPath(path)) {
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
  if (isValidPath(path)) {
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
  if (isValidPath(path)) {
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
  if (isValidPath(path)) {
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
  if (isValidPath(path)) {
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
  if (isValidPath(path)) {
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

  // Identity can only be set once
  if (state.identity) {
    return false;
  }

  char buffer[LOVR_PATH_MAX];
  if (!lovrFilesystemGetAppdataDirectory(buffer, LOVR_PATH_MAX)) {
    return false;
  }

  // Get appdata
  // Append LOVR, mkdir or stat
  // Append identity, mkdir or stat
  // Set savePath to final buffer contents
  // Mount the savePath
  // Seriously consider setting identity to just be a pointer to the leaf of the savePath, instead
  // of copying into its own weird buffer

  return true;
}

const char* lovrFilesystemGetSaveDirectory() {
  return state.savePath;
}

// TODO mkdir_p
bool lovrFilesystemCreateDirectory(const char* path) {
  char resolved[LOVR_PATH_MAX];
  return isValidPath(path) && joinPaths(resolved, state.savePath, path) && fs_mkdir(resolved);
}

bool lovrFilesystemRemove(const char* path) {
  char resolved[LOVR_PATH_MAX];
  return isValidPath(path) && joinPaths(resolved, state.savePath, path) && fs_remove(resolved);
}

size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  char resolved[LOVR_PATH_MAX];
  if (!isValidPath(path) || !joinPaths(resolved, state.savePath, path)) {
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
// TODO rm

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
