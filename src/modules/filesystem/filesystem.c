#include "filesystem/filesystem.h"
#include "core/arr.h"
#include "core/fs.h"
#include "core/hash.h"
#include "core/map.h"
#include "core/zip.h"
#include "lib/stb/stb_image.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define FOREACH_ARCHIVE(a) for (Archive* a = state.archives.data; a != state.archives.data + state.archives.length; a++)

typedef arr_t(char) strpool;

static size_t strpool_append(strpool* pool, const char* string, size_t length) {
  size_t tip = pool->length;
  arr_reserve(pool, pool->length + length + 1);
  memcpy(pool->data + tip, string, length);
  pool->data[tip + length] = '\0';
  pool->length += length + 1;
  return tip;
}

static const char* strpool_resolve(strpool* pool, size_t offset) {
  return pool->data + offset;
}

typedef struct {
  uint32_t firstChild;
  uint32_t nextSibling;
  size_t filename;
  uint64_t offset;
  uint16_t mdate;
  uint16_t mtime;
  FileInfo info;
} zip_node;

typedef struct Archive {
  bool (*stat)(struct Archive* archive, const char* path, FileInfo* info);
  void (*list)(struct Archive* archive, const char* path, fs_list_cb callback, void* context);
  bool (*read)(struct Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** data);
  void (*close)(struct Archive* archive);
  zip_state zip;
  strpool strings;
  arr_t(zip_node) nodes;
  map_t lookup;
  size_t path;
  size_t pathLength;
  size_t mountpoint;
  size_t mountpointLength;
} Archive;

static struct {
  bool initialized;
  arr_t(Archive) archives;
  size_t savePathLength;
  char savePath[1024];
  char source[1024];
  char requirePath[2][1024];
  char* identity;
  bool fused;
} state;

static bool valid(const char* path) {
  if (path[0] == '.' && (path[1] == '\0' || path[1] == '.')) {
    return false;
  }

  do {
    if (
      *path == ':' ||
      *path == '\\' ||
      (*path == '/' && path[1] == '.' &&
      (path[2] == '.' ? (path[3] == '/' || path[3] == '\0') : (path[2] == '/' || path[2] == '\0')))
    ) {
      return false;
    }
  } while (*path++ != '\0');

  return true;
}

// Does not work with empty strings
static bool concat(char* buffer, const char* p1, size_t length1, const char* p2, size_t length2) {
  if (length1 + 1 + length2 >= LOVR_PATH_MAX) return false;
  memcpy(buffer + length1 + 1, p2, length2);
  buffer[length1 + 1 + length2] = '\0';
  memcpy(buffer, p1, length1);
  buffer[length1] = '/';
  return true;
}

static size_t normalize(char* buffer, const char* path, size_t length) {
  size_t i = 0;
  size_t n = 0;
  while (path[i] == '/') i++;
  while (i < length) {
    buffer[n++] = path[i++];
    while (path[i] == '/' && (path[i + 1] == '/' || path[i + 1] == '\0')) {
      i++;
    }
  }
  buffer[n] = '\0';
  return n;
}

bool lovrFilesystemInit(const char* argExe, const char* argGame, const char* argRoot) {
  if (state.initialized) return false;
  state.initialized = true;

  arr_init(&state.archives);
  arr_reserve(&state.archives, 2);

  lovrFilesystemSetRequirePath("?.lua;?/init.lua;lua_modules/?.lua;lua_modules/?/init.lua;deps/?.lua;deps/?/init.lua");
  lovrFilesystemSetCRequirePath("??;lua_modules/??;deps/??");

  // First, try to mount a bundled archive
  if (fs_getBundlePath(state.source, LOVR_PATH_MAX) && lovrFilesystemMount(state.source, NULL, true, argRoot)) {
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
  for (size_t i = 0; i < state.archives.length; i++) {
    Archive* archive = &state.archives.data[i];
    archive->close(archive);
  }
  arr_free(&state.archives);
  memset(&state, 0, sizeof(state));
}

const char* lovrFilesystemGetSource() {
  return state.source;
}

bool lovrFilesystemIsFused() {
  return state.fused;
}

// Archives

static bool dir_init(Archive* archive, const char* path, const char* mountpoint, const char* root);
static bool zip_init(Archive* archive, const char* path, const char* mountpoint, const char* root);

bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* root) {
  FOREACH_ARCHIVE(archive) {
    if (!strcmp(strpool_resolve(&archive->strings, archive->path), path)) {
      return false;
    }
  }

  Archive archive;
  arr_init(&archive.strings);

  if (!dir_init(&archive, path, mountpoint, root) && !zip_init(&archive, path, mountpoint, root)) {
    arr_free(&archive.strings);
    return false;
  }

  archive.pathLength = strlen(path);
  archive.path = strpool_append(&archive.strings, path, archive.pathLength);

  if (mountpoint) {
    char buffer[LOVR_PATH_MAX];
    size_t length = strlen(mountpoint);
    if (length >= sizeof(buffer)) return false;
    length = normalize(buffer, mountpoint, length);
    archive.mountpointLength = length;
    archive.mountpoint = strpool_append(&archive.strings, buffer, archive.mountpointLength);
  } else {
    archive.mountpointLength = 0;
    archive.mountpoint = 0;
  }

  if (append) {
    arr_push(&state.archives, archive);
  } else {
    arr_expand(&state.archives, 1);
    memmove(state.archives.data + 1, state.archives.data, sizeof(Archive) * state.archives.length);
    state.archives.data[0] = archive;
    state.archives.length++;
  }

  return true;
}

