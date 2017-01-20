#include "math/vec3.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

vec3 luax_newvector(lua_State* L);
vec3 luax_checkvector(lua_State* L, int i);

extern const luaL_Reg lovrVector[];
int l_lovrVectorClone(lua_State* L);
int l_lovrVectorUnpack(lua_State* L);
int l_lovrVectorApply(lua_State* L);
int l_lovrVectorScale(lua_State* L);
int l_lovrVectorNormalize(lua_State* L);
int l_lovrVectorDistance(lua_State* L);
int l_lovrVectorAngle(lua_State* L);
int l_lovrVectorDot(lua_State* L);
int l_lovrVectorCross(lua_State* L);
int l_lovrVectorLerp(lua_State* L);
int l_lovrVectorAdd(lua_State* L);
int l_lovrVectorSub(lua_State* L);
int l_lovrVectorMul(lua_State* L);
int l_lovrVectorDiv(lua_State* L);
int l_lovrVectorLen(lua_State* L);
