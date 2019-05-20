#include "api.h"
#include "math/math.h"
#include "math/pool.h"
#include "core/maf.h"

int luax_readmat4(lua_State* L, int index, mat4 m, int scaleComponents) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      mat4_identity(m);
      return index + 1;

    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
    default: {
      MathType type;
      float* p = luax_tomathtype(L, index, &type);
      if (type == MATH_MAT4) {
        mat4_init(m, p);
        return index + 1;
      }
    } // Fall through

    case LUA_TNUMBER: {
      float S[3];
      float R[4];
      mat4_identity(m);
      index = luax_readvec3(L, index, m + 12, "mat4, vec3, or number");
      index = luax_readscale(L, index, S, scaleComponents, NULL);
      index = luax_readquat(L, index, R, NULL);
      mat4_rotateQuat(m, R);
      mat4_scale(m, S[0], S[1], S[2]);
      return index;
    }
  }
}

static int l_lovrMat4Unpack(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  if (lua_toboolean(L, 2)) {
    for (int i = 0; i < 16; i++) {
      lua_pushnumber(L, m[i]);
    }
    return 16;
  } else {
    float angle, ax, ay, az;
    float position[3], orientation[4], scale[3];
    mat4_getPosition(m, position);
    mat4_getScale(m, scale);
    mat4_getOrientation(m, orientation);
    quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    lua_pushnumber(L, scale[0]);
    lua_pushnumber(L, scale[1]);
    lua_pushnumber(L, scale[2]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 10;
  }
}

int l_lovrMat4Set(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  if (lua_gettop(L) >= 17) {
    for (int i = 2; i <= 17; i++) {
      *m++ = luaL_checknumber(L, i);
    }
  } else {
    luax_readmat4(L, 2, m, 3);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Identity(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  mat4_identity(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Invert(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  mat4_invert(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Transpose(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  mat4_transpose(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Translate(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    mat4_translate(m, luax_checkfloat(L, 2), luax_checkfloat(L, 3), luax_checkfloat(L, 4));
  } else {
    float* v = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    mat4_translate(m, v[0], v[1], v[2]);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Rotate(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    mat4_rotate(m, luax_checkfloat(L, 2), luax_checkfloat(L, 3), luax_checkfloat(L, 4), luax_checkfloat(L, 5));
  } else {
    float* q = luax_checkmathtype(L, 2, MATH_QUAT, "quat or number");
    mat4_rotateQuat(m, q);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Scale(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = luax_checkfloat(L, 2);
    mat4_scale(m, x, luax_optfloat(L, 3, x), luax_optfloat(L, 4, x));
  } else {
    float* s = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    mat4_scale(m, s[0], s[1], s[2]);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Mul(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  MathType type;
  float* n = luax_tomathtype(L, 2, &type);
  if (n && type == MATH_MAT4) {
    mat4_multiply(m, n);
    lua_settop(L, 1);
    return 1;
  } else if (n && type == MATH_VEC3) {
    mat4_transform(m, n);
    lua_settop(L, 2);
    return 1;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = luaL_checknumber(L, 2);
    float y = luaL_optnumber(L, 3, 0.f);
    float z = luaL_optnumber(L, 4, 0.f);
    float v[3] = { x, y, z };
    mat4_transform(m, v);
    lua_pushnumber(L, v[0]);
    lua_pushnumber(L, v[1]);
    lua_pushnumber(L, v[2]);
    return 3;
  } else {
    return luaL_typerror(L, 2, "mat4, vec3, or number");
  }
}

static int l_lovrMat4Perspective(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  float clipNear = luax_checkfloat(L, 2);
  float clipFar = luax_checkfloat(L, 3);
  float fov = luax_checkfloat(L, 4);
  float aspect = luax_checkfloat(L, 5);
  mat4_perspective(m, clipNear, clipFar, fov, aspect);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Orthographic(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  float left = luax_checkfloat(L, 2);
  float right = luax_checkfloat(L, 3);
  float top = luax_checkfloat(L, 4);
  float bottom = luax_checkfloat(L, 5);
  float clipNear = luax_checkfloat(L, 6);
  float clipFar = luax_checkfloat(L, 7);
  mat4_orthographic(m, left, right, top, bottom, clipNear, clipFar);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4__mul(lua_State* L) {
  mat4 m = luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  MathType type;
  float* n = luax_tomathtype(L, 2, &type);
  if (!n || type == MATH_QUAT) return luaL_typerror(L, 2, "mat4 or vec3");
  if (type == MATH_MAT4) {
    mat4 out = luax_newmathtype(L, MATH_MAT4);
    mat4_multiply(mat4_init(out, m), n);
  } else {
    vec3 out = luax_newmathtype(L, MATH_VEC3);
    vec3_init(out, n);
    mat4_transform(m, n);
  }
  return 1;
}

static int l_lovrMat4__tostring(lua_State* L) {
  luax_checkmathtype(L, 1, MATH_MAT4, NULL);
  lua_pushliteral(L, "mat4");
  return 1;
}

const luaL_Reg lovrMat4[] = {
  { "unpack", l_lovrMat4Unpack },
  { "set", l_lovrMat4Set },
  { "identity", l_lovrMat4Identity },
  { "invert", l_lovrMat4Invert },
  { "transpose", l_lovrMat4Transpose },
  { "translate", l_lovrMat4Translate },
  { "rotate", l_lovrMat4Rotate },
  { "scale", l_lovrMat4Scale },
  { "mul", l_lovrMat4Mul },
  { "perspective", l_lovrMat4Perspective },
  { "orthographic", l_lovrMat4Orthographic },
  { "__mul", l_lovrMat4__mul },
  { "__tostring", l_lovrMat4__tostring },
  { NULL, NULL }
};
