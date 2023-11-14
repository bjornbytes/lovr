#include "filesystem/filesystem.h"
#include "core/fs.h"
#include "core/os.h"
#include "util.h"
#include "lib/stb/stb_image.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define FOREACH_ARCHIVE(a) for (Archive* a = state.archives; a; a = a->next)

typedef struct {
  uint32_t firstChild;
  uint32_t nextSibling;
  const char* filename;
  const void* data;
  uint32_t compressedSize;
  uint32_t uncompressedSize;
  uint16_t filenameLength;
  uint16_t mtime;
  uint16_t mdate;
  bool directory;
  bool compressed;
} zip_node;

struct Archive {
  uint32_t ref;
  struct Archive* next;
  bool (*stat)(struct Archive* archive, const char* path, FileInfo* info, bool needTime);
  void (*list)(struct Archive* archive, const char* path, fs_list_cb callback, void* context);
  bool (*read)(struct Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** data);
  char* path;
  char* mountpoint;
  size_t pathLength;
  size_t mountLength;
  uint8_t* data;
  size_t size;
  map_t lookup;
  arr_t(zip_node) nodes;
};

static struct {
  bool initialized;
  Archive* archives;
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

// Buffer must be at least as big as path length
static size_t normalize(const char* path, size_t length, char* buffer) {
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

// Validates path and strips leading, trailing, and consecutive slashes, updating length
static bool sanitize(const char* path, char* buffer, size_t* length) {
  if (!valid(path)) return false;
  size_t pathLength = strlen(path);
  if (pathLength >= *length) return false;
  *length = normalize(path, pathLength, buffer);
  return true;
}

bool lovrFilesystemInit(const char* archive) {
  if (state.initialized) return false;
  state.initialized = true;

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

void lovrFilesystemDestroy(void) {
  if (!state.initialized) return;
  Archive* archive = state.archives;
  while (archive) {
    Archive* next = archive->next;
    lovrRelease(archive, lovrArchiveDestroy);
    archive = next;
  }
  memset(&state, 0, sizeof(state));
}

const char* lovrFilesystemGetSource(void) {
  return state.source;
}

bool lovrFilesystemIsFused(void) {
  return state.fused;
}

// Archives

bool lovrFilesystemMount(const char* path, const char* mountpoint, bool append, const char* root) {
  FOREACH_ARCHIVE(archive) {
    if (!strcmp(archive->path, path)) {
      return false;
    }
  }

  Archive* archive = lovrArchiveCreate(path, mountpoint, root);
  if (!archive) return false;

  if (append) {
    Archive* list = state.archives;
    while (list && list->next) list = list->next;
    *(list ? &list->next : &state.archives) = archive;
  } else {
    archive->next = state.archives;
    state.archives = archive;
  }

  return true;
}

bool lovrFilesystemUnmount(const char* path) {
  for (Archive** archive = &state.archives; *archive; archive = &(*archive)->next) {
    if (!strcmp((*archive)->path, path)) {
      Archive* next = (*archive)->next;
      lovrRelease(*archive, lovrArchiveDestroy);
      *archive = next;
      return true;
    }
  }
  return false;
}

static bool archiveContains(Archive* archive, const char* path, size_t length) {
  if (archive->mountLength == 0) return true;
  if (length == archive->mountLength && !memcmp(path, archive->mountpoint, archive->mountLength)) return true;
  if (length > archive->mountLength && path[archive->mountLength] == '/' && !memcmp(path, archive->mountpoint, archive->mountLength)) return true;
  return false;
}

static bool mountpointContains(Archive* archive, const char* path, size_t length) {
  return length < archive->mountLength && archive->mountpoint[length] == '/' && !memcmp(path, archive->mountpoint, length);
}

static Archive* lovrFilesystemStat(const char* p, FileInfo* info, bool needTime) {
  char path[1024];
  size_t length = sizeof(path);
  if (sanitize(p, path, &length)) {
    FOREACH_ARCHIVE(archive) {
      if (archiveContains(archive, path, length)) {
        if (archive->stat(archive, path, info, needTime)) {
          return archive;
        }
      } else if (mountpointContains(archive, path, length)) {
        info->type = FILE_DIRECTORY;
        info->lastModified = ~0ull;
        info->size = 0;
        return archive;
      }
    }
  }
  return NULL;
}

const char* lovrFilesystemGetRealDirectory(const char* path) {
  FileInfo info;
  Archive* archive = lovrFilesystemStat(path, &info, false);
  return archive ? archive->path : NULL;
}

bool lovrFilesystemIsFile(const char* path) {
  FileInfo info;
  return lovrFilesystemStat(path, &info, false) && info.type == FILE_REGULAR;
}

bool lovrFilesystemIsDirectory(const char* path) {
  FileInfo info;
  return lovrFilesystemStat(path, &info, false) && info.type == FILE_DIRECTORY;
}

uint64_t lovrFilesystemGetSize(const char* path) {
  FileInfo info;
  return lovrFilesystemStat(path, &info, false) && info.type == FILE_REGULAR ? info.size : 0;
}

uint64_t lovrFilesystemGetLastModified(const char* path) {
  FileInfo info;
  return lovrFilesystemStat(path, &info, true) ? info.lastModified : ~0ull;
}

void* lovrFilesystemRead(const char* p, size_t bytes, size_t* bytesRead) {
  void* data;
  char path[1024];
  size_t length = sizeof(path);
  if (sanitize(p, path, &length)) {
    FOREACH_ARCHIVE(archive) {
      if (archiveContains(archive, path, length)) {
        if (archive->read(archive, path, bytes, bytesRead, &data)) {
          return data;
        }
      }
    }
  }
  return NULL;
}

void lovrFilesystemGetDirectoryItems(const char* p, void (*callback)(void* context, const char* path), void* context) {
  char path[1024];
  size_t length = sizeof(path);
  if (sanitize(p, path, &length)) {
    FOREACH_ARCHIVE(archive) {
      if (archiveContains(archive, path, length)) {
        archive->list(archive, path, callback, context);
      } else if (mountpointContains(archive, path, length)) {
        const char* leaf = archive->mountpoint + length + 1;
        const char* slash = strchr(leaf, '/');
        size_t sublength = slash ? slash - leaf : archive->mountLength - length - 1;
        char buffer[1024];
        memcpy(buffer, leaf, sublength);
        buffer[sublength] = '\0';
        callback(context, buffer);
      }
    }
  }
}

// Writing

const char* lovrFilesystemGetIdentity(void) {
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

const char* lovrFilesystemGetSaveDirectory(void) {
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

const char* lovrFilesystemGetRequirePath(void) {
  return state.requirePath;
}

void lovrFilesystemSetRequirePath(const char* requirePath) {
  size_t length = strlen(requirePath);
  lovrCheck(length < sizeof(state.requirePath), "Require path is too long");
  memcpy(state.requirePath, requirePath, length);
  state.requirePath[length] = '\0';
}

// Archive: dir

static bool dir_resolve(Archive* archive, const char* fulpathLength, char* buffer) {
  const char* path = fulpathLength + (archive->mountLength ? archive->mountLength + 1 : 0);
  return concat(buffer, archive->path, archive->pathLength, path, strlen(path));
}

static bool dir_open(Archive* archive, const char* path, const char* root) {
  FileInfo info;
  return fs_stat(path, &info) && info.type == FILE_DIRECTORY;
}

static bool dir_stat(Archive* archive, const char* path, FileInfo* info, bool needTime) {
  char resolved[LOVR_PATH_MAX];
  return dir_resolve(archive, path, resolved) && fs_stat(resolved, info);
}

static void dir_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  char resolved[LOVR_PATH_MAX];
  if (dir_resolve(archive, path, resolved)) {
    fs_list(resolved, callback, context);
  }
}

static bool dir_read(Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** data) {
  char resolved[LOVR_PATH_MAX];
  if (!dir_resolve(archive, path, resolved)) {
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

// Archive: zip

static uint16_t readu16(const uint8_t* p) { uint16_t x; memcpy(&x, p, sizeof(x)); return x; }
static uint32_t readu32(const uint8_t* p) { uint32_t x; memcpy(&x, p, sizeof(x)); return x; }

static void zip_close(Archive* archive) {
  arr_free(&archive->nodes);
  map_free(&archive->lookup);
  if (archive->data) fs_unmap(archive->data, archive->size);
}

static bool zip_open(Archive* archive, const char* filename, const char* root) {
  // Map the zip file into memory
  archive->data = fs_map(filename, &archive->size);
  if (!archive->data) {
    zip_close(archive);
    return false;
  }

  // Check the end of the file for the magic zip footer
  const uint8_t* p = archive->data + archive->size - 22;
  if (archive->size < 22 || readu32(p) != 0x06054b50) {
    zip_close(archive);
    return false;
  }

  // Parse the number of file entries and reserve memory
  uint16_t nodeCount = readu16(p + 10);
  arr_init(&archive->nodes, realloc);
  arr_reserve(&archive->nodes, nodeCount);
  map_init(&archive->lookup, nodeCount);

  zip_node rootNode;
  memset(&rootNode, 0xff, sizeof(zip_node));
  arr_push(&archive->nodes, rootNode);

  // See where the zip thinks its central directory is
  uint64_t cursor = readu32(p + 16);
  if (cursor + 4 > archive->size) {
    zip_close(archive);
    return false;
  }

  // See if the central directory starts where the endOfCentralDirectory said it would.
  // If it doesn't, then it might be a self-extracting archive with broken offsets (common).
  // In this case, assume the central directory is directly adjacent to the endOfCentralDirectory,
  // located at (offsetOfEndOfCentralDirectory (aka size - 22) - sizeOfCentralDirectory).
  // If we find a central directory there, then compute a "base" offset that equals the difference
  // between where it is and where it was supposed to be, and apply this offset to everything else.
  uint64_t base = 0;
  if (readu32(archive->data + cursor) != 0x02014b50) {
    size_t offsetOfEndOfCentralDirectory = archive->size - 22;
    size_t sizeOfCentralDirectory = readu32(p + 12);
    size_t centralDirectoryOffset = offsetOfEndOfCentralDirectory - sizeOfCentralDirectory;

    if (sizeOfCentralDirectory > offsetOfEndOfCentralDirectory || centralDirectoryOffset + 4 > archive->size) {
      zip_close(archive);
      return false;
    }

    base = centralDirectoryOffset - cursor;
    cursor = centralDirectoryOffset;

    // And if that didn't work, just give up
    if (readu32(archive->data + cursor) != 0x02014b50) {
      zip_close(archive);
      return false;
    }
  }

  // Simple root normalization (only strips leading/trailing slashes, sorry)
  while (root && root[0] == '/') root++;
  size_t rootLength = root ? strlen(root) : 0;
  while (root && root[rootLength - 1] == '/') rootLength--;

  // Iterate the list of files in the zip and build up a tree of nodes
  for (uint32_t i = 0; i < nodeCount; i++) {
    p = archive->data + cursor;
    if (cursor + 46 > archive->size || readu32(p) != 0x02014b50) {
      zip_close(archive);
      return false;
    }

    zip_node node;
    node.firstChild = ~0u;
    node.nextSibling = ~0u;
    node.compressedSize = readu32(p + 20);
    node.uncompressedSize = readu32(p + 24);
    node.mtime = readu16(p + 12);
    node.mdate = readu16(p + 14);
    node.compressed = readu16(p + 10) == 8;
    node.directory = false;
    uint16_t length = readu16(p + 28);
    const char* path = (const char*) (p + 46);
    cursor += 46 + readu16(p + 28) + readu16(p + 30) + readu16(p + 32);

    // Sanity check the local file header
    uint64_t headerOffset = base + readu32(p + 42);
    uint8_t* header = archive->data + headerOffset;
    if (base + headerOffset > archive->size - 30 || readu32(header) != 0x04034b50) {
      zip_close(archive);
      return false;
    }

    // Filename and extra data are 30 bytes after the header, then the data starts
    uint64_t dataOffset = headerOffset + 30 + readu16(header + 26) + readu16(header + 28);
    node.data = archive->data + dataOffset;

    // Make sure data is actually contained in the zip
    if (dataOffset + node.compressedSize > archive->size) {
      zip_close(archive);
      return false;
    }

    // Strip leading slashes
    while (length > 0 && *path == '/') {
      length--;
      path++;
    }

    // Filenames that end in slashes are directories
    while (length > 0 && path[length - 1] == '/') {
      node.directory = true;
      length--;
    }

    // Skip files if their names are too long (or too short)
    if (length == 0 || length < rootLength) {
      continue;
    }

    // Skip files if they aren't under the root
    if (root && memcmp(path, root, rootLength)) {
      continue;
    }

    // Strip root
    path += rootLength;
    length -= rootLength;

    // Keep chopping off path segments, building up a tree of paths
    // We can stop early if we reach a path that has already been indexed
    for (;;) {
      uint64_t hash = hash64(path, length);
      uint64_t index = map_get(&archive->lookup, hash);

      if (index == MAP_NIL) {
        index = archive->nodes.length;
        map_set(&archive->lookup, hash, index);
        arr_push(&archive->nodes, node);
        node.firstChild = index;
      } else {
        uint32_t childIndex = node.firstChild;
        zip_node* parent = &archive->nodes.data[index];
        zip_node* child = &archive->nodes.data[childIndex];
        child->nextSibling = parent->firstChild;
        parent->firstChild = childIndex;
        break;
      }

      uint16_t end = length;
      while (length && path[length - 1] != '/') {
        length--;
      }

      archive->nodes.data[index].filename = path + length;
      archive->nodes.data[index].filenameLength = end - length;

      // Root node
      if (length == 0) {
        archive->nodes.data[index].nextSibling = archive->nodes.data[0].firstChild;
        archive->nodes.data[0].firstChild = index;
        break;
      }

      while (path[length - 1] == '/') length--;
    }
  }

  return true;
}

static zip_node* zip_resolve(Archive* archive, const char* fulpathLength) {
  const char* path = fulpathLength + (archive->mountLength ? archive->mountLength + 1 : 0);
  size_t length = strlen(path);
  if (length == 0) return &archive->nodes.data[0];
  uint64_t hash = hash64(path, length);
  uint64_t index = map_get(&archive->lookup, hash);
  return index == MAP_NIL ? NULL : &archive->nodes.data[index];
}

static bool zip_stat(Archive* archive, const char* path, FileInfo* info, bool needTime) {
  zip_node* node = zip_resolve(archive, path);
  if (!node) return false;

  // This is slow, so it's only done when asked for
  if (needTime) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_isdst = -1;
    t.tm_year = ((node->mdate >> 9) & 127) + 80;
    t.tm_mon = ((node->mdate >> 5) & 15) - 1;
    t.tm_mday = node->mdate & 31;
    t.tm_hour = (node->mtime >> 11) & 31;
    t.tm_min = (node->mtime >> 5) & 63;
    t.tm_sec = (node->mtime << 1) & 62;
    info->lastModified = mktime(&t);
  }

  info->size = node->uncompressedSize;
  info->type = node->directory ? FILE_DIRECTORY : FILE_REGULAR;
  return true;
}

static void zip_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  const zip_node* node = zip_resolve(archive, path);
  if (!node) return;
  for (uint32_t i = node->firstChild; i != ~0u; i = node->nextSibling) {
    char buffer[1024];
    node = &archive->nodes.data[i];
    memcpy(buffer, node->filename, node->filenameLength);
    buffer[node->filenameLength] = '\0';
    callback(context, buffer);
  }
}

static bool zip_read(Archive* archive, const char* path, size_t bytes, size_t* bytesRead, void** dst) {
  const zip_node* node = zip_resolve(archive, path);
  if (!node) return false;
  *dst = NULL;

  // Directories can't be read (but still return true because the file was present in the archive)
  if (node->directory) {
    return true;
  }

  if ((*dst = malloc(node->uncompressedSize)) == NULL) {
    return true;
  }

  *bytesRead = (bytes == (size_t) -1 || bytes > node->uncompressedSize) ? (uint32_t) node->uncompressedSize : bytes;

  if (node->compressed) {
    // Add 4 to compressed size to fix an stb_image "bug" where it reads past end of buffer
    if (stbi_zlib_decode_noheader_buffer(*dst, (int) node->uncompressedSize, node->data, (int) node->compressedSize + 4) < 0) {
      free(*dst);
      *dst = NULL;
    }
  } else {
    memcpy(*dst, node->data, *bytesRead);
  }

  return true;
}

// Archive

Archive* lovrArchiveCreate(const char* path, const char* mountpoint, const char* root) {
  Archive* archive = calloc(1, sizeof(Archive));
  lovrAssert(archive, "Out of memory");
  archive->ref = 1;

  if (dir_open(archive, path, root)) {
    archive->stat = dir_stat;
    archive->list = dir_list;
    archive->read = dir_read;
  } else if (zip_open(archive, path, root)) {
    archive->stat = zip_stat;
    archive->list = zip_list;
    archive->read = zip_read;
  } else {
    free(archive);
    return NULL;
  }

  if (mountpoint) {
    size_t length = strlen(mountpoint);
    archive->mountpoint = malloc(length + 1);
    lovrAssert(archive->mountpoint, "Out of memory");
    archive->mountLength = normalize(mountpoint, length, archive->mountpoint);
  }

  archive->pathLength = strlen(path);
  archive->path = malloc(archive->pathLength + 1);
  lovrAssert(archive->path, "Out of memory");
  memcpy(archive->path, path, archive->pathLength + 1);

  return archive;
}

void lovrArchiveDestroy(void* ref) {
  Archive* archive = ref;
  if (archive->data) zip_close(archive);
  free(archive->mountpoint);
  free(archive->path);
  free(archive);
}
