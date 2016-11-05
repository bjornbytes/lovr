#include "filesystem.h"
#include "../filesystem/filesystem.h"
#include <stdlib.h>
#include <string.h>

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

  for (int i = 0; i < sizeof(requirePath) / sizeof(char*); i++) {
    char filename[256];
    char* sub = strchr(requirePath[i], '?');

    memset(filename, 0, 256);

    if (sub) {
      int index = (int) (sub - requirePath[i]);
      strncat(filename, requirePath[i], index);
      strncat(filename, module, strlen(module));
      strncat(filename, requirePath[i] + index + 1, strlen(requirePath[i]) - index);

      if (lovrFilesystemIsFile(filename)) {
        int size;
        void* data = lovrFilesystemRead(filename, &size);

        if (data) {
          if (!luaL_loadbuffer(L, data, size, filename)) {
            return 1;
          }

          free(data);
        }
      }
    }
  }

  return 0;
}

const luaL_Reg lovrFilesystem[] = {
  { "init", l_lovrFilesystemInitialize },
  { "exists", l_lovrFilesystemExists },
  { "getExecutablePath", l_lovrFilesystemGetExecutablePath },
  { "isDirectory", l_lovrFilesystemIsDirectory },
  { "isFile", l_lovrFilesystemIsFile },
  { "read", l_lovrFilesystemRead },
  { "setSource", l_lovrFilesystemSetSource },
  { "write", l_lovrFilesystemWrite },
  { NULL, NULL }
};

int l_lovrFilesystemInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrFilesystem);

  // Add custom package loader
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");
  if (lua_istable(L, -1)) {
    lua_pushcfunction(L, filesystemLoader);
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
  }

  lua_pop(L, 2);

  return 1;
}

int l_lovrFilesystemInitialize(lua_State* L) {
  const char* arg0 = luaL_checkstring(L, 1);
  lovrFilesystemInit(arg0);
  return 0;
}

int l_lovrFilesystemExists(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  lua_pushboolean(L, lovrFilesystemExists(path));
  return 1;
}

int l_lovrFilesystemGetExecutablePath(lua_State* L) {
  const char* exePath = lovrFilesystemGetExecutablePath();

  if (exePath) {
    lua_pushstring(L, exePath);
  } else {
    lua_pushnil(L);
  }

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

int l_lovrFilesystemRead(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  int size;
  char* contents = lovrFilesystemRead(path, &size);
  if (!contents) {
    return luaL_error(L, "Could not read file '%s'", path);
  }

  lua_pushlstring(L, contents, size);
  return 1;
}

int l_lovrFilesystemSetSource(lua_State* L) {
  const char* source = lua_tostring(L, 1);

  if (source) {
    lua_pushboolean(L, lovrFilesystemSetSource(source));
  } else {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int l_lovrFilesystemWrite(lua_State* L) {
  int size;
  const char* path = luaL_checkstring(L, 1);
  const char* contents = luaL_checklstring(L, 2, &size);
  lua_pushnumber(L, lovrFilesystemWrite(path, contents, size));
  return 1;
}
