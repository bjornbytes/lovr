#include "filesystem.h"
#include "../filesystem/filesystem.h"

const luaL_Reg lovrFilesystem[] = {
  { "init", l_lovrFilesystemInitialize },
  { "exists", l_lovrFilesystemExists },
  { "getExecutablePath", l_lovrFilesystemGetExecutablePath },
  { "isDirectory", l_lovrFilesystemIsDirectory },
  { "isFile", l_lovrFilesystemIsFile },
  { "setSource", l_lovrFilesystemSetSource },
  { NULL, NULL }
};

int l_lovrFilesystemInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrFilesystem);
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

int l_lovrFilesystemSetSource(lua_State* L) {
  const char* source = lua_tostring(L, 1);

  if (source) {
    lua_pushboolean(L, lovrFilesystemSetSource(source));
  } else {
    lua_pushboolean(L, 0);
  }

  return 1;
}
