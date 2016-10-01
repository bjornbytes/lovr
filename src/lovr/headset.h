#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrHeadset[];
int l_lovrHeadsetInit(lua_State* L);
int l_lovrHeadsetIsPresent(lua_State* L);
int l_lovrHeadsetGetType(lua_State* L);
int l_lovrHeadsetGetClipDistance(lua_State* L);
int l_lovrHeadsetSetClipDistance(lua_State* L);
int l_lovrHeadsetGetTrackingSize(lua_State* L);
int l_lovrHeadsetGetPosition(lua_State* L);
int l_lovrHeadsetGetOrientation(lua_State* L);
int l_lovrHeadsetGetVelocity(lua_State* L);
int l_lovrHeadsetGetAngularVelocity(lua_State* L);
int l_lovrHeadsetRenderTo(lua_State* L);
