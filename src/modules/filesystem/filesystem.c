#include "filesystem/filesystem.h"
#include "core/fs.h"
#include "core/os.h"
#include "util.h"
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
  uint64_t csize;
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
  char requirePath[1024];
  char identity[64];
  bool fused;
} state;

// Rejects any path component that would escape the virtual filesystem (./, ../, :, and \)
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

bool lovrFilesystemInit(const char* archive) {
  if (state.initialized) return false;
  state.initialized = true;

  arr_init(&state.archives, arr_alloc);
  arr_reserve(&state.archives, 2);

  lovrFilesystemSetRequirePath("?.lua;?/init.lua");

  // On Android, the save directory is mounted early, because the identity is fixed to the package
  // name and it is convenient to be able to load main.lua and conf.lua from the save directory,
  // which requires it to be mounted early in the boot process.
#ifdef __ANDROID__
  size_t cursor = os_get_data_directory(state.savePath, sizeof(state.savePath));

  // The data path ends in /package.id/files, so to extract the identity the '/files' is temporarily
  // chopped off and everything from the last slash is copied to the identity buffer
  if (cursor > 6) {
    state.savePath[cursor - 6] = '\0';
    char* id = strrchr(state.savePath, '/') + 1;
    size_t length = strlen(id);
    memcpy(state.identity, id, length);
    state.identity[length] = '\0';
    state.savePath[cursor - 6] = '/';
    state.savePathLength = cursor;
    if (!lovrFilesystemMount(state.savePath, NULL, false, NULL)) {
      state.identity[0] = '\0';
    }
  }
#endif

  // Try to mount a bundled archive
  const char* root = NULL;
  if (os_get_bundle_path(state.source, LOVR_PATH_MAX, &root) && lovrFilesystemMount(state.source, NULL, true, root)) {
    state.fused = true;
    return true;
  }

  // If that didn't work, try mounting an archive passed in from the command line
  if (archive) {
    state.source[LOVR_PATH_MAX - 1] = '\0';
    strncpy(state.source, archive, LOVR_PATH_MAX - 1);

    // If the command line parameter is a file, use its containing folder as the source
    size_t length = strlen(state.source);
    if (length > 4 && !memcmp(state.source + length - 4, ".lua", 4)) {
      char* slash = strrchr(state.source, '/');

      if (slash) {
        *slash = '\0';
      } else if ((slash = strrchr(state.source, '\\')) != NULL) {
        *slash = '\0';
      } else {
        state.source[0] = '.';
        state.source[1] = '\0';
      }
    }

    if (lovrFilesystemMount(state.source, NULL, true, NULL)) {
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
  arr_init(&archive.strings, arr_alloc);

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
  return state.identity[0] == '\0' ? NULL : state.identity;
}

bool lovrFilesystemSetIdentity(const char* identity, bool precedence) {
  size_t length = strlen(identity);

  // Identity can only be set once, and can't be empty
  if (state.identity[0] != '\0' || length == 0) {
    return false;
  }

  // Initialize the save path to the data path
  size_t cursor = os_get_data_directory(state.savePath, sizeof(state.savePath));

  // If the data path was too long or unavailable, fail
  if (cursor == 0) {
    return false;
  }

  // Make sure there is enough room to tack on /LOVR/<identity>
  if (cursor + 1 + strlen("LOVR") + 1 + length >= sizeof(state.savePath)) {
    return false;
  }

  // Append /LOVR, mkdir
  state.savePath[cursor++] = LOVR_PATH_SEP;
  memcpy(state.savePath + cursor, "LOVR", strlen("LOVR"));
  cursor += strlen("LOVR");
  state.savePath[cursor] = '\0';
  fs_mkdir(state.savePath);

  // Append /<identity>, mkdir
  state.savePath[cursor++] = LOVR_PATH_SEP;
  memcpy(state.savePath + cursor, identity, length);
  cursor += length;
  state.savePath[cursor] = '\0';
  state.savePathLength = cursor;
  fs_mkdir(state.savePath);

  // Set the identity string
  memcpy(state.identity, identity, length + 1);

  // Mount the fully resolved save path
  if (!lovrFilesystemMount(state.savePath, NULL, !precedence, NULL)) {
    state.identity[0] = '\0';
    return false;
  }

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

bool lovrFilesystemWrite(const char* path, const char* content, size_t size, bool append) {
  char resolved[LOVR_PATH_MAX];
  if (!valid(path) || !concat(resolved, state.savePath, state.savePathLength, path, strlen(path))) {
    return false;
  }

  fs_handle file;
  if (!fs_open(resolved, append ? OPEN_APPEND : OPEN_WRITE, &file)) {
    return false;
  }

  if (!fs_write(file, content, &size)) {
    return false;
  }

  return fs_close(file);
}

// Paths

size_t lovrFilesystemGetAppdataDirectory(char* buffer, size_t size) {
  return os_get_data_directory(buffer, size);
}

size_t lovrFilesystemGetExecutablePath(char* buffer, size_t size) {
  return os_get_executable_path(buffer, size);
}

size_t lovrFilesystemGetUserDirectory(char* buffer, size_t size) {
  return os_get_home_directory(buffer, size);
}

size_t lovrFilesystemGetWorkingDirectory(char* buffer, size_t size) {
  return os_get_working_directory(buffer, size);
}

const char* lovrFilesystemGetRequirePath() {
  return state.requirePath;
}

void lovrFilesystemSetRequirePath(const char* requirePath) {
  size_t length = strlen(requirePath);
  lovrCheck(length < sizeof(state.requirePath), "Require path is too long");
  memcpy(state.requirePath, requirePath, length);
  state.requirePath[length] = '\0';
}

// Archive: dir

enum {
  PATH_INVALID,
  PATH_VIRTUAL,
  PATH_PHYSICAL
};

static int dir_resolve(Archive* archive, char* buffer, const char* rawpath) {
  char normalized[LOVR_PATH_MAX];
  char* path = normalized;

  // Normalize the path
  size_t length = strlen(rawpath);
  if (length >= sizeof(normalized)) return PATH_INVALID;
  length = normalize(normalized, rawpath, length);

  // Compare each component of normalized path and mountpoint
  if (archive->mountpointLength > 0) {
    const char* mountpoint = strpool_resolve(&archive->strings, archive->mountpoint);
    size_t mountpointLength = archive->mountpointLength;

    for (;;) {
      char* slash = strchr(mountpoint, '/');
      size_t sublength = slash ? slash - mountpoint : mountpointLength;

      // If the path is empty but there was still stuff in the mountpoint, it's a virtual directory
      if (length == 0) {
        // Return child directory's name for convenience in getDirectoryItems
        memcpy(buffer, mountpoint, sublength);
        buffer[sublength] = '\0';
        return PATH_VIRTUAL;
      }

      // Check for paths that don't match this component of the mountpoint
      if (length < sublength || strncmp(path, mountpoint, sublength)) {
        return PATH_INVALID;
      }

      // If the path matched, make sure there's a slash after the match
      if (length > sublength && path[sublength] != '/') {
        return PATH_INVALID;
      }

      // Strip this component off of the path
      if (length == sublength) {
        path += sublength;
        length -= sublength;
      } else {
        path += sublength + 1;
        length -= sublength + 1;
      }

      // Strip this component off of the mountpoint, if mountpoint is empty then we're done
      if (mountpointLength > sublength) {
        mountpoint += sublength + 1;
        mountpointLength -= sublength + 1;
      } else {
        break;
      }
    }
  }

  // Concat archive path and normalized path (without mountpoint), return full path
  if (!concat(buffer, strpool_resolve(&archive->strings, archive->path), archive->pathLength, path, length)) {
    return PATH_INVALID;
  }

  return PATH_PHYSICAL;
}

static bool dir_stat(Archive* archive, const char* path, FileInfo* info) {
  char resolved[LOVR_PATH_MAX];
  switch (dir_resolve(archive, resolved, path)) {
    default:
    case PATH_INVALID: return false;
    case PATH_VIRTUAL: return fs_stat(strpool_resolve(&archive->strings, archive->path), info);
    case PATH_PHYSICAL: return fs_stat(resolved, info);
  }
}

static void dir_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  char resolved[LOVR_PATH_MAX];
  switch (dir_resolve(archive, resolved, path)) {
    case PATH_INVALID: return;
    case PATH_VIRTUAL: callback(context, resolved); return;
    case PATH_PHYSICAL: fs_list(resolved, callback, context); return;
  }
}

static bool dir_read(Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** data) {
  char resolved[LOVR_PATH_MAX];
  if (dir_resolve(archive, resolved, path) != PATH_PHYSICAL) {
    return false;
  }

  fs_handle file;
  if (!fs_open(resolved, OPEN_READ, &file)) {
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
  uint64_t hash = length ? hash64(buffer, length) : 0;
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
  size_t srcSize = node->csize;
  bool compressed;
  const void* src;

  if ((src = zip_load(&archive->zip, node->offset, &compressed)) == NULL) {
    *dst = NULL;
    return true;
  }

  if ((*dst = malloc(dstSize)) == NULL) {
    return true;
  }

  *bytesRead = (bytes == (size_t) -1 || bytes > dstSize) ? (uint32_t) dstSize : bytes;

  if (compressed) {
    srcSize += 4; // pad buffer to fix an stb_image "bug"
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
  arr_init(&archive->nodes, arr_alloc);

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

    if (mountpointLength > 0) {
      path[mountpointLength++] = '/';
    }
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
      .csize = info.csize,
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
    for (;;) {
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

      // Root node
      if (length == 0) {
        index = archive->nodes.length;
        map_set(&archive->lookup, 0, index);
        arr_push(&archive->nodes, node);
        archive->nodes.data[index].filename = strpool_append(&archive->strings, path, 0);
        break;
      }

      slash = --length;
    }
  }

  archive->stat = zip_stat;
  archive->list = zip_list;
  archive->read = zip_read;
  archive->close = zip_close;
  return true;
}
