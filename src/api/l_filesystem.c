#include "api.h"
#include "filesystem/filesystem.h"
#include "data/blob.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

StringEntry lovrFileAction[] = {
  [FILE_CREATE] = ENTRY("create"),
  [FILE_DELETE] = ENTRY("delete"),
  [FILE_MODIFY] = ENTRY("modify"),
  [FILE_RENAME] = ENTRY("rename"),
  { 0 }
};

StringEntry lovrOpenMode[] = {
  [OPEN_READ] = ENTRY("r"),
  [OPEN_WRITE] = ENTRY("w"),
  [OPEN_APPEND] = ENTRY("a"),
  { 0 }
};

void* luax_readfile(const char* filename, size_t* bytesRead) {
  return lovrFilesystemRead(filename, bytesRead);
}

bool luax_writefile(const char* filename, const void* data, size_t size) {
  return lovrFilesystemWrite(filename, data, size, false);
}

// Returns a Blob, leaving stack unchanged.  The Blob must be released when finished.
Blob* luax_readblob(lua_State* L, int index, const char* debug) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    Blob* blob = luax_checktype(L, index, Blob);
    lovrRetain(blob);
    return blob;
  } else {
    const char* path = luaL_checkstring(L, index);

    size_t size;
    void* data = luax_readfile(path, &size);
    if (!data) {
      luaL_error(L, "Could not read %s from '%s'", debug, path);
    }

    return lovrBlobCreate(data, size, path);
  }
}

static void pushDirectoryItem(void* context, const char* path) {
  lua_State* L = context;

  if (path[0] == '.' && (path[1] == '\0' || (path[1] == '.' && path[2] == '\0'))) {
    return;
  }

  lua_pushstring(L, path);
  lua_rawget(L, 2);

  if (lua_isnil(L, -1)) {

    // t[path] = true
    lua_pushstring(L, path);
    lua_pushboolean(L, true);
    lua_rawset(L, 2);

    // t[#t + 1] = path
    int n = luax_len(L, 2);
    lua_pushstring(L, path);
    lua_rawseti(L, 2, n + 1);
  }

  lua_pop(L, 1);
}

static int luax_loadfile(lua_State* L, const char* path, const char* debug, const char* mode) {
  size_t size;
  void* buffer = luax_readfile(path, &size);
  if (!buffer) {
    lua_pushnil(L);
    lua_pushfstring(L, "Could not read file '%s': %s", path, lovrGetError());
    return 2;
  }
  int status = luax_loadbufferx(L, buffer, size, debug, mode);
  lovrFree(buffer);
  switch (status) {
    case LUA_ERRMEM: return luaL_error(L, "Memory allocation error: %s", lua_tostring(L, -1)), 0;
    case LUA_ERRSYNTAX: return luaL_error(L, "Syntax error: %s", lua_tostring(L, -1)), 0;
    default: return 1;
  }
}

static int l_lovrFilesystemAppend(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);

  size_t size;
  const char* data;
  Blob* blob = luax_totype(L, 2, Blob);
  if (blob) {
    data = blob->data;
    size = blob->size;
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    data = lua_tolstring(L, 2, &size);
  } else {
    return luax_typeerror(L, 2, "string or Blob");
  }

  return luax_pushsuccess(L, lovrFilesystemWrite(path, data, size, true));
}

static int l_lovrFilesystemCreateDirectory(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  return luax_pushsuccess(L, lovrFilesystemCreateDirectory(path));
}

