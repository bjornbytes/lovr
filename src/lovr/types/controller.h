#include "luax.h"
#include "headset/headset.h"

extern const luaL_Reg lovrController[];
int l_lovrControllerIsPresent(lua_State* L);
int l_lovrControllerGetPosition(lua_State* L);
int l_lovrControllerGetOrientation(lua_State* L);
int l_lovrControllerGetAxis(lua_State* L);
int l_lovrControllerIsDown(lua_State* L);
int l_lovrControllerVibrate(lua_State* L);
int l_lovrControllerNewModel(lua_State* L);