bool lovrFilesystemUnmount(const char* path) {
  FOREACH_ARCHIVE(archive) {
    if (!strcmp(strpool_resolve(&archive->strings, archive->path), path)) {
      archive->close(archive);
      arr_splice(&state.archives, archive - state.archives.data, 1);
      return true;
    }
  }
  return false;
}

static Archive* archiveStat(const char* path, FileInfo* info) {
  if (valid(path)) {
    FOREACH_ARCHIVE(archive) {
      if (archive->stat(archive, path, info)) {
        return archive;
      }
    }
  }
  return NULL;
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  FileInfo info;
  Archive* archive = archiveStat(path, &info);
  return archive ? (archive->strings.data + archive->path) : NULL;
}

bool lovrFilesystemIsFile(const char* path) {
  FileInfo info;
  return archiveStat(path, &info) ? info.type == FILE_REGULAR : false;
}

bool lovrFilesystemIsDirectory(const char* path) {
  FileInfo info;
  return archiveStat(path, &info) ? info.type == FILE_DIRECTORY : false;
}

uint64_t lovrFilesystemGetSize(const char* path) {
  FileInfo info;
  return archiveStat(path, &info) ? info.size : ~0ull;
}

uint64_t lovrFilesystemGetLastModified(const char* path) {
  FileInfo info;
  return archiveStat(path, &info) ? info.lastModified : ~0ull;
}

void* lovrFilesystemRead(const char* path, size_t bytes, size_t* bytesRead) {
  if (valid(path)) {
    void* data;
    FOREACH_ARCHIVE(archive) {
      if (archive->read(archive, path, bytes, bytesRead, &data)) {
        return data;
      }
    }
  }
  return NULL;
}

void lovrFilesystemGetDirectoryItems(const char* path, void (*callback)(void* context, const char* path), void* context) {
  if (valid(path)) {
    FOREACH_ARCHIVE(archive) {
      archive->list(archive, path, callback, context);
    }
  }
}

// Writing

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
  size_t cursor = fs_getDataDir(state.savePath, sizeof(state.savePath));

  // If the data path was too long or unavailable, fail
  if (cursor == 0) {
    return false;
  }

  // Make sure there is enough room to tack on /LOVR/<identity>
  if (cursor + strlen("/LOVR") + 1 + length >= sizeof(state.savePath)) {
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
  state.savePathLength = cursor;
  fs_mkdir(state.savePath);

  // Mount the fully resolved save path
  if (!lovrFilesystemMount(state.savePath, NULL, false, NULL)) {
    return false;
  }

  // Stash a pointer for the identity string (leaf of save path)
  state.identity = state.savePath + cursor - length;
  return true;
}

const char* lovrFilesystemGetSaveDirectory() {
  return state.savePath;
}

bool lovrFilesystemCreateDirectory(const char* path) {
  char resolved[LOVR_PATH_MAX];

  if (!valid(path) || !concat(resolved, state.savePath, state.savePathLength, path, strlen(path))) {
    return false;
  }

  char* cursor = resolved + state.savePathLength;
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
  return valid(path) && concat(resolved, state.savePath, state.savePathLength, path, strlen(path)) && fs_remove(resolved);
}

