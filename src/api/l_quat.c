#include "api.h"
#include "math/math.h"
#include "core/maf.h"

int luax_readquat(lua_State* L, int index, quat q, const char* expected) {
  float angle, ax, ay, az;
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      quat_set(q, 0, 0, 0, 0);
      return ++index;
    case LUA_TNUMBER:
      angle = luax_optfloat(L, index++, 0.f);
      ax = luax_optfloat(L, index++, 0.f);
      ay = luax_optfloat(L, index++, 1.f);
      az = luax_optfloat(L, index++, 0.f);
      quat_fromAngleAxis(q, angle, ax, ay, az);
      return index;
    default:
      quat_init(q, luax_checkmathtype(L, index++, MATH_QUAT, expected ? expected : "quat or number"));
      return index;
  }
}

static int l_lovrQuatUnpack(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  bool raw = lua_toboolean(L, 2);
  if (raw) {
    lua_pushnumber(L, q[0]);
    lua_pushnumber(L, q[1]);
    lua_pushnumber(L, q[2]);
    lua_pushnumber(L, q[3]);
  } else {
    float angle, ax, ay, az;
    quat_getAngleAxis(q, &angle, &ax, &ay, &az);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  }
  return 4;
}

int l_lovrQuatSet(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  if (lua_isnoneornil(L, 2)) {
    quat_set(q, 0.f, 0.f, 0.f, 1.f);
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    if (lua_type(L, 3) == LUA_TNUMBER) {
      float y = luax_checkfloat(L, 3);
      float z = luax_checkfloat(L, 4);
      float w = luax_checkfloat(L, 5);
      bool raw = lua_toboolean(L, 6);
      if (raw) {
        quat_set(q, x, y, z, w);
      } else {
        quat_fromAngleAxis(q, x, y, z, w);
      }
    } else {
      vec3 axis = luax_checkmathtype(L, 3, MATH_VEC3, "vec3 or number");
      quat_fromAngleAxis(q, x, axis[0], axis[1], axis[2]);
    }
  } else {
    MathType type;
    float* p = luax_tomathtype(L, 2, &type);
    if (!p) return luaL_typerror(L, 2, "vec3, quat, or number");

    if (type == MATH_VEC3) {
      if (lua_gettop(L) > 2) {
        vec3 u = luax_checkmathtype(L, 3, MATH_VEC3, "vec3");
        quat_between(q, p, u);
      } else {
        quat_between(q, (float[4]) { 0.f, 0.f, -1.f }, p);
      }
    } else if (type == MATH_QUAT) {
      quat_init(q, p);
    } else if (type == MATH_MAT4) {
      quat_fromMat4(q, p);
    } else {
      return luaL_typerror(L, 2, "vec3, quat, mat4, or number");
    }
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuatMul(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  MathType type;
  float* r = luax_tomathtype(L, 2, &type);
  if (!r || type == MATH_MAT4) return luaL_typerror(L, 2, "quat or vec3");
  if (type == MATH_VEC3) {
    quat_rotate(q, r);
    lua_settop(L, 2);
  } else {
    quat_mul(q, r);
    lua_settop(L, 1);
  }
  return 1;
}

static int l_lovrQuatLength(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  lua_pushnumber(L, quat_length(q));
  return 1;
}

static int l_lovrQuatNormalize(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  quat_normalize(q);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuatSlerp(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  quat r = luax_checkmathtype(L, 2, MATH_QUAT, NULL);
  float t = luax_checkfloat(L, 3);
  quat_slerp(q, r, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuat__mul(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  MathType type;
  float* r = luax_tomathtype(L, 2, &type);
  if (!r) return luaL_typerror(L, 2, "quat or vec3");
  if (type == MATH_VEC3) {
    vec3 out = luax_newmathtype(L, MATH_VEC3);
    quat_rotate(q, vec3_init(out, r));
  } else {
    quat out = luax_newmathtype(L, MATH_QUAT);
    quat_mul(quat_init(out, q), r);
  }
  return 1;
}

static int l_lovrQuat__len(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  lua_pushnumber(L, quat_length(q));
  return 1;
}

static int l_lovrQuat__tostring(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  lua_pushfstring(L, "(%f, %f, %f, %f)", q[0], q[1], q[2], q[3]);
  return 1;
}

const luaL_Reg lovrQuat[] = {
  { "unpack", l_lovrQuatUnpack },
  { "set", l_lovrQuatSet },
  { "mul", l_lovrQuatMul },
  { "length", l_lovrQuatLength },
  { "normalize", l_lovrQuatNormalize },
  { "slerp", l_lovrQuatSlerp },
  { "__mul", l_lovrQuat__mul },
  { "__len", l_lovrQuat__len },
  { "__tostring", l_lovrQuat__tostring },
  { NULL, NULL }
};
