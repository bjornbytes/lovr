#include "luax.h"
#include "math/transform.h"
#include "math/math.h"

void luax_readtransform(lua_State* L, int i, mat4 transform);

extern const luaL_Reg lovrTransform[];
int l_lovrTransformClone(lua_State* L);
int l_lovrTransformInverse(lua_State* L);
int l_lovrTransformApply(lua_State* L);
int l_lovrTransformOrigin(lua_State* L);
int l_lovrTransformTranslate(lua_State* L);
int l_lovrTransformRotate(lua_State* L);
int l_lovrTransformScale(lua_State* L);
int l_lovrTransformSetTransformation(lua_State* L);
int l_lovrTransformTransformPoint(lua_State* L);
int l_lovrTransformInverseTransformPoint(lua_State* L);
