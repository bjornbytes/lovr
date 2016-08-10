#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrEvent[];
int l_lovrEventInit(lua_State* L);
int l_lovrEventPoll(lua_State* L);
int l_lovrEventQuit(lua_State* L);
