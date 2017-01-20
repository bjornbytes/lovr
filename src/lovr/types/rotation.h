#include "math/quat.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

vec3 luax_newrotation(lua_State* L);
vec3 luax_checkrotation(lua_State* L, int i);

extern const luaL_Reg lovrRotation[];
int l_lovrRotationClone(lua_State* L);
int l_lovrRotationUnpack(lua_State* L);
int l_lovrRotationApply(lua_State* L);
int l_lovrRotationNormalize(lua_State* L);
int l_lovrRotationRotate(lua_State* L);
int l_lovrRotationMix(lua_State* L);
int l_lovrRotationMul(lua_State* L);
int l_lovrRotationLen(lua_State* L);
