#include "lovr/types/transform.h"
#include "lovr/types/rotation.h"
#include "math/vec3.h"
#include "util.h"

mat4 luax_newtransform(lua_State* L) {
  mat4 m = (mat4) lua_newuserdata(L, 16 * sizeof(float));
  luaL_getmetatable(L, "Transform");
  lua_setmetatable(L, -2);
  return m;
}

mat4 luax_checktransform(lua_State* L, int i) {
  return luaL_checkudata(L, i, "Transform");
}

const luaL_Reg lovrTransform[] = {
  { "clone", l_lovrTransformClone },
  { "unpack", l_lovrTransformUnpack },
  { "inverse", l_lovrTransformInverse },
  { "apply", l_lovrTransformApply },
  { "origin", l_lovrTransformOrigin },
  { "translate", l_lovrTransformTranslate },
  { "rotate", l_lovrTransformRotate },
  { "scale", l_lovrTransformScale },
  { "transform", l_lovrTransformTransform },
  { "__mul", l_lovrTransformMul },
  { NULL, NULL }
};

int l_lovrTransformClone(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4_set(luax_newtransform(L), m);
  return 1;
}

int l_lovrTransformUnpack(lua_State* L) {
  return 0;
}

int l_lovrTransformInverse(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4_invert(mat4_set(luax_newtransform(L), m));
  return 1;
}

int l_lovrTransformApply(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4 n = luax_checktransform(L, 2);
  mat4_multiply(m, n);
  return 1;
}

int l_lovrTransformOrigin(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4_identity(m);
  return 1;
}

int l_lovrTransformTranslate(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  mat4_translate(m, x, y, z);
  lua_settop(L, 1);
  return 1;
}

int l_lovrTransformRotate(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);

  if (lua_isnumber(L, 2)) {
    float angle = luaL_checknumber(L, 2);
    float axis[3];
    axis[0] = luaL_checknumber(L, 3);
    axis[1] = luaL_checknumber(L, 4);
    axis[2] = luaL_checknumber(L, 5);
    float q[4];
    quat_fromAngleAxis(q, angle, axis);
    mat4_rotate(m, q);
  } else if (luax_istype(L, 2, "Rotation")) {
    quat q = luax_checkrotation(L, 2);
    mat4_rotate(m, q);
  }

  lua_settop(L, 1);
  return 1;
}

int l_lovrTransformScale(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  float x = luaL_checknumber(L, 2);
  float y = lua_gettop(L) > 2 ? luaL_checknumber(L, 3) : x;
  float z = lua_gettop(L) > 2 ? luaL_checknumber(L, 4) : x;
  mat4_scale(m, x, y, z);
  lua_settop(L, 1);
  return 1;
}

int l_lovrTransformTransform(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  float v[3];
  v[0] = luaL_checknumber(L, 2);
  v[1] = luaL_checknumber(L, 3);
  v[2] = luaL_checknumber(L, 4);
  vec3_transform(v, m);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  return 3;
}

int l_lovrTransformMul(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4 n = luax_checktransform(L, 2);
  mat4_multiply(mat4_set(luax_newtransform(L), m), n);
  return 1;
}
