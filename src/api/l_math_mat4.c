#include "api.h"
#include "math/math.h"
#include "core/maf.h"
#include "util.h"

static int l_lovrMat4Equals(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  mat4 n = luax_checkmat4(L, 2);
  for (int i = 0; i < 16; i += 4) {
    float dx = m[i + 0] - n[i + 0];
    float dy = m[i + 1] - n[i + 1];
    float dz = m[i + 2] - n[i + 2];
    float dw = m[i + 3] - n[i + 3];
    float distance2 = dx * dx + dy * dy + dz * dz + dw * dw;
    if (distance2 > 1e-10f) {
      lua_pushboolean(L, false);
      return 1;
    }
  }
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrMat4Unpack(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  if (lua_toboolean(L, 2)) {
    for (int i = 0; i < 16; i++) {
      lua_pushnumber(L, m[i]);
    }
    return 16;
  } else {
    float position[3], scale[3], angle, ax, ay, az;
    mat4_getPosition(m, position);
    mat4_getScale(m, scale);
    mat4_getAngleAxis(m, &angle, &ax, &ay, &az);
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

static int l_lovrMat4GetPosition(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float position[3];
  mat4_getPosition(m, position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrMat4GetOrientation(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float angle, ax, ay, az;
  mat4_getAngleAxis(m, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrMat4GetScale(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float scale[3];
  mat4_getScale(m, scale);
  lua_pushnumber(L, scale[0]);
  lua_pushnumber(L, scale[1]);
  lua_pushnumber(L, scale[2]);
  return 3;
}

static int l_lovrMat4GetPose(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float position[3], angle, ax, ay, az;
  mat4_getPosition(m, position);
  mat4_getAngleAxis(m, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

int l_lovrMat4Set(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  int top = lua_gettop(L);
  int type = lua_type(L, 2);
  if (type == LUA_TNONE || type == LUA_TNIL || (top == 2 && type == LUA_TNUMBER)) {
    float x = luax_optfloat(L, 2, 1.f);
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = x;
  } else if (top == 17) {
    for (int i = 2; i <= 17; i++) {
      *m++ = luax_checkfloat(L, i);
    }
  } else {
    if (luax_ismat4(L, 2)) {
      mat4_init(m, lua_touserdata(L, 2));
    } else {
      int index = 2;
      mat4_identity(m);
      index = luax_readvec3(L, index, &m[12], "nil, number, vec3, or mat4");

      if (lua_istable(L, index) && luax_len(L, index) == 4) {
        float q[4];
        luax_readquat(L, index, q, NULL);
        mat4_rotateQuat(m, q);
      } else if ((top - index) == 3 && lua_type(L, top) == LUA_TNUMBER) {
        float angle = luax_checkfloat(L, index++);
        float ax = luax_checkfloat(L, index++);
        float ay = luax_checkfloat(L, index++);
        float az = luax_checkfloat(L, index++);
        mat4_rotate(m, angle, ax, ay, az);
      } else {
        float scale[3];
        index = luax_readscale(L, index, scale, 3, NULL);

        float rotation[4];
        luax_readquat(L, index, rotation, NULL);
        mat4_rotateQuat(m, rotation);
        mat4_scale(m, scale[0], scale[1], scale[2]);
      }
    }
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Mul(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  if (luax_ismat4(L, 2)) {
    mat4_mul(m, lua_touserdata(L, 2));
    lua_settop(L, 1);
  } else if (lua_isnumber(L, 2) || (lua_istable(L, 2) && luax_len(L, 2) == 3)) {
    float v[3];
    luax_readvec3(L, 2, v, NULL);
    mat4_mulPoint(m, v);
    luax_pushvec3(L, v);
  } else if (lua_type(L, 2) == LUA_TTABLE && luax_len(L, 2) == 4) {
    float v[4];
    lua_rawgeti(L, 2, 1);
    lua_rawgeti(L, 2, 2);
    lua_rawgeti(L, 2, 3);
    lua_rawgeti(L, 2, 4);
    v[0] = luax_tofloat(L, -4);
    v[1] = luax_tofloat(L, -3);
    v[2] = luax_tofloat(L, -2);
    v[3] = luax_tofloat(L, -1);
    lua_pop(L, 4);
    mat4_mulVec4(m, v);
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, v[0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, v[1]);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, v[2]);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, v[3]);
    lua_rawseti(L, -2, 4);
    luaL_newmetatable(L, "Vec4");
    lua_setmetatable(L, -2);
  } else {
    return luax_typeerror(L, 2, "mat4, vec3, vec4, or number");
  }
  return 1;
}

static int l_lovrMat4Identity(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  mat4_identity(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Invert(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  mat4_invert(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Transpose(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  mat4_transpose(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Translate(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float translation[3];
  luax_readvec3(L, 2, translation, NULL);
  mat4_translate(m, translation[0], translation[1], translation[2]);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Rotate(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float rotation[4];
  luax_readquat(L, 2, rotation, NULL);
  mat4_rotateQuat(m, rotation);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Scale(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float scale[3];
  luax_readscale(L, 2, scale, 3, NULL);
  mat4_scale(m, scale[0], scale[1], scale[2]);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Orthographic(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  if (lua_gettop(L) <= 5) {
    float width = luax_checkfloat(L, 2);
    float height = luax_checkfloat(L, 3);
    float n = luax_optfloat(L, 4, -1.f);
    float f = luax_optfloat(L, 5, 1.f);
    mat4_orthographic(m, 0.f, width, 0.f, height, n, f);
  } else {
    float left = luax_checkfloat(L, 2);
    float right = luax_checkfloat(L, 3);
    float bottom = luax_checkfloat(L, 4);
    float top = luax_checkfloat(L, 5);
    float n = luax_checkfloat(L, 6);
    float f = luax_checkfloat(L, 7);
    mat4_orthographic(m, left, right, bottom, top, n, f);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Perspective(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float fovy = luax_checkfloat(L, 2);
  float aspect = luax_checkfloat(L, 3);
  float n = luax_checkfloat(L, 4);
  float f = luax_optfloat(L, 5, 0.);
  mat4_perspective(m, fovy, aspect, n, f);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Fov(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float left = luax_checkfloat(L, 2);
  float right = luax_checkfloat(L, 3);
  float up = luax_checkfloat(L, 4);
  float down = luax_checkfloat(L, 5);
  float n = luax_checkfloat(L, 6);
  float f = luax_optfloat(L, 7, 0.);
  mat4_fov(m, left, right, up, down, n, f);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4LookAt(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float from[3], to[3], up[3];
  int index = 2;
  index = luax_readvec3(L, index, from, NULL);
  index = luax_readvec3(L, index, to, NULL);
  if (lua_isnoneornil(L, index)) {
    vec3_set(up, 0.f, 1.f, 0.f);
  } else {
    luax_readvec3(L, index, up, NULL);
  }
  mat4_lookAt(m, from, to, up);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Target(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float from[3], to[3], up[3];
  int index = 2;
  index = luax_readvec3(L, index, from, NULL);
  index = luax_readvec3(L, index, to, NULL);
  if (lua_isnoneornil(L, index)) {
    vec3_set(up, 0.f, 1.f, 0.f);
  } else {
    luax_readvec3(L, index, up, NULL);
  }
  mat4_target(m, from, to, up);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Reflect(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  float position[3], normal[3];
  int index = 2;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readvec3(L, index, normal, NULL);
  mat4_reflect(m, position, normal);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4__mul(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  if (lua_type(L, 2) == LUA_TTABLE && luax_len(L, 2) == 4) {
    float v[4];
    lua_rawgeti(L, 2, 1);
    lua_rawgeti(L, 2, 2);
    lua_rawgeti(L, 2, 3);
    lua_rawgeti(L, 2, 4);
    v[0] = luax_tofloat(L, -4);
    v[1] = luax_tofloat(L, -3);
    v[2] = luax_tofloat(L, -2);
    v[3] = luax_tofloat(L, -1);
    lua_pop(L, 4);
    mat4_mulVec4(m, v);
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, v[0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, v[1]);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, v[2]);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, v[3]);
    lua_rawseti(L, -2, 4);
    luaL_newmetatable(L, "Vec4");
    lua_setmetatable(L, -2);
    return 1;
  } else if (lua_type(L, 2) == LUA_TTABLE && luax_len(L, 2) == 3) {
    float v[3];
    luax_readvec3(L, 2, v, NULL);
    mat4_mulPoint(m, v);
    luax_pushvec3(L, v);
    return 1;
  }
  mat4 n = luax_checkmat4(L, 2);
  mat4 result = luax_newmat4(L);
  mat4_mul(mat4_init(result, m), n);
  return 1;
}

static int l_lovrMat4__tostring(lua_State* L) {
  mat4 m = luax_checkmat4(L, 1);
  const char* format = "(%f, %f, %f, %f,\n %f, %f, %f, %f,\n %f, %f, %f, %f,\n %f, %f, %f, %f)";
  lua_pushfstring(L, format,
    m[0], m[4], m[8], m[12],
    m[1], m[5], m[9], m[13],
    m[2], m[6], m[10], m[14],
    m[3], m[7], m[11], m[15]);
  return 1;
}

const luaL_Reg lovrMat4[] = {
  { "equals", l_lovrMat4Equals },
  { "unpack", l_lovrMat4Unpack },
  { "getPosition", l_lovrMat4GetPosition },
  { "getOrientation", l_lovrMat4GetOrientation },
  { "getScale", l_lovrMat4GetScale },
  { "getPose", l_lovrMat4GetPose },
  { "set", l_lovrMat4Set },
  { "mul", l_lovrMat4Mul },
  { "identity", l_lovrMat4Identity },
  { "invert", l_lovrMat4Invert },
  { "transpose", l_lovrMat4Transpose },
  { "translate", l_lovrMat4Translate },
  { "rotate", l_lovrMat4Rotate },
  { "scale", l_lovrMat4Scale },
  { "orthographic", l_lovrMat4Orthographic },
  { "perspective", l_lovrMat4Perspective },
  { "fov", l_lovrMat4Fov },
  { "lookAt", l_lovrMat4LookAt },
  { "target", l_lovrMat4Target },
  { "reflect", l_lovrMat4Reflect },
  { "__mul", l_lovrMat4__mul },
  { "__tostring", l_lovrMat4__tostring },
  { NULL, NULL }
};
