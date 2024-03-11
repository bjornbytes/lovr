#include "filesystem/filesystem.h"
#include "core/fs.h"
#include "core/os.h"
#include "util.h"
#include "lib/miniz/miniz_tinfl.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

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

typedef struct {
  size_t inputCursor;
  size_t outputCursor;
  size_t bufferExtent;
  uint8_t buffer[32768];
  tinfl_decompressor decompressor;
} zip_stream;

typedef struct {
  fs_handle file;
  zip_node* node;
  zip_stream* stream;
  uint64_t offset;
} Handle;

struct Archive {
  uint32_t ref;
  struct Archive* next;
  bool (*open)(Archive* archive, const char* path, Handle* handle);
  bool (*close)(Archive* archive, Handle* handle);
  bool (*read)(Archive* archive, Handle* handle, uint8_t* data, size_t size, size_t* count);
  bool (*seek)(Archive* archive, Handle* handle, uint64_t offset);
  bool (*fsize)(Archive* archive, Handle* handle, size_t* size);
  bool (*stat)(Archive* archive, const char* path, FileInfo* info, bool needTime);
  void (*list)(Archive* archive, const char* path, fs_list_cb callback, void* context);
  char* path;
  char* mountpoint;
  size_t pathLength;
  size_t mountLength;
  uint8_t* data;
  size_t size;
  map_t lookup;
  arr_t(zip_node) nodes;
};

struct File {
  uint32_t ref;
  OpenMode mode;
  Handle handle;
  Archive* archive;
  char* path;
};

