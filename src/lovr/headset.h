#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrHeadset[];
int l_lovrHeadsetInit(lua_State* L);
int l_lovrHeadsetGetDisplayWidth(lua_State* L);
int l_lovrHeadsetGetDisplayHeight(lua_State* L);
int l_lovrHeadsetGetDisplayDimensions(lua_State* L);
int l_lovrHeadsetRenderTo(lua_State* L);
