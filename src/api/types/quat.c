#include "api.h"
#include "api/math.h"
#include "math/math.h"

int luax_readquat(lua_State* L, int index, quat q, const char* expected) {
  float angle, ax, ay, az;
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      quat_set(q, 0, 0, 0, 0);
      return ++index;
    case LUA_TNUMBER:
      angle = luaL_optnumber(L, index++, 0.);
      ax = luaL_optnumber(L, index++, 0.);
      ay = luaL_optnumber(L, index++, 1.);
      az = luaL_optnumber(L, index++, 0.);
      quat_fromAngleAxis(q, angle, ax, ay, az);
      return index;
    default:
      quat_init(q, luax_checkmathtype(L, index++, MATH_QUAT, expected ? expected : "quat or number"));
      return index;
  }
}

int luax_pushquat(lua_State* L, quat q, int index) {
  quat out;
  if (index > 0 && !lua_isnoneornil(L, index) && (out = luax_checkmathtype(L, index, MATH_QUAT, NULL)) != NULL) {
    quat_init(out, q);
    lua_settop(L, index);
    return 1;
  } else {
    float angle, ax, ay, az;
    quat_getAngleAxis(q, &angle, &ax, &ay, &az);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 4;
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
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    if (lua_type(L, 3) == LUA_TNUMBER) {
      float y = luaL_checknumber(L, 3);
      float z = luaL_checknumber(L, 4);
      float w = luaL_checknumber(L, 5);
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
        quat_between(q, (float[3]) { 0.f, 0.f, -1.f }, p);
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

static int l_lovrQuatCopy(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  quat r = lovrPoolAllocate(lovrMathGetPool(), MATH_QUAT);
  if (r) {
    quat_init(r, q);
    luax_pushlightmathtype(L, r, MATH_QUAT);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrQuatSave(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  quat copy = lua_newuserdata(L, 4 * sizeof(float));
  quat_init(copy, q);
  luaL_getmetatable(L, "quat");
  lua_setmetatable(L, -2);
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
  float t = luaL_checknumber(L, 3);
  quat_slerp(q, r, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuat__mul(lua_State* L) {
  quat q = luax_checkmathtype(L, 1, MATH_QUAT, NULL);
  MathType type;
  float* r = luax_tomathtype(L, 2, &type);
  if (type == MATH_VEC3) {
    vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
    quat_rotate(q, vec3_init(out, r));
    luax_pushlightmathtype(L, out, MATH_VEC3);
  } else {
    quat out = lovrPoolAllocate(lovrMathGetPool(), MATH_QUAT);
    quat_mul(quat_init(out, q), r);
    luax_pushlightmathtype(L, out, MATH_QUAT);
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
  { "copy", l_lovrQuatCopy },
  { "save", l_lovrQuatSave },
  { "normalize", l_lovrQuatNormalize },
  { "slerp", l_lovrQuatSlerp },
  { "__mul", l_lovrQuat__mul },
  { "__len", l_lovrQuat__len },
  { "__tostring", l_lovrQuat__tostring },
  { NULL, NULL }
};