static struct {
  uint32_t ref;
  Archive* archives;
  size_t savePathLength;
  char savePath[1024];
  char source[1024];
  char requirePath[1024];
  char identity[64];
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

bool lovrFilesystemInit(void) {
  if (atomic_fetch_add(&state.ref, 1)) return false;

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

  return true;
}

void lovrFilesystemDestroy(void) {
  if (atomic_fetch_sub(&state.ref, 1) != 1) return;
  Archive* archive = state.archives;
  while (archive) {
    Archive* next = archive->next;
    lovrRelease(archive, lovrArchiveDestroy);
    archive = next;
  }
  memset(&state, 0, sizeof(state));
}

void lovrFilesystemSetSource(const char* source) {
  lovrCheck(!state.source[0], "Source is already set!");
  size_t length = strlen(source);
  lovrCheck(sizeof(state.source) > length, "Source is too long!");
  memcpy(state.source, source, length);
  state.source[length] = '\0';
}

const char* lovrFilesystemGetSource(void) {
  return state.source[0]  ? state.source : NULL;
}

bool lovrFilesystemIsFused(void) {
  char path[LOVR_PATH_MAX];
  const char* root;
  return lovrFilesystemGetBundlePath(path, sizeof(path), &root) && !strcmp(state.source, path);
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

static Archive* archiveStat(const char* p, FileInfo* info, bool needTime) {
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
  Archive* archive = archiveStat(path, &info, false);
  return archive ? archive->path : NULL;
}

bool lovrFilesystemIsFile(const char* path) {
  FileInfo info;
  return archiveStat(path, &info, false) && info.type == FILE_REGULAR;
}

bool lovrFilesystemIsDirectory(const char* path) {
  FileInfo info;
  return archiveStat(path, &info, false) && info.type == FILE_DIRECTORY;
}

uint64_t lovrFilesystemGetSize(const char* path) {
  FileInfo info;
  return archiveStat(path, &info, false) && info.type == FILE_REGULAR ? info.size : 0;
}

uint64_t lovrFilesystemGetLastModified(const char* path) {
  FileInfo info;
  return archiveStat(path, &info, true) ? info.lastModified : ~0ull;
}

void* lovrFilesystemRead(const char* p, size_t* size) {
  Handle handle;
  char path[1024];
  size_t length = sizeof(path);
  if (sanitize(p, path, &length)) {
    FOREACH_ARCHIVE(archive) {
      if (!archiveContains(archive, path, length)) {
        continue;
      }

      if (!archive->open(archive, path, &handle)) {
        continue;
      }

      uint64_t bytes;
      if (!archive->fsize(archive, &handle, &bytes) || bytes > SIZE_MAX) {
        archive->close(archive, &handle);
        continue;
      }

      *size = (size_t) bytes;
      void* data = lovrMalloc(*size);

      if (archive->read(archive, &handle, data, *size, size)) {
        archive->close(archive, &handle);
        return data;
      }

      archive->close(archive, &handle);
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
  state.savePath[cursor++] = SLASH;
  memcpy(state.savePath + cursor, "LOVR", strlen("LOVR"));
  cursor += strlen("LOVR");
  state.savePath[cursor] = '\0';
  fs_mkdir(state.savePath);

  // Append /<identity>, mkdir
  state.savePath[cursor++] = SLASH;
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
  if (!fs_open(resolved, append ? 'a' : 'w', &file)) {
    return false;
  }

  size_t count;
  if (!fs_write(file, content, size, &count) || count != size) {
    return false;
  }

  return fs_close(file);
}

// Paths

size_t lovrFilesystemGetAppdataDirectory(char* buffer, size_t size) {
  return os_get_data_directory(buffer, size);
}

size_t lovrFilesystemGetBundlePath(char* buffer, size_t size, const char** root) {
  return os_get_bundle_path(buffer, size, root);
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

static bool dir_resolve(Archive* archive, const char* fullpath, char* buffer) {
  const char* path = fullpath + (archive->mountLength ? archive->mountLength + 1 : 0);
  return concat(buffer, archive->path, archive->pathLength, path, strlen(path));
}

static bool dir_init(Archive* archive, const char* path, const char* root) {
  FileInfo info;
  return fs_stat(path, &info) && info.type == FILE_DIRECTORY;
}

static bool dir_open(Archive* archive, const char* path, Handle* handle) {
  char resolved[LOVR_PATH_MAX];
  return dir_resolve(archive, path, resolved) && fs_open(resolved, 'r', &handle->file);
}

static bool dir_close(Archive* archive, Handle* handle) {
  return fs_close(handle->file);
}

static bool dir_read(Archive* archive, Handle* handle, uint8_t* data, size_t size, size_t* count) {
  if (!fs_read(handle->file, data, size, count)) {
    return false;
  } else {
    handle->offset += *count;
    return true;
  }
}

static bool dir_seek(Archive* archive, Handle* handle, uint64_t offset) {
  if (fs_seek(handle->file, offset)) {
    handle->offset = offset;
    return true;
  }
  return false;
}

static bool dir_fsize(Archive* archive, Handle* handle, uint64_t* size) {
  FileInfo info;
  if (!fs_fstat(handle->file, &info)) {
    return false;
  } else {
    *size = info.size;
    return true;
  }
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

// Archive: zip

static uint16_t readu16(const uint8_t* p) { uint16_t x; memcpy(&x, p, sizeof(x)); return x; }
static uint32_t readu32(const uint8_t* p) { uint32_t x; memcpy(&x, p, sizeof(x)); return x; }

static void zip_free(Archive* archive) {
  arr_free(&archive->nodes);
  map_free(&archive->lookup);
  if (archive->data) fs_unmap(archive->data, archive->size);
}

static bool zip_init(Archive* archive, const char* filename, const char* root) {
  // Map the zip file into memory
  archive->data = fs_map(filename, &archive->size);
  if (!archive->data) {
    zip_free(archive);
    return false;
  }

  // Check the end of the file for the magic zip footer
  const uint8_t* p = archive->data + archive->size - 22;
  if (archive->size < 22 || readu32(p) != 0x06054b50) {
    zip_free(archive);
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
    zip_free(archive);
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
      zip_free(archive);
      return false;
    }

    base = centralDirectoryOffset - cursor;
    cursor = centralDirectoryOffset;

    // And if that didn't work, just give up
    if (readu32(archive->data + cursor) != 0x02014b50) {
      zip_free(archive);
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
      zip_free(archive);
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
    size_t length = readu16(p + 28);
    const char* path = (const char*) (p + 46);
    cursor += 46 + readu16(p + 28) + readu16(p + 30) + readu16(p + 32);

    // Sanity check the local file header
    uint64_t headerOffset = base + readu32(p + 42);
    uint8_t* header = archive->data + headerOffset;
    if (headerOffset > archive->size - 30 || readu32(header) != 0x04034b50) {
      zip_free(archive);
      return false;
    }

    // Filename and extra data are 30 bytes after the header, then the data starts
    uint64_t dataOffset = headerOffset + 30 + readu16(header + 26) + readu16(header + 28);
    node.data = archive->data + dataOffset;

    // Make sure data is actually contained in the zip
    if (dataOffset + node.compressedSize > archive->size) {
      zip_free(archive);
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

    // Skip files if their names are too long, too short, or not under the root
    if (length <= rootLength || (root && memcmp(path, root, rootLength) && path[rootLength] != '/')) {
      continue;
    }

    // Strip root
    if (root) {
      path += rootLength + 1;
      length -= rootLength + 1;
    }

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

      size_t end = length;
      while (length && path[length - 1] != '/') {
        length--;
      }

      archive->nodes.data[index].filename = path + length;
      archive->nodes.data[index].filenameLength = (uint16_t) (end - length);

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

static bool zip_open(Archive* archive, const char* path, Handle* handle) {
  handle->node = zip_resolve(archive, path);

  if (!handle->node || handle->node->directory) {
    return false;
  }

  if (handle->node->compressed) {
    zip_stream* stream = handle->stream = lovrMalloc(sizeof(zip_stream));
    tinfl_init(&stream->decompressor);
    stream->inputCursor = 0;
    stream->outputCursor = 0;
    stream->bufferExtent = 0;
  } else {
    handle->stream = NULL;
  }

  handle->offset = 0;
  return true;
}

static bool zip_close(Archive* archive, Handle* handle) {
  lovrFree(handle->stream);
  return true;
}

static bool decompress(zip_node* node, zip_stream* stream, uint8_t* data, size_t size, size_t* count) {
  if (size > 0 && stream->bufferExtent > 0) {
    lovrUnreachable(); // Data in the buffer must be copied out first!
  }

  while (size > 0) {
    uint8_t* input = (uint8_t*) node->data + stream->inputCursor;
    uint8_t* output = stream->buffer;
    size_t inSize = node->compressedSize - stream->inputCursor;
    size_t outSize = sizeof(stream->buffer);
    uint32_t flags = stream->outputCursor + outSize < node->uncompressedSize ? TINFL_FLAG_HAS_MORE_INPUT : 0;
    int status = tinfl_decompress(&stream->decompressor, input, &inSize, output, output, &outSize, flags);
    if (status < 0) return false;
    size_t n = MIN(outSize, size);
    if (data) memcpy(data, stream->buffer, n), data += n;
    stream->inputCursor += inSize;
    stream->outputCursor += n;
    stream->bufferExtent = sizeof(stream->buffer) - n;
    if (count) *count += n;
    size -= n;
  }

  return true;
}

static bool zip_read(Archive* archive, Handle* handle, uint8_t* data, size_t size, size_t* count) {
  zip_node* node = handle->node;
  zip_stream* stream = handle->stream;

  // EOF
  if (handle->offset >= node->uncompressedSize) {
    *count = 0;
    return true;
  }

  // Uncompressed reads are a simple memcpy
  if (!node->compressed) {
    *count = MIN(size, node->uncompressedSize - handle->offset);
    memcpy(data, (char*) node->data + handle->offset, *count);
    handle->offset += *count;
    return true;
  }

  // Whole-file compressed reads can use a simpler codepath
  if (handle->offset == 0 && size == node->uncompressedSize) {
    size_t inputSize = node->compressedSize;
    uint32_t flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
    int status = tinfl_decompress(&stream->decompressor, node->data, &inputSize, data, data, &size, flags);
    if (status != TINFL_STATUS_DONE) return false;
    stream->outputCursor = size;
    handle->offset = size;
    *count = size;
    return true;
  }

  // If the file seeked backwards, gotta rewind to the beginning
  if (stream->outputCursor > handle->offset) {
    tinfl_init(&stream->decompressor);
    stream->inputCursor = 0;
    stream->outputCursor = 0;
    stream->bufferExtent = 0;
  }

  // Decompress and throw away data until reaching the current seek position
  if (handle->offset > stream->outputCursor) {
    if (stream->bufferExtent > 0) {
      size_t n = MIN(stream->bufferExtent, handle->offset - stream->outputCursor);
      stream->outputCursor += n;
      stream->bufferExtent -= n;
    }

    if (!decompress(node, stream, NULL, handle->offset - stream->outputCursor, NULL)) {
      return false;
    }
  }

  size = MIN(size, node->uncompressedSize - handle->offset);
  *count = 0;

  // Use any remaining data from the buffer, if possible
  if (stream->bufferExtent > 0) {
    size_t n = MIN(stream->bufferExtent, size);
    size_t offset = sizeof(stream->buffer) - stream->bufferExtent;
    memcpy(data, stream->buffer + offset, n);
    stream->outputCursor += n;
    stream->bufferExtent -= n;
    handle->offset += n;
    *count += n;
    size -= n;
    data += n;
  }

  // Finally, decompress data in chunks and copy to the output until finished
  if (decompress(node, stream, data, size, count)) {
    handle->offset += size;
    return true;
  }

  return false;
}

static bool zip_seek(Archive* archive, Handle* handle, uint64_t offset) {
  handle->offset = offset;
  return true;
}

static bool zip_fsize(Archive* archive, Handle* handle, uint64_t* size) {
  zip_node* node = handle->node;
  *size = node->uncompressedSize;
  return true;
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

// Archive

Archive* lovrArchiveCreate(const char* path, const char* mountpoint, const char* root) {
  Archive* archive = lovrCalloc(sizeof(Archive));
  archive->ref = 1;

  if (dir_init(archive, path, root)) {
    archive->open = dir_open;
    archive->close = dir_close;
    archive->read = dir_read;
    archive->seek = dir_seek;
    archive->fsize = dir_fsize;
    archive->stat = dir_stat;
    archive->list = dir_list;
  } else if (zip_init(archive, path, root)) {
    archive->open = zip_open;
    archive->close = zip_close;
    archive->read = zip_read;
    archive->seek = zip_seek;
    archive->fsize = zip_fsize;
    archive->stat = zip_stat;
    archive->list = zip_list;
  } else {
    lovrFree(archive);
    return NULL;
  }

  if (mountpoint) {
    size_t length = strlen(mountpoint);
    archive->mountpoint = lovrMalloc(length + 1);
    archive->mountLength = normalize(mountpoint, length, archive->mountpoint);
  }

  archive->pathLength = strlen(path);
  archive->path = lovrMalloc(archive->pathLength + 1);
  memcpy(archive->path, path, archive->pathLength + 1);

  return archive;
}

void lovrArchiveDestroy(void* ref) {
  Archive* archive = ref;
  if (archive->data) zip_free(archive);
  lovrFree(archive->mountpoint);
  lovrFree(archive->path);
  lovrFree(archive);
}

// File

File* lovrFileCreate(const char* p, OpenMode mode, const char** error) {
  char path[1024];
  size_t length = sizeof(path);
  if (!sanitize(p, path, &length)) {
    return *error = "File path is too long or contains invalid characters", NULL;
  }

  Handle handle = { 0 };
  Archive* archive = NULL;

  if (mode == OPEN_READ) {
    FOREACH_ARCHIVE(a) {
      if (archiveContains(a, path, length)) {
        if (a->open(a, path, &handle)) {
          archive = a;
          break;
        }
      }
    }

    if (!archive) {
      return *error = "File does not exist", NULL;
    }
  } else {
    char fullpath[LOVR_PATH_MAX];
    if (!concat(fullpath, state.savePath, state.savePathLength, path, length)) {
      return *error = "File path is too long", NULL;
    }

    if (!fs_open(fullpath, mode == OPEN_APPEND ? 'a' : 'w', &handle.file)) {
      return *error = "Failed to open file for writing", NULL; // TODO better errors pl0x
    }
  }

  File* file = lovrCalloc(sizeof(File));
  file->ref = 1;
  file->mode = mode;
  file->handle = handle;
  file->archive = archive;
  lovrRetain(archive);

  file->path = lovrMalloc(length + 1);
  memcpy(file->path, path, length + 1);

  return file;
}

void lovrFileDestroy(void* ref) {
  File* file = ref;
  if (file->archive) file->archive->close(file->archive, &file->handle);
  lovrRelease(file->archive, lovrArchiveDestroy);
  lovrFree(file->path);
  lovrFree(file);
}

const char* lovrFileGetPath(File* file) {
  return file->path;
}

OpenMode lovrFileGetMode(File* file) {
  return file->mode;
}

uint64_t lovrFileGetSize(File* file) {
  uint64_t size;
  return file->archive->fsize(file->archive, &file->handle, &size) ? size : ~0ull;
}

bool lovrFileRead(File* file, void* data, size_t size, size_t* count) {
  lovrCheck(file->mode == OPEN_READ, "File was not opened for reading");
  return file->archive->read(file->archive, &file->handle, data, size, count);
}

bool lovrFileWrite(File* file, const void* data, size_t size, size_t* count) {
  lovrCheck(file->mode != OPEN_READ, "File was not opened for writing");
  return fs_write(file->handle.file, data, size, count);
}

bool lovrFileSeek(File* file, uint64_t offset) {
  return file->archive->seek(file->archive, &file->handle, offset);
}

uint64_t lovrFileTell(File* file) {
  return file->handle.offset;
}
