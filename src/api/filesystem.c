#include "api/lovr.h"
#include "filesystem/filesystem.h"
#include "filesystem/blob.h"
#include <stdlib.h>
#include <string.h>

// Returns a Blob, leaving stack unchanged.  The Blob must be released when finished.
Blob* luax_readblob(lua_State* L, int index, const char* debug) {
  if (lua_type(L, index) == LUA_TUSERDATA) {
    Blob* blob = luax_checktype(L, index, Blob);
    lovrRetain(&blob->ref);
    return blob;
  } else {
    const char* path = luaL_checkstring(L, index);

    size_t size;
    void* data = lovrFilesystemRead(path, &size);
    if (!data) {
      luaL_error(L, "Could not read %s from '%s'", debug, path);
    }

    return lovrBlobCreate(data, size, path);
  }
}

static void pushDirectoryItem(void* userdata, const char* path, const char* filename) {
  lua_State* L = userdata;
  int n = lua_objlen(L, -1);
  lua_pushstring(L, filename);
  lua_rawseti(L, -2, n + 1);
}

// Loader to help Lua's require understand PhysFS.
static int filesystemLoader(lua_State* L) {
  const char* module = luaL_checkstring(L, -1);
  char* dot;

  while ((dot = strchr(module, '.')) != NULL) {
    *dot = '/';
  }

  const char* requirePath[] = {
    "?.lua",
    "?/init.lua"
  };

  for (size_t i = 0; i < sizeof(requirePath) / sizeof(char*); i++) {
    char filename[LOVR_PATH_MAX];
    char* sub = strchr(requirePath[i], '?');

    memset(filename, 0, LOVR_PATH_MAX);

    if (sub) {
      int index = (int) (sub - requirePath[i]);
      strncat(filename, requirePath[i], index);
      strncat(filename, module, strlen(module));
      strncat(filename, requirePath[i] + index + 1, strlen(requirePath[i]) - index);

      if (lovrFilesystemIsFile(filename)) {
        size_t size;
        void* data = lovrFilesystemRead(filename, &size);
        char identifier[LOVR_PATH_MAX + 1];
        strncpy(identifier, "@", 2);
        strncat(identifier, filename, LOVR_PATH_MAX);

        if (data) {
          if (!luaL_loadbuffer(L, data, size, identifier)) {
            free(data);
            return 1;
          }

          free(data);
        }
      }
    }
  }

  return 0;
}

int l_lovrFilesystemInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrFilesystem);
  luax_registertype(L, "Blob", lovrBlob);

  lua_getglobal(L, "arg");
  lua_rawgeti(L, -1, -2);
  lua_rawgeti(L, -2, 1);
  const char* arg0 = lua_tostring(L, -2);
  const char* arg1 = lua_tostring(L, -1);
  lovrFilesystemInit(arg0, arg1);
  lua_pop(L, 3);

  // Add custom package loader
  lua_getglobal(L, "table");
  lua_getfield(L, -1, "insert");
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");
  lua_remove(L, -2);
  if (lua_istable(L, -1)) {
    lua_pushinteger(L, 2); // Insert our loader after package.preload
    lua_pushcfunction(L, filesystemLoader);
    lua_call(L, 3, 0);
  }

  lua_pop(L, 1);

  return 1;
}

int l_lovrFilesystemAppend(lua_State* L) {
  size_t size;
  const char* path = luaL_checkstring(L, 1);
  const char* content = luaL_checklstring(L, 2, &size);
  lua_pushnumber(L, lovrFilesystemWrite(path, content, size, 1));
  return 1;
}

int l_lovrFilesystemCreateDirectory(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, !lovrFilesystemCreateDirectory(path));
  return 1;
}

int l_lovrFilesystemExists(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemExists(path));
  return 1;
}

int l_lovrFilesystemGetAppdataDirectory(lua_State* L) {
  char buffer[1024];

  if (lovrFilesystemGetAppdataDirectory(buffer, sizeof(buffer))) {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, buffer);
  }

  return 1;
}

int l_lovrFilesystemGetDirectoryItems(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_newtable(L);
  lovrFilesystemGetDirectoryItems(path, pushDirectoryItem, L);
  return 1;
}

int l_lovrFilesystemGetExecutablePath(lua_State* L) {
  char buffer[1024];

  if (lovrFilesystemGetExecutablePath(buffer, sizeof(buffer))) {
    lua_pushnil(L);
  } else {
    lua_pushstring(L, buffer);
  }

  return 1;
}

