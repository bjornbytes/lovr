#include "math/mat4.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

mat4 luax_newtransform(lua_State* L);
mat4 luax_checktransform(lua_State* L, int i);
void luax_readtransform(lua_State* L, int i, mat4 transform);

extern const luaL_Reg lovrTransform[];
int l_lovrTransformClone(lua_State* L);
int l_lovrTransformUnpack(lua_State* L);
int l_lovrTransformApply(lua_State* L);
int l_lovrTransformInverse(lua_State* L);
int l_lovrTransformOrigin(lua_State* L);
int l_lovrTransformTranslate(lua_State* L);
int l_lovrTransformRotate(lua_State* L);
int l_lovrTransformScale(lua_State* L);
int l_lovrTransformTransform(lua_State* L);
int l_lovrTransformMul(lua_State* L);
