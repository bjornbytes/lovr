#include "filesystem/filesystem.h"
#include "core/arr.h"
#include "core/fs.h"
#include "core/hash.h"
#include "core/map.h"
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
  size_t length1 = strlen(p1);
  size_t length2 = strlen(p2);
  if (length1 + 1 + length2 >= LOVR_PATH_MAX) {
    return false;
  }
  memcpy(buffer, p1, length1);
  buffer[length1] = '/';
  memcpy(buffer + length1 + 1, p2, length2);
  buffer[length1 + 1 + length2] = '\0';
  return true;
}

typedef struct {
  uint32_t firstChild;
  uint32_t nextSibling;
  uint64_t offset;
  FileInfo info;
  char filename[1024];
} zip_node;

typedef struct Archive {
  struct Archive* next;
  fs_handle file;
  arr_t(zip_node) nodes;
  map_t lookup;
  bool (*stat)(struct Archive* archive, const char* path, FileInfo* info);
  void (*list)(struct Archive* archive, const char* path, void (*callback)(void* context, char* path), void* context);
  char path[FS_PATH_MAX];
  char mountpoint[64];
  char root[64];
} Archive;

// Archive: directory

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

bool dir_init(Archive* archive, const char* path) {
  FileInfo info;
  if (fs_stat(path, &info) && info.type == FILE_DIRECTORY) {
    archive->stat = dir_stat;
    archive->list = dir_list;
    return true;
  }

  return false;
}

// Archive: zip

#define ZIP_HEADER_SIZE 22
#define ZIP_ENTRY_SIZE 46
#define ZIP_HEADER_MAGIC 0x06054b50
#define ZIP_ENTRY_MAGIC 0x02014b50

typedef struct {
  uint32_t magic;
  uint16_t disk;
  uint16_t startDisk;
  uint16_t localEntryCount;
  uint16_t totalEntryCount;
  uint32_t centralDirectorySize;
  uint32_t centralDirectoryOffset;
  uint16_t commentLength;
} zip_header;

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t minimumVersion;
  uint16_t flags;
  uint16_t compression;
  uint16_t lastModifiedTime;
  uint16_t lastModifiedDate;
  uint32_t crc32;
  uint32_t compressedSize;
  uint32_t size;
  uint16_t nameLength;
  uint16_t extraLength;
  uint16_t commentLength;
  uint16_t disk;
  uint16_t internalAttributes;
  uint16_t externalAttributes;
  uint32_t offset;
} zip_entry;

bool zip_stat(Archive* archive, const char* path, FileInfo *info) {
  uint64_t hash = hash64(path, strlen(path));
  uint64_t index = map_get(&archive->lookup, hash);

  if (index == MAP_NIL) {
    return false;
  }

  *info = archive->nodes.data[index].info;
  return true;
}

void zip_list(Archive* archive, const char* path, fs_list_cb callback, void* context) {
  uint64_t hash = hash64(path, strlen(path));
  uint64_t index = map_get(&archive->lookup, hash);

  if (index == MAP_NIL) {
    return;
  }

  zip_node* dir = &archive->nodes.data[index];
  uint32_t i = dir->firstChild;
  while (i != ~0u) {
    zip_node* child = &archive->nodes.data[i];
    callback(context, child->filename);
    i = child->nextSibling;
  }
}