int l_lovrFilesystemGetIdentity(lua_State* L) {
  const char* identity = lovrFilesystemGetIdentity();
  if (identity) {
    lua_pushstring(L, identity);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrFilesystemGetLastModified(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  int lastModified = lovrFilesystemGetLastModified(path);

  if (lastModified < 0) {
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, lastModified);
  }

  return 1;
}

int l_lovrFilesystemGetRealDirectory(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushstring(L, lovrFilesystemGetRealDirectory(path));
  return 1;
}

int l_lovrFilesystemGetSaveDirectory(lua_State* L) {
  lua_pushstring(L, lovrFilesystemGetSaveDirectory());
  return 1;
}

int l_lovrFilesystemGetSize(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushinteger(L, lovrFilesystemGetSize(path));
  return 1;
}

int l_lovrFilesystemGetSource(lua_State* L) {
  const char* source = lovrFilesystemGetSource();

  if (source) {
    lua_pushstring(L, source);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int l_lovrFilesystemGetUserDirectory(lua_State* L) {
  lua_pushstring(L, lovrFilesystemGetUserDirectory());
  return 1;
}

int l_lovrFilesystemIsDirectory(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemIsDirectory(path));
  return 1;
}

int l_lovrFilesystemIsFile(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemIsFile(path));
  return 1;
}

int l_lovrFilesystemIsFused(lua_State* L) {
  lua_pushboolean(L, lovrFilesystemIsFused());
  return 1;
}

int l_lovrFilesystemLoad(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  size_t size;
  char* content = lovrFilesystemRead(path, &size);

  int status = luaL_loadbuffer(L, content, size, path);
  switch (status) {
    case LUA_ERRMEM: return luaL_error(L, "Memory allocation error: %s", lua_tostring(L, -1));
    case LUA_ERRSYNTAX: return luaL_error(L, "Syntax error: %s", lua_tostring(L, -1));
    default: return 1;
  }
}

int l_lovrFilesystemMount(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  const char* mountpoint = luaL_optstring(L, 2, NULL);
  int append = lua_isnoneornil(L, 3) ? 0 : lua_toboolean(L, 3);
  lua_pushboolean(L, !lovrFilesystemMount(path, mountpoint, append));
  return 1;
}

int l_lovrFilesystemNewBlob(lua_State* L) {
  const char* path;
  char* data;
  size_t size;

  if (lua_gettop(L) == 1) {
    path = luaL_checkstring(L, 1);
    data = lovrFilesystemRead(path, &size);
  } else {
    const char* str = luaL_checklstring(L, 1, &size);
    data = malloc(size + 1); // Copy the Lua string so we can hold onto it
    memcpy(data, str, size);
    data[size] = '\0';
    path = luaL_checkstring(L, 2);
  }

  Blob* blob = lovrBlobCreate((void*) data, size, path);
  luax_pushtype(L, Blob, blob);
  lovrRelease(&blob->ref);
  return 1;
}

int l_lovrFilesystemRead(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  size_t size;
  char* content = lovrFilesystemRead(path, &size);
  if (!content) {
    return luaL_error(L, "Could not read file '%s'", path);
  }

  lua_pushlstring(L, content, size);
  free(content);
  return 1;
}

int l_lovrFilesystemRemove(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, !lovrFilesystemRemove(path));
  return 1;
}

int l_lovrFilesystemSetIdentity(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrFilesystemSetIdentity(NULL);
  } else {
    const char* identity = luaL_checkstring(L, 1);
    lovrFilesystemSetIdentity(identity);
  }
  return 0;
}

int l_lovrFilesystemUnmount(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, !lovrFilesystemUnmount(path));
  return 1;
}

int l_lovrFilesystemWrite(lua_State* L) {
  size_t size;
  const char* path = luaL_checkstring(L, 1);
  const char* content = luaL_checklstring(L, 2, &size);
  lua_pushnumber(L, lovrFilesystemWrite(path, content, size, 0));
  return 1;
}

const luaL_Reg lovrFilesystem[] = {
  { "append", l_lovrFilesystemAppend },
  { "createDirectory", l_lovrFilesystemCreateDirectory },
  { "exists", l_lovrFilesystemExists },
  { "getAppdataDirectory", l_lovrFilesystemGetAppdataDirectory },
  { "getDirectoryItems", l_lovrFilesystemGetDirectoryItems },
  { "getExecutablePath", l_lovrFilesystemGetExecutablePath },
  { "getIdentity", l_lovrFilesystemGetIdentity },
  { "getLastModified", l_lovrFilesystemGetLastModified },
  { "getRealDirectory", l_lovrFilesystemGetRealDirectory },
  { "getSaveDirectory", l_lovrFilesystemGetSaveDirectory },
  { "getSize", l_lovrFilesystemGetSize },
  { "getSource", l_lovrFilesystemGetSource },
  { "getUserDirectory", l_lovrFilesystemGetUserDirectory },
  { "isDirectory", l_lovrFilesystemIsDirectory },
  { "isFile", l_lovrFilesystemIsFile },
  { "isFused", l_lovrFilesystemIsFused },
  { "load", l_lovrFilesystemLoad },
  { "mount", l_lovrFilesystemMount },
  { "newBlob", l_lovrFilesystemNewBlob },
  { "read", l_lovrFilesystemRead },
  { "remove", l_lovrFilesystemRemove },
  { "setIdentity", l_lovrFilesystemSetIdentity },
  { "write", l_lovrFilesystemWrite },
  { NULL, NULL }
};
