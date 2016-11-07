#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrFilesystem[];
int l_lovrFilesystemInit(lua_State* L);
int l_lovrFilesystemInitialize(lua_State* L);
int l_lovrFilesystemExists(lua_State* L);
int l_lovrFilesystemGetExecutablePath(lua_State* L);
int l_lovrFilesystemIsDirectory(lua_State* L);
int l_lovrFilesystemIsFile(lua_State* L);
int l_lovrFilesystemRead(lua_State* L);
int l_lovrFilesystemSetSource(lua_State* L);