bool zip_init(Archive* archive, const char* path) {
  if (!fs_open(path, OPEN_READ, &archive->file)) {
    return false;
  }

  // Seek to the end of the file, minus the header size
  if (!fs_seek(archive->file, -ZIP_HEADER_SIZE, FROM_END)) {
    return false;
  }

  size_t size = ZIP_HEADER_SIZE;
  uint8_t headerData[ZIP_HEADER_SIZE];
  zip_header* header = (zip_header*) headerData;

  // Read the header
  if (!fs_read(archive->file, headerData, &size) || size != sizeof(headerData)) {
    return false;
  }

  // "Validate"
  if (header->magic != ZIP_HEADER_MAGIC) {
    return false;
  }

  // Seek to the entry list
  if (!fs_seek(archive->file, header->centralDirectoryOffset, FROM_START)) {
    return false;
  }

  // Data structure memory
  arr_init(&archive->nodes);
  arr_reserve(&archive->nodes, header->totalEntryCount);
  map_init(&archive->lookup, header->totalEntryCount);

  // Process entries
  for (uint32_t i = 0; i < header->totalEntryCount; i++) {
    size_t size = ZIP_ENTRY_SIZE;
    uint8_t entryData[ZIP_ENTRY_SIZE];
    zip_entry* entry = (zip_entry*) entryData;

    // Read the metadata
    if (!fs_read(archive->file, entryData, &size) || size != sizeof(entryData)) {
      return false;
    }

    // "Validate"
    if (entry->magic != ZIP_ENTRY_MAGIC || entry->nameLength >= FS_PATH_MAX) {
      return false;
    }

    // Create node
    zip_node node = {
      .firstChild = ~0u,
      .nextSibling = ~0u,
      .offset = entry->offset,
      .info.size = entry->size,
      // TODO info.lastmodified
      // TODO info.type
    };

    // Read filename
    size_t length = entry->nameLength;
    if (!fs_read(archive->file, node.filename, &length)) {
      return false;
    }

    // Filenames that end in slashes are directories, but should have trailing slash stripped
    if (node.filename[length - 1] == '/') {
      node.info.type = FILE_DIRECTORY;
      length--;
    }

    node.filename[length] = '\0';

    // Add node to lookups if not already added by a previous ancestor add
    uint64_t hash = hash64(node.filename, length);
    uint64_t child = map_get(&archive->lookup, hash);

    if (child == MAP_NIL) {
      child = archive->nodes.length;
      map_set(&archive->lookup, hash, child);
      arr_push(&archive->nodes, node);
    } else if (node.info.type != FILE_DIRECTORY) {
      return false; // Only directories should be able to get indexed twice
    }

    // Update directory tree adding ancestors, iterating backwards to allow for early out
    for (char* s = node.filename + length; s-- > node.filename;) {
      bool root = (s == node.filename);

      if (*s == '/' || root) {
        uint64_t hash = root ? hash64("/", 1) : hash64(node.filename, s - node.filename);
        uint64_t parent = map_get(&archive->lookup, hash);

        // If we encounter a parent, then we can add ourselves as a child of that parent and exit
        if (parent != MAP_NIL) {
          archive->nodes.data[child].nextSibling = archive->nodes.data[parent].firstChild;
          archive->nodes.data[parent].firstChild = child;
          break;
        }

        // Otherwise, index the parent directory and recurse
        parent = archive->nodes.length;
        map_set(&archive->lookup, hash, parent);
        arr_push(&archive->nodes, ((zip_node) {
          .firstChild = child,
          .nextSibling = ~0u,
          .offset = ~0u,
          .info.size = 0,
          .info.type = FILE_DIRECTORY
        }));

        child = parent;
      }
    }

    // Seek to next entry, skipping over extra/comment strings
    if (!fs_seek(archive->file, entry->extraLength + entry->commentLength, FROM_CURRENT)) {
      return false;
    }
  }

  archive->stat = zip_stat;
  archive->list = zip_list;
  return true;
}

// Filesystem module

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

bool lovrFilesystemGetApplicationId(char* buffer, size_t size) {
  return false;
}

bool lovrFilesystemGetAppdataDirectory(char* buffer, size_t size) {
  return fs_getDataDirectory(buffer, size);
}

bool lovrFilesystemGetExecutablePath(char* buffer, size_t size) {
  return fs_getExecutablePath(buffer, size);
}

bool lovrFilesystemGetUserDirectory(char* buffer, size_t size) {
  return fs_getUserDirectory(buffer, size);
}

bool lovrFilesystemGetWorkingDirectory(char* buffer, size_t size) {
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

  lovrAssert(state.waitlist, "Too many mounted archives (up to %d are supported)", MAX_ARCHIVES);
  Archive* archive = state.waitlist;

  if (!(dir_init(archive, path) || zip_init(archive, path))) {
    return false;
  }

  state.waitlist = state.waitlist->next;
  memcpy(archive->path, path, length);
  archive->path[length] = '\0';
  // TODO mountpoint
  // TODO root

  if (append) {
    archive->next = NULL;
    if (state.archives) {
      Archive* tail = state.archives;
      while (tail->next) tail = tail->next;
      tail->next = archive;
    } else {
      state.archives = archive;
    }
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
      if (archive->stat(archive, path, &info)) {
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
      if (archive->stat(archive, path, &info)) {
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
      if (archive->stat(archive, path, &info)) {
        return info.type == FILE_DIRECTORY;
      }
    }
  }
  return false;
}

uint64_t lovrFilesystemGetSize(const char* path) {
  if (validate(path)) {
    FileInfo info;
    FOREACH_ARCHIVE(archive) {
      if (archive->stat(archive, path, &info)) {
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
      if (archive->stat(archive, path, &info)) {
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

// TODO dedupe / sort (or use Lua for it)
void lovrFilesystemGetDirectoryItems(const char* path, void (*callback)(void* context, char* path), void* context) {
  if (validate(path)) {
    FOREACH_ARCHIVE(archive) {
      archive->list(archive, path, callback, context);
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
