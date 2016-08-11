#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrHeadset[];
int l_lovrHeadsetInit(lua_State* L);
int l_lovrHeadsetGetPosition(lua_State* L);
int l_lovrHeadsetGetOrientation(lua_State* L);
int l_lovrHeadsetIsPresent(lua_State* L);
int l_lovrHeadsetRenderTo(lua_State* L);
