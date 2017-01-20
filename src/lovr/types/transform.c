#include "lovr/types/transform.h"
#include "lovr/types/rotation.h"
#include "lovr/types/vector.h"
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

void luax_readtransform(lua_State* L, int i, mat4 m) {
  if (lua_isnumber(L, i)) {
    float x = luaL_optnumber(L, i++, 0);
    float y = luaL_optnumber(L, i++, 0);
    float z = luaL_optnumber(L, i++, 0);
    float s = luaL_optnumber(L, i++, 1);
    float angle = luaL_optnumber(L, i++, 0);
    float ax = luaL_optnumber(L, i++, 0);
    float ay = luaL_optnumber(L, i++, 1);
    float az = luaL_optnumber(L, i++, 0);
    mat4_identity(m);
    mat4_translate(m, x, y, z);
    mat4_scale(m, s, s, s);
    mat4_rotate(m, angle, ax, ay, az);
  } else {
    mat4_init(m, luax_checktransform(L, i));
  }
}

const luaL_Reg lovrTransform[] = {
  { "clone", l_lovrTransformClone },
  { "apply", l_lovrTransformApply },
  { "inverse", l_lovrTransformInverse },
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

int l_lovrTransformApply(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);

  if (luax_istype(L, 2, "Transform")) {
    mat4 n = luax_checktransform(L, 2);
    mat4_multiply(m, n);
  } else {
    quat q = luax_checkrotation(L, 2);
    mat4_rotateQuat(m, q);
  }

  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformInverse(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4_invert(mat4_set(luax_newtransform(L), m));
  return 1;
}

int l_lovrTransformOrigin(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4_identity(m);
  return 1;
}

int l_lovrTransformTranslate(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);

  if (lua_isnumber(L, 2)) {
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    mat4_translate(m, x, y, z);
  } else {
    vec3 v = luax_checkvector(L, 1);
    mat4_translate(m, v[0], v[1], v[2]);
  }

  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformRotate(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);

  if (lua_isnumber(L, 2)) {
    float angle = luaL_checknumber(L, 2);
    float x = luaL_checknumber(L, 3);
    float y = luaL_checknumber(L, 4);
    float z = luaL_checknumber(L, 5);
    mat4_rotate(m, angle, x, y, z);
  } else if (luax_istype(L, 2, "Rotation")) {
    quat q = luax_checkrotation(L, 2);
    mat4_rotateQuat(m, q);
  }

  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformScale(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);

  if (lua_isnumber(L, 2)) {
    float x = luaL_checknumber(L, 2);
    float y = lua_gettop(L) > 2 ? luaL_checknumber(L, 3) : x;
    float z = lua_gettop(L) > 2 ? luaL_checknumber(L, 4) : x;
    mat4_scale(m, x, y, z);
  } else {
    vec3 v = luax_checkvector(L, 1);
    mat4_scale(m, v[0], v[1], v[2]);
  }

  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrTransformTransform(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  if (lua_isnumber(L, 2)) {
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    float v[3];
    vec3_transform(vec3_set(v, x, y, z), m);
    lua_pushnumber(L, v[0]);
    lua_pushnumber(L, v[1]);
    lua_pushnumber(L, v[2]);
    return 3;
  } else {
    vec3 v = luax_checkvector(L, 2);
    vec3_transform(v, m);
    return 1;
  }
}

int l_lovrTransformMul(lua_State* L) {
  mat4 m = luax_checktransform(L, 1);
  mat4 n = luax_checktransform(L, 2);
  mat4_multiply(mat4_set(luax_newtransform(L), m), n);
  return 1;
}