size_t lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  char resolved[LOVR_PATH_MAX];
  if (!valid(path) || !concat(resolved, state.savePath, state.savePathLength, path, strlen(path))) {
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

// Paths

size_t lovrFilesystemGetApplicationId(char* buffer, size_t size) {
  return fs_getBundleId(buffer, size);
}

size_t lovrFilesystemGetAppdataDirectory(char* buffer, size_t size) {
  return fs_getDataDir(buffer, size);
}

size_t lovrFilesystemGetExecutablePath(char* buffer, size_t size) {
  return fs_getExecutablePath(buffer, size);
}

size_t lovrFilesystemGetUserDirectory(char* buffer, size_t size) {
  return fs_getHomeDir(buffer, size);
}

size_t lovrFilesystemGetWorkingDirectory(char* buffer, size_t size) {
  return fs_getWorkDir(buffer, size);
}

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

// Archive: dir

static bool dir_resolve(char* buffer, Archive* archive, const char* path) {
  char innerBuffer[LOVR_PATH_MAX];
  size_t length = strlen(path);
  if (length >= sizeof(innerBuffer)) return false;
  length = normalize(innerBuffer, path, length);
  path = innerBuffer;

  if (archive->mountpoint) {
    if (strncmp(path, strpool_resolve(&archive->strings, archive->mountpoint), archive->mountpointLength)) {
      return false;
    } else {
      path += archive->mountpointLength;
      length -= archive->mountpointLength;
    }
  }

  return concat(buffer, strpool_resolve(&archive->strings, archive->path), archive->pathLength, path, length);
}

static bool dir_stat(Archive* archive, const char* path, FileInfo* info) {
  char resolved[LOVR_PATH_MAX];
  return dir_resolve(resolved, archive, path) && fs_stat(resolved, info);
}

static void dir_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  char resolved[LOVR_PATH_MAX];
  if (dir_resolve(resolved, archive, path)) {
    fs_list(resolved, callback, context);
  }
}

