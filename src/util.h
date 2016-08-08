#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

void error(const char* format, ...);
char* loadFile(char* filename);
void luaRegisterModule(lua_State* L, const char* name, const luaL_Reg* module);
void luaRegisterType(lua_State* L, const char* name, const luaL_Reg* functions, lua_CFunction gc);
int luaPreloadModule(lua_State* L, const char* key, lua_CFunction f);
