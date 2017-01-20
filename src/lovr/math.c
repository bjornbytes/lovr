#include "lovr/math.h"
#include "lovr/types/vector.h"
#include "lovr/types/rotation.h"
#include "lovr/types/transform.h"
#include "math/vec3.h"
#include "math/quat.h"
#include "math/mat4.h"
#include "util.h"

const luaL_Reg lovrMath[] = {
  { "newVector", l_lovrMathNewVector },
  { "newRotation", l_lovrMathNewRotation },
  { "newTransform", l_lovrMathNewTransform },
  { NULL, NULL }
};

int l_lovrMathInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrMath);
  luax_registertype(L, "Vector", lovrVector, NULL);
  luax_registertype(L, "Rotation", lovrRotation, NULL);
  luax_registertype(L, "Transform", lovrTransform, NULL);
  return 1;
}

int l_lovrMathNewVector(lua_State* L) {
  float x, y, z;

  if (lua_gettop(L) == 1) {
    x = y = z = luaL_checknumber(L, 1);
  } else {
    x = luaL_checknumber(L, 1);
    y = luaL_checknumber(L, 2);
    z = luaL_checknumber(L, 3);
  }

  vec3_set(luax_newvector(L), x, y, z);
  return 1;
}

int l_lovrMathNewRotation(lua_State* L) {
  if (lua_gettop(L) == 4) {
    float angle = luaL_optnumber(L, 1, 0);
    float axis[3];
    axis[0] = luaL_optnumber(L, 2, 0);
    axis[1] = luaL_optnumber(L, 3, 0);
    axis[2] = luaL_optnumber(L, 4, 0);
    quat_fromAngleAxis(luax_newrotation(L), angle, axis);
  } else if (lua_isnumber(L, 1) && luax_istype(L, 2, "Vector")) {
    quat_fromAngleAxis(luax_newrotation(L), lua_tonumber(L, 1), luax_checkvector(L, 2));
  } else {
    quat_between(luax_newrotation(L), luax_checkvector(L, 1), luax_checkvector(L, 2));
  }

  return 1;
}

int l_lovrMathNewTransform(lua_State* L) {
  int hasArgs = lua_gettop(L) > 0;
  mat4 m = luax_newtransform(L);
  mat4_identity(m);

  if (hasArgs) {
    float x = luaL_checknumber(L, 1);
    float y = luaL_checknumber(L, 2);
    float z = luaL_checknumber(L, 3);
    float s = luaL_optnumber(L, 4, 1);
    float angle = luaL_optnumber(L, 5, 0);
    float ax = luaL_optnumber(L, 6, 0);
    float ay = luaL_optnumber(L, 7, 0);
    float az = luaL_optnumber(L, 8, 0);
    mat4_translate(m, x, y, z);
    mat4_scale(m, s, s, s);
    mat4_rotate(m, angle, ax, ay, az);
  }

  return 1;
}