static int l_lovrFilesystemGetAppdataDirectory(lua_State* L) {
  char buffer[LOVR_PATH_MAX];

  if (lovrFilesystemGetAppdataDirectory(buffer, sizeof(buffer))) {
    lua_pushstring(L, buffer);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int l_lovrFilesystemGetDirectoryItems(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_settop(L, 1);
  lua_newtable(L);
  lovrFilesystemGetDirectoryItems(path, pushDirectoryItem, L);

  // Remove all string keys (used for deduplication)
  lua_pushnil(L);
  while (lua_next(L, 2)) {
    lua_pop(L, 1);
    if (lua_type(L, -1) == LUA_TSTRING) {
      lua_pushvalue(L, -1);
      lua_pushnil(L);
      lua_rawset(L, 2);
    }
  }

  lua_getglobal(L, "table");
  if (lua_type(L, -1) == LUA_TTABLE) {
    lua_getfield(L, -1, "sort");
    if (lua_type(L, -1) == LUA_TFUNCTION) {
      lua_pushvalue(L, 2);
      lua_call(L, 1, 0);
    } else {
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  return 1;
}

static int l_lovrFilesystemGetBundlePath(lua_State* L) {
  char buffer[LOVR_PATH_MAX];
  const char* root;

  if (lovrFilesystemGetBundlePath(buffer, sizeof(buffer), &root)) {
    lua_pushstring(L, buffer);
    lua_pushstring(L, root);
  } else {
    lua_pushnil(L);
    lua_pushnil(L);
  }

  return 2;
}

static int l_lovrFilesystemGetExecutablePath(lua_State* L) {
  char buffer[LOVR_PATH_MAX];

  if (lovrFilesystemGetExecutablePath(buffer, sizeof(buffer))) {
    lua_pushstring(L, buffer);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int l_lovrFilesystemGetIdentity(lua_State* L) {
  const char* identity = lovrFilesystemGetIdentity();
  if (identity) {
    lua_pushstring(L, identity);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrFilesystemGetLastModified(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  uint64_t modtime;
  bool success = lovrFilesystemGetLastModified(path, &modtime);
  if (!success) return luax_pushnilerror(L);
  lua_pushinteger(L, modtime);
  return 1;
}

static int l_lovrFilesystemGetRealDirectory(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushstring(L, lovrFilesystemGetRealDirectory(path));
  return 1;
}

static int l_lovrFilesystemGetRequirePath(lua_State* L) {
  lua_pushstring(L, lovrFilesystemGetRequirePath());
  return 1;
}

static int l_lovrFilesystemGetSaveDirectory(lua_State* L) {
  lua_pushstring(L, lovrFilesystemGetSaveDirectory());
  return 1;
}

static int l_lovrFilesystemGetSize(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  uint64_t size;
  bool success = lovrFilesystemGetSize(path, &size);
  if (!success) return luax_pushnilerror(L);
  lua_pushinteger(L, size);
  return 1;
}

static int l_lovrFilesystemGetSource(lua_State* L) {
  const char* source = lovrFilesystemGetSource();

  if (source) {
    lua_pushstring(L, source);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int l_lovrFilesystemGetUserDirectory(lua_State* L) {
  char buffer[LOVR_PATH_MAX];

  if (lovrFilesystemGetUserDirectory(buffer, sizeof(buffer))) {
    lua_pushstring(L, buffer);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int l_lovrFilesystemGetWorkingDirectory(lua_State* L) {
  char buffer[LOVR_PATH_MAX];

  if (lovrFilesystemGetWorkingDirectory(buffer, sizeof(buffer))) {
    lua_pushstring(L, buffer);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static int l_lovrFilesystemIsDirectory(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemIsDirectory(path));
  return 1;
}

static int l_lovrFilesystemIsFile(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemIsFile(path));
  return 1;
}

static int l_lovrFilesystemIsFused(lua_State* L) {
  lua_pushboolean(L, lovrFilesystemIsFused());
  return 1;
}

static int l_lovrFilesystemLoad(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  const char* mode = luaL_optstring(L, 2, "bt");
  lua_pushfstring(L, "@%s", path);
  const char* debug = lua_tostring(L, -1);
  return luax_loadfile(L, path, debug, mode);
}

static int l_lovrFilesystemMount(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  const char* mountpoint = luaL_optstring(L, 2, NULL);
  bool append = lua_toboolean(L, 3);
  const char* root = luaL_optstring(L, 4, NULL);
  return luax_pushsuccess(L, lovrFilesystemMount(path, mountpoint, append, root));
}

static int l_lovrFilesystemNewBlob(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  size_t size;
  void* data = luax_readfile(path, &size);
  if (!data) return luax_pushnilerror(L);
  Blob* blob = lovrBlobCreate(data, size, path);
  luax_pushtype(L, Blob, blob);
  lovrRelease(blob, lovrBlobDestroy);
  return 1;
}

static int l_lovrFilesystemRead(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);

  size_t size;
  void* data = lovrFilesystemRead(path, &size);
  if (!data) return luax_pushnilerror(L);
  lua_pushlstring(L, data, size);
  lovrFree(data);
  return 1;
}

static int l_lovrFilesystemRemove(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  return luax_pushsuccess(L, lovrFilesystemRemove(path));
}

static int l_lovrFilesystemSetIdentity(lua_State* L) {
  const char* identity = luaL_checkstring(L, 1);
  bool precedence = lua_toboolean(L, 2);
  luax_assert(L, lovrFilesystemSetIdentity(identity, precedence));
  return 0;
}

static int l_lovrFilesystemSetRequirePath(lua_State* L) {
  lovrFilesystemSetRequirePath(luaL_checkstring(L, 1));
  return 0;
}

static int l_lovrFilesystemSetSource(lua_State* L) {
  const char* source = luaL_checkstring(L, 1);
  luax_assert(L, lovrFilesystemSetSource(source));
  return 0;
}

static int l_lovrFilesystemUnmount(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemUnmount(path));
  return 1;
}

static int l_lovrFilesystemUnwatch(lua_State* L) {
  lovrFilesystemUnwatch();
  return 0;
}

static int l_lovrFilesystemWatch(lua_State* L) {
  lovrFilesystemWatch();
  return 0;
}

static int l_lovrFilesystemWrite(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  size_t size;
  const char* data;
  Blob* blob = luax_totype(L, 2, Blob);
  if (blob) {
    data = blob->data;
    size = blob->size;
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    data = lua_tolstring(L, 2, &size);
  } else {
    return luax_typeerror(L, 2, "string or Blob");
  }
  return luax_pushsuccess(L, lovrFilesystemWrite(path, data, size, false));
}

static int l_lovrFilesystemNewFile(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  OpenMode mode = luax_checkenum(L, 2, OpenMode, NULL);
  File* file = lovrFileCreate(path, mode);
  if (!file) return luax_pushnilerror(L);
  luax_pushtype(L, File, file);
  lovrRelease(file, lovrFileDestroy);
  return 1;
}

static const luaL_Reg lovrFilesystem[] = {
  { "append", l_lovrFilesystemAppend },
  { "createDirectory", l_lovrFilesystemCreateDirectory },
  { "getAppdataDirectory", l_lovrFilesystemGetAppdataDirectory },
  { "getBundlePath", l_lovrFilesystemGetBundlePath },
  { "getDirectoryItems", l_lovrFilesystemGetDirectoryItems },
  { "getExecutablePath", l_lovrFilesystemGetExecutablePath },
  { "getIdentity", l_lovrFilesystemGetIdentity },
  { "getLastModified", l_lovrFilesystemGetLastModified },
  { "getRealDirectory", l_lovrFilesystemGetRealDirectory },
  { "getRequirePath", l_lovrFilesystemGetRequirePath },
  { "getSaveDirectory", l_lovrFilesystemGetSaveDirectory },
  { "getSize", l_lovrFilesystemGetSize },
  { "getSource", l_lovrFilesystemGetSource },
  { "getUserDirectory", l_lovrFilesystemGetUserDirectory },
  { "getWorkingDirectory", l_lovrFilesystemGetWorkingDirectory },
  { "isDirectory", l_lovrFilesystemIsDirectory },
  { "isFile", l_lovrFilesystemIsFile },
  { "isFused", l_lovrFilesystemIsFused },
  { "load", l_lovrFilesystemLoad },
  { "mount", l_lovrFilesystemMount },
  { "newBlob", l_lovrFilesystemNewBlob },
  { "read", l_lovrFilesystemRead },
  { "remove", l_lovrFilesystemRemove },
  { "setIdentity", l_lovrFilesystemSetIdentity },
  { "setRequirePath", l_lovrFilesystemSetRequirePath },
  { "setSource", l_lovrFilesystemSetSource },
  { "unmount", l_lovrFilesystemUnmount },
  { "unwatch", l_lovrFilesystemUnwatch },
  { "watch", l_lovrFilesystemWatch },
  { "write", l_lovrFilesystemWrite },
  { "newFile", l_lovrFilesystemNewFile },
  { NULL, NULL }
};

static int luaLoader(lua_State* L) {
  const char* module = lua_tostring(L, 1);
  const char* p = lovrFilesystemGetRequirePath();

  char buffer[1024] = { '@' };
  char* debug = &buffer[0];
  char* filename = &buffer[1];
  char* f = filename;
  size_t n = sizeof(buffer) - 1;

  // Loop over the require path, character by character, and:
  //   - Replace question marks with the module that's being required, converting '.' to '/'.
  //   - If there's a semicolon/eof, treat it as the end of a potential filename and try to load it.
  // The filename buffer has an '@' before it so it can also be used as the debug label for the Lua
  // chunk without additional memory allocation.

  while (1) {
    if (*p == ';' || *p == '\0') {
      *f = '\0';

      if (lovrFilesystemIsFile(filename)) {
        return luax_loadfile(L, filename, debug, NULL);
      }

      if (*p == '\0') {
        break;
      } else {
        p++;
        f = filename;
        n = sizeof(buffer) - 1;
      }
    } else if (*p == '?') {
      for (const char* m = module; n && *m; n--, m++) {
        *f++ = *m == '.' ? '/' : *m;
      }
      p++;
    } else {
      *f++ = *p++;
      n--;
    }

    luax_check(L, n > 0, "Tried to require a filename that was too long (%s)", module);
  }

  return 0;
}

#ifdef __ANDROID__
#include <jni.h>
#include <dlfcn.h>
typedef jint fn_JNI_OnLoad(JavaVM* vm, void* reserved);
extern void* os_get_java_vm();
#endif

static int libLoaderCommon(lua_State* L, bool allInOneFlag) {
#ifdef _WIN32
  const char* extension = ".dll";
  const char sep = '\\';
#else
  const char* extension = ".so";
  const char sep = '/';
#endif

  const char* module = lua_tostring(L, 1);
  const char* hyphen = strchr(module, '-');
  const char* symbol = hyphen ? hyphen + 1 : module;

  char path[1024];

  // On Android, load libraries directly from the apk by passing a path like this to the linker:
  //   /path/to/app.apk!/lib/arm64-v8a/lib.so
  // On desktop systems, look for libraries next to the executable
#ifdef __ANDROID__
  const char* source = lovrFilesystemGetSource();
  size_t length = strlen(source);
  memcpy(path, source, length);

  const char* subpath = "!/lib/arm64-v8a/";
  size_t subpathLength = strlen(subpath);
  char* p = path + length;
  if (length + subpathLength >= sizeof(path)) {
    return 0;
  }

  memcpy(p, subpath, subpathLength);
  length += subpathLength;
  p += subpathLength;
#else
  size_t length = lovrFilesystemGetExecutablePath(path, sizeof(path));
  if (length == 0) {
    return 0;
  }

  char* slash = strrchr(path, sep);
  char* p = slash ? slash + 1 : path;
  length = p - path;
#endif

  for (const char* m = module; *m && length < sizeof(path); m++, length++) {
    if (allInOneFlag && *m == '.') break;
    *p++ = *m == '.' ? sep : *m;
  }

  for (const char* e = extension; *e && length < sizeof(path); e++, length++) {
    *p++ = *e;
  }

  if (length >= sizeof(path)) {
    return 0;
  }

  *p = '\0';

#ifdef __ANDROID__
  // This is very appropriately cursed, but on Android (before API level 31) there is no way for a
  // plugin to retrieve a pointer to the Java VM.  Normally there is a JNI_OnLoad callback that Java
  // will call when a native library is loaded to give it the JVM pointer, but native libraries
  // loaded by other native libraries won't have it called and so it needs to be shared somehow.
  // Plugins can't dlsym os_get_java_vm or similar because liblovr.so is inside the APK and there's
  // no way to get the path to the APK without JNI.  Also it's not possible to load liblovr.so with
  // RTLD_GLOBAL which would expose symbols via RTLD_DEFAULT.  The chosen solution is to emulate
  // JNI_OnLoad for LÖVR plugins (before they're loaded by Lua so they can use JNI in luaopen_*).
  void* plugin = dlopen(path, RTLD_LAZY);
  if (plugin) {
    fn_JNI_OnLoad* JNI_OnLoad = (fn_JNI_OnLoad*) dlsym(plugin, "JNI_OnLoad");
    if (JNI_OnLoad) {
      JNI_OnLoad(os_get_java_vm(), NULL);
    } else {
      dlclose(plugin);
      plugin = NULL;
    }
  }
#endif

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loadlib");
  lua_pushlstring(L, path, length);

  // Synthesize luaopen_<module> symbol
  luaL_Buffer buffer;
  luaL_buffinit(L, &buffer);
  luaL_addstring(&buffer, "luaopen_");
  for (const char* s = symbol; *s; s++) {
    luaL_addchar(&buffer, *s == '.' ? '_' : *s);
  }
  luaL_pushresult(&buffer);

  lua_call(L, 2, 1);

#ifdef __ANDROID__
  if (plugin) {
    dlclose(plugin);
  }
#endif
  return 1;
}

static int libLoader(lua_State* L) {
  bool allInOneFlag = false;
  return libLoaderCommon(L, allInOneFlag);
}

static int libLoaderAllInOne(lua_State* L) {
  bool allInOneFlag = true;
  return libLoaderCommon(L, allInOneFlag);
}

extern const luaL_Reg lovrFile[];

int luaopen_lovr_filesystem(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrFilesystem);
  luax_registertype(L, File);
  luax_registerloader(L, luaLoader, 2);
  luax_registerloader(L, libLoader, 3);
  luax_registerloader(L, libLoaderAllInOne, 4);
  luax_assert(L, lovrFilesystemInit());
  luax_atexit(L, lovrFilesystemDestroy);
  return 1;
}
