#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "vendor/map/map.h"

map_int_t ControllerHands;
map_int_t ControllerAxes;
map_int_t ControllerButtons;

extern const luaL_Reg lovrHeadset[];
int l_lovrHeadsetInit(lua_State* L);
int l_lovrHeadsetIsPresent(lua_State* L);
int l_lovrHeadsetGetType(lua_State* L);
int l_lovrHeadsetGetDisplayWidth(lua_State* L);
int l_lovrHeadsetGetDisplayHeight(lua_State* L);
int l_lovrHeadsetGetDisplayDimensions(lua_State* L);
int l_lovrHeadsetGetClipDistance(lua_State* L);
int l_lovrHeadsetSetClipDistance(lua_State* L);
int l_lovrHeadsetGetBoundsWidth(lua_State* L);
int l_lovrHeadsetGetBoundsDepth(lua_State* L);
int l_lovrHeadsetGetBoundsDimensions(lua_State* L);
int l_lovrHeadsetGetBoundsGeometry(lua_State* L);
int l_lovrHeadsetIsBoundsVisible(lua_State* L);
int l_lovrHeadsetSetBoundsVisible(lua_State* L);
int l_lovrHeadsetGetPosition(lua_State* L);
int l_lovrHeadsetGetOrientation(lua_State* L);
int l_lovrHeadsetGetVelocity(lua_State* L);
int l_lovrHeadsetGetAngularVelocity(lua_State* L);
int l_lovrHeadsetGetControllers(lua_State* L);
int l_lovrHeadsetGetControllerCount(lua_State* L);
int l_lovrHeadsetRenderTo(lua_State* L);
