#include "lovr/types/vector.h"
#include "lovr/types/rotation.h"
#include "lovr/types/transform.h"
#include "util.h"

vec3 luax_newvector(lua_State* L) {
  vec3 v = (vec3) lua_newuserdata(L, 3 * sizeof(float));
  luaL_getmetatable(L, "Vector");
  lua_setmetatable(L, -2);
  return v;
}

vec3 luax_checkvector(lua_State* L, int i) {
  return luaL_checkudata(L, i, "Vector");
}

const luaL_Reg lovrVector[] = {
  { "clone", l_lovrVectorClone },
  { "unpack", l_lovrVectorUnpack },
  { "apply", l_lovrVectorApply },
  { "scale", l_lovrVectorScale },
  { "normalize", l_lovrVectorNormalize },
  { "distance", l_lovrVectorDistance },
  { "angle", l_lovrVectorAngle },
  { "dot", l_lovrVectorDot },
  { "cross", l_lovrVectorCross },
  { "lerp", l_lovrVectorLerp },
  { "__add", l_lovrVectorAdd },
  { "__sub", l_lovrVectorSub },
  { "__mul", l_lovrVectorMul },
  { "__div", l_lovrVectorDiv },
  { "__len", l_lovrVectorLen },
  { NULL, NULL }
};

int l_lovrVectorClone(lua_State* L) {
  vec3 v = luax_checkvector(L, 1);
  vec3_init(luax_newvector(L), v);
  return 1;
}

int l_lovrVectorUnpack(lua_State* L) {
  vec3 v = luax_checkvector(L, 1);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  return 3;
}

int l_lovrVectorApply(lua_State* L) {
  vec3 v = luax_checkvector(L, 1);

  if (luax_istype(L, 2, "Rotation")) {
    quat q = luax_checkrotation(L, 2);
    vec3_rotate(v, q);
  } else if (luax_istype(L, 2, "Transform")) {
    mat4 m = luax_checktransform(L, 2);
    vec3_transform(v, m);
  }

  lua_pushvalue(L, 1);
  return 1;
}

int l_lovrVectorScale(lua_State* L) {
  vec3 v = luax_checkvector(L, 1);
  float s = luaL_checknumber(L, 2);
  vec3_scale(v, s);
  return 1;
}

int l_lovrVectorNormalize(lua_State* L) {
  vec3 v = luax_checkvector(L, 1);
  vec3_normalize(v);
  return 1;
}

int l_lovrVectorDistance(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  lua_pushnumber(L, vec3_distance(u, v));
  return 1;
}

int l_lovrVectorAngle(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  lua_pushnumber(L, vec3_angle(u, v));
  return 1;
}

int l_lovrVectorDot(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  lua_pushnumber(L, vec3_dot(u, v));
  return 1;
}

int l_lovrVectorCross(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  vec3_cross(vec3_init(luax_newvector(L), u), v);
  return 1;
}

int l_lovrVectorLerp(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  float t = luaL_checknumber(L, 3);
  vec3_lerp(vec3_init(luax_newvector(L), u), v, t);
  return 1;
}

int l_lovrVectorAdd(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  vec3_add(vec3_init(luax_newvector(L), u), v);
  return 1;
}

int l_lovrVectorSub(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  vec3_sub(vec3_init(luax_newvector(L), u), v);
  return 1;
}

int l_lovrVectorMul(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  vec3_mul(vec3_init(luax_newvector(L), u), v);
  return 1;
}

int l_lovrVectorDiv(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  vec3 v = luax_checkvector(L, 2);
  vec3_div(vec3_init(luax_newvector(L), u), v);
  return 1;
}

int l_lovrVectorLen(lua_State* L) {
  vec3 u = luax_checkvector(L, 1);
  lua_pushnumber(L, vec3_length(u));
  return 1;
}
