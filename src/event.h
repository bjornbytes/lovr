#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int lovrEventPoll(lua_State* L);
int lovrEventQuit(lua_State* L);

extern const luaL_Reg lovrEvent[];
