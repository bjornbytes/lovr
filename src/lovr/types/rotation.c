#include "lovr/types/rotation.h"
#include "lovr/types/vector.h"
#include "math/vec3.h"
#include "util.h"

quat luax_newrotation(lua_State* L) {
  quat q = (quat) lua_newuserdata(L, 4 * sizeof(float));
  luaL_getmetatable(L, "Rotation");
  lua_setmetatable(L, -2);
  return q;
}

quat luax_checkrotation(lua_State* L, int i) {
  return luaL_checkudata(L, i, "Rotation");
}

const luaL_Reg lovrRotation[] = {
  { "clone", l_lovrRotationClone },
  { "unpack", l_lovrRotationUnpack },
  { "apply", l_lovrRotationApply },
  { "normalize", l_lovrRotationNormalize },
  { "rotate", l_lovrRotationRotate },
  { "mix", l_lovrRotationMix },
  { "__mul", l_lovrRotationMul },
  { "__len", l_lovrRotationLen },
  { NULL, NULL }
};

int l_lovrRotationClone(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  quat_init(luax_newrotation(L), q);
  return 1;
}

int l_lovrRotationUnpack(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  float angle, x, y, z;
  quat_getAngleAxis(q, &angle, &x, &y, &z);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrRotationApply(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  quat r = luax_checkrotation(L, 2);
  quat_multiply(q, r);
  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrRotationNormalize(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  quat_normalize(q);
  return 1;
}

int l_lovrRotationRotate(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  if (lua_isnumber(L, 2)) {
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);
    float v[3];
    vec3_rotate(vec3_set(v, x, y, z), q);
    lua_pushnumber(L, v[0]);
    lua_pushnumber(L, v[1]);
    lua_pushnumber(L, v[2]);
    return 3;
  } else {
    vec3 v = luax_checkvector(L, 2);
    vec3_rotate(v, q);
    return 1;
  }
}

int l_lovrRotationMix(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  quat r = luax_checkrotation(L, 2);
  float t = luaL_checknumber(L, 3);
  quat_slerp(quat_init(luax_newrotation(L), q), r, t);
  return 1;
}

int l_lovrRotationMul(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  if (luax_istype(L, 2, "Rotation")) {
    quat r = luax_checkrotation(L, 2);
    quat_multiply(quat_init(luax_newrotation(L), q), r);
    return 1;
  } else {
    vec3 v = luax_checkvector(L, 2);
    vec3_rotate(vec3_init(luax_newvector(L), v), q);
    return 1;
  }
}

int l_lovrRotationLen(lua_State* L) {
  quat q = luax_checkrotation(L, 1);
  lua_pushnumber(L, quat_length(q));
  return 1;
}