static bool dir_read(Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** data) {
  char resolved[LOVR_PATH_MAX];
  fs_handle file;

  if (!dir_resolve(resolved, archive, path) || !fs_open(resolved, OPEN_READ, &file)) {
    return false;
  }

  FileInfo info;
  if (bytes == (size_t) -1) {
    if (fs_stat(resolved, &info)) {
      bytes = info.size;
    } else {
      fs_close(file);
      return false;
    }
  }

  if ((*data = malloc(bytes)) == NULL) {
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

static void dir_close(Archive* archive) {
  arr_free(&archive->strings);
}

static bool dir_init(Archive* archive, const char* path, const char* mountpoint, const char* root) {
  FileInfo info;
  if (!fs_stat(path, &info) || info.type != FILE_DIRECTORY) {
    return false;
  }

  archive->stat = dir_stat;
  archive->list = dir_list;
  archive->read = dir_read;
  archive->close = dir_close;
  return true;
}

// Archive: zip

static zip_node* zip_lookup(Archive* archive, const char* path) {
  char buffer[LOVR_PATH_MAX];
  size_t length = strlen(path);
  if (length >= sizeof(buffer)) return NULL;
  length = normalize(buffer, path, length);
  uint64_t hash = hash64(buffer, length);
  uint64_t index = map_get(&archive->lookup, hash);
  return index == MAP_NIL ? NULL : &archive->nodes.data[index];
}

static bool zip_stat(Archive* archive, const char* path, FileInfo* info) {
  zip_node* node = zip_lookup(archive, path);
  if (!node) return false;

  // zip stores timestamps in dos time, conversion is slow so we do it only on request
  if (node->info.lastModified == ~0ull) {
    uint16_t mdate = node->mdate;
    uint16_t mtime = node->mtime;
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_isdst = -1;
    t.tm_year = ((mdate >> 9) & 127) + 80;
    t.tm_mon = ((mdate >> 5) & 15) - 1;
    t.tm_mday = mdate & 31;
    t.tm_hour = (mtime >> 11) & 31;
    t.tm_min = (mtime >> 5) & 63;
    t.tm_sec = (mtime << 1) & 62;
    node->info.lastModified = mktime(&t);
  }

  *info = node->info;
  return true;
}

static void zip_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  const zip_node* node = zip_lookup(archive, path);
  if (!node || node->info.type != FILE_DIRECTORY) return;
  uint32_t i = node->firstChild;
  while (i != ~0u) {
    zip_node* child = &archive->nodes.data[i];
    callback(context, strpool_resolve(&archive->strings, child->filename));
    i = child->nextSibling;
  }
}

static bool zip_read(Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** dst) {
  const zip_node* node = zip_lookup(archive, path);
  if (!node) return false;

  // Directories can't be read (but still return true because the file was present in the archive)
  if (node->info.type == FILE_DIRECTORY) {
    *dst = NULL;
    return true;
  }

  size_t dstSize = node->info.size;
  size_t srcSize;
  bool compressed;
  const void* src;

  if ((src = zip_load(&archive->zip, node->offset, &srcSize, &compressed)) == NULL) {
    *dst = NULL;
    return true;
  }

  if ((*dst = malloc(dstSize)) == NULL) {
    return true;
  }

  *bytesRead = (bytes == (size_t) -1 || bytes > dstSize) ? (uint32_t) dstSize : bytes;

  if (compressed) {
    if (stbi_zlib_decode_noheader_buffer(*dst, (int) dstSize, src, (int) srcSize) < 0) {
      free(*dst);
      *dst = NULL;
    }
  } else {
    memcpy(*dst, src, *bytesRead);
  }

  return true;
}

static void zip_close(Archive* archive) {
  arr_free(&archive->nodes);
  map_free(&archive->lookup);
  arr_free(&archive->strings);
  if (archive->zip.data) fs_unmap(archive->zip.data, archive->zip.size);
}

static bool zip_init(Archive* archive, const char* filename, const char* mountpoint, const char* root) {
  char path[LOVR_PATH_MAX];
  memset(&archive->lookup, 0, sizeof(archive->lookup));
  arr_init(&archive->nodes);

  // mmap the zip file, try to parse it, and figure out how many files there are
  archive->zip.data = fs_map(filename, &archive->zip.size);
  if (!archive->zip.data || !zip_open(&archive->zip) || archive->zip.count > UINT32_MAX) {
    zip_close(archive);
    return false;
  }

  // Paste mountpoint into path, normalize, and add trailing slash.  Paths are "pre hashed" with the
  // mountpoint prepended (and the root stripped) to avoid doing those operations on every lookup.
  size_t mountpointLength = 0;
  if (mountpoint) {
    mountpointLength = strlen(mountpoint);
    if (mountpointLength + 1 >= sizeof(path)) {
      zip_close(archive);
      return false;
    }

    mountpointLength = normalize(path, mountpoint, mountpointLength);
    path[mountpointLength++] = '/';
  }

  // Simple root normalization (only strips leading/trailing slashes, sorry)
  while (root && root[0] == '/') root++;
  size_t rootLength = root ? strlen(root) : 0;
  while (root && root[rootLength - 1] == '/') rootLength--;

  // Allocate
  map_init(&archive->lookup, archive->zip.count);
  arr_reserve(&archive->nodes, archive->zip.count);

  zip_file info;
  for (uint32_t i = 0; i < archive->zip.count; i++) {
    if (!zip_next(&archive->zip, &info)) {
      zip_close(archive);
      return false;
    }

    // Node
    zip_node node = {
      .firstChild = ~0u,
      .nextSibling = ~0u,
      .filename = (size_t) -1,
      .offset = info.offset,
      .mdate = info.mdate,
      .mtime = info.mtime,
      .info.size = info.size,
      .info.lastModified = ~0ull,
      .info.type = FILE_REGULAR
    };

    // Filenames that end in slashes are directories
    if (info.name[info.length - 1] == '/') {
      node.info.type = FILE_DIRECTORY;
      info.length--;
    }

    // Skip files if their names are too long
    if (mountpointLength + info.length - rootLength >= sizeof(path)) {
      continue;
    }

    // Skip files if they aren't under the root
    if (root && (info.length < rootLength || memcmp(info.name, root, rootLength))) {
      continue;
    }

    // Strip off the root from the filename and paste it after the mountpoint to get the "canonical" path
    size_t length = normalize(path + mountpointLength, info.name + rootLength, info.length - rootLength) + mountpointLength;
    size_t slash = length;

    // Keep chopping off path segments, building up a tree of paths
    // We can stop early if we reach a path that has already been indexed
    // Also add individual path segments to the string pool, for zip_list
    while (length != SIZE_MAX) {
      uint64_t hash = hash64(path, length);
      uint64_t index = map_get(&archive->lookup, hash);

      if (index == MAP_NIL) {
        index = archive->nodes.length;
        map_set(&archive->lookup, hash, index);
        arr_push(&archive->nodes, node);
        node.firstChild = index;
        node.info.type = FILE_DIRECTORY;
      } else {
        uint32_t childIndex = node.firstChild;
        zip_node* parent = &archive->nodes.data[index];
        zip_node* child = &archive->nodes.data[childIndex];
        child->nextSibling = parent->firstChild;
        parent->firstChild = childIndex;
        break;
      }

      while (length && path[length - 1] != '/') {
        length--;
      }

      archive->nodes.data[index].filename = strpool_append(&archive->strings, path + length, slash - length);
      slash = --length;
    }
  }

  archive->stat = zip_stat;
  archive->list = zip_list;
  archive->read = zip_read;
  archive->close = zip_close;
  return true;
}
