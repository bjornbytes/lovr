#include "api.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>

#define EQ_THRESHOLD 1e-10f

static const uint32_t* swizzles[5] = {
  [2] = (uint32_t[]) {
    ['x'] = 1,
    ['y'] = 2,
    ['r'] = 1,
    ['g'] = 2,
    ['s'] = 1,
    ['t'] = 2
  },
  [3] = (uint32_t[]) {
    ['x'] = 1,
    ['y'] = 2,
    ['z'] = 3,
    ['r'] = 1,
    ['g'] = 2,
    ['b'] = 3,
    ['s'] = 1,
    ['t'] = 2,
    ['p'] = 3
  },
  [4] = (uint32_t[]) {
    ['x'] = 1,
    ['y'] = 2,
    ['z'] = 3,
    ['w'] = 4,
    ['r'] = 1,
    ['g'] = 2,
    ['b'] = 3,
    ['a'] = 4,
    ['s'] = 1,
    ['t'] = 2,
    ['p'] = 3,
    ['q'] = 4
  }
};

// Helpers

int luax_readvec2(lua_State* L, int index, vec2 v, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = 0.f;
      return index + 1;
    case LUA_TNUMBER:
      v[0] = luax_tofloat(L, index++);
      v[1] = luax_optfloat(L, index++, v[0]);
      return index;
    default:
      vec2_init(v, luax_checkvector(L, index, V_VEC2, expected ? expected : "vec2 or number"));
      return index + 1;
  }
}

int luax_readvec3(lua_State* L, int index, vec3 v, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = 0.f;
      return index + 1;
    case LUA_TNUMBER:
      v[0] = luax_tofloat(L, index++);
      v[1] = luax_optfloat(L, index++, v[0]);
      v[2] = luax_optfloat(L, index++, v[0]);
      return index;
    default:
      vec3_init(v, luax_checkvector(L, index, V_VEC3, expected ? expected : "vec3 or number"));
      return index + 1;
  }
}

int luax_readvec4(lua_State* L, int index, vec4 v, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = v[3] = 0.f;
      return index + 1;
    case LUA_TNUMBER:
      v[0] = luax_tofloat(L, index++);
      v[1] = luax_optfloat(L, index++, v[0]);
      v[2] = luax_optfloat(L, index++, v[0]);
      v[3] = luax_optfloat(L, index++, v[0]);
      return index;
    default:
      vec4_init(v, luax_checkvector(L, index, V_VEC4, expected ? expected : "vec4 or number"));
      return index + 1;
  }
}

int luax_readscale(lua_State* L, int index, vec3 v, int components, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = 1.f;
      return index + components;
    case LUA_TNUMBER:
      if (components == 1) {
        v[0] = v[1] = v[2] = luax_tofloat(L, index++);
      } else if (components == -2) { // -2 is special and means "2 components: xy and z"
        v[0] = v[1] = luax_tofloat(L, index++);
        v[2] = luax_optfloat(L, index++, 1.f);
      } else {
        v[0] = v[1] = v[2] = 1.f;
        for (int i = 0; i < components; i++) {
          v[i] = luax_optfloat(L, index++, v[0]);
        }
      }
      return index;
    default: {
      VectorType type;
      float* u = luax_tovector(L, index++, &type);
      if (type == V_VEC2) {
        v[0] = u[0];
        v[1] = u[1];
        v[2] = 1.f;
      } else if (type == V_VEC3) {
        vec3_init(v, u);
      } else {
        return luax_typeerror(L, index, "vec2, vec3, or number");
      }
      return index;
    }
  }
}

int luax_readquat(lua_State* L, int index, quat q, const char* expected) {
  float angle, ax, ay, az;
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      quat_identity(q);
      return ++index;
    case LUA_TNUMBER:
      angle = luax_optfloat(L, index++, 0.f);
      ax = luax_optfloat(L, index++, 0.f);
      ay = luax_optfloat(L, index++, 1.f);
      az = luax_optfloat(L, index++, 0.f);
      quat_fromAngleAxis(q, angle, ax, ay, az);
      return index;
    default:
      quat_init(q, luax_checkvector(L, index++, V_QUAT, expected ? expected : "quat or number"));
      return index;
  }
}

int luax_readmat4(lua_State* L, int index, mat4 m, int scaleComponents) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      mat4_identity(m);
      return index + 1;

    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
    default: {
      VectorType type;
      float* p = luax_tovector(L, index, &type);
      if (type == V_MAT4) {
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

// vec2

static int l_lovrVec2Type(lua_State* L) {
  lua_pushliteral(L, "Vec2");
  return 1;
}

static int l_lovrVec2Equals(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float* u = luax_checkvector(L, 2, V_VEC2, NULL);
  lua_pushboolean(L, vec2_distance2(v, u) < EQ_THRESHOLD);
  return 1;
}

static int l_lovrVec2Unpack(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  return 2;
}

int l_lovrVec2Set(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  luax_readvec2(L, 2, v, NULL);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Add(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  vec2_add(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Sub(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  vec2_sub(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Mul(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  vec2_mul(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Div(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  vec2_div(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Length(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushnumber(L, vec2_length(v));
  return 1;
}

static int l_lovrVec2Normalize(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  vec2_normalize(v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Distance(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  lua_pushnumber(L, vec2_distance(v, u));
  return 1;
}

static int l_lovrVec2Dot(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  lua_pushnumber(L, vec2_dot(v, u));
  return 1;
}

static int l_lovrVec2Lerp(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  int index = luax_readvec2(L, 2, u, NULL);
  float t = luax_checkfloat(L, index);
  vec2_lerp(v, u, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Angle(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float u[2];
  luax_readvec2(L, 2, u, NULL);
  lua_pushnumber(L, vec2_angle(v, u));
  return 1;
}

static int l_lovrVec2__add(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = x + u[0];
    out[1] = x + u[1];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] + x;
    out[1] = v[1] + x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    vec2_add(vec2_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec2__sub(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = x - u[0];
    out[1] = x - u[1];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] - x;
    out[1] = v[1] - x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    vec2_sub(vec2_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec2__mul(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = x * u[0];
    out[1] = x * u[1];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] * x;
    out[1] = v[1] * x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    vec2_mul(vec2_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec2__div(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = x / u[0];
    out[1] = x / u[1];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] / x;
    out[1] = v[1] / x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    vec2_div(vec2_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec2__unm(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float* out = luax_newtempvector(L, V_VEC2);
  vec2_scale(vec2_init(out, v), -1.f);
  return 1;
}

static int l_lovrVec2__len(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushnumber(L, vec2_length(v));
  return 1;
}

static int l_lovrVec2__tostring(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushfstring(L, "(%f, %f)", v[0], v[1]);
  return 1;
}

static int l_lovrVec2__newindex(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index == 1 || index == 2) {
      float x = luax_checkfloat(L, 3);
      v[index - 1] = x;
      return 0;
    }
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;
    const char* str = lua_tolstring(L, 2, &length);
    const unsigned char* key = (const unsigned char*) str;

    if (length == 1 && swizzles[2][key[0]]) {
      v[swizzles[2][key[0]] - 1] = luax_checkfloat(L, 3);
      return 0;
    } else if (length == 2 && swizzles[2][key[0]] && swizzles[2][key[1]]) {
      float* u = luax_checkvector(L, 3, V_VEC2, NULL);
      for (size_t i = 0; i < length; i++) {
        v[swizzles[2][key[i]] - 1] = u[i];
      }
      return 0;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to assign property %s of vec2 (invalid property)", lua_tostring(L, -1));
  return 0;
}

static int l_lovrVec2__index(lua_State* L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      return 1;
    } else {
      lua_pop(L, 2);
    }
  }

  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  int type = lua_type(L, 2);
  if (type == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index == 1 || index == 2) {
      lua_pushnumber(L, v[index - 1]);
      return 1;
    }
  } else if (type == LUA_TSTRING) {
    size_t length;
    const char* str = lua_tolstring(L, 2, &length);
    const unsigned char* key = (const unsigned char*) str;

    if (length == 1 && swizzles[2][key[0]]) {
      lua_pushnumber(L, v[swizzles[2][key[0]] - 1]);
      return 1;
    } else if (length == 2 && swizzles[2][key[0]] && swizzles[2][key[1]]) {
      float* out = luax_newtempvector(L, V_VEC2);
      out[0] = v[swizzles[2][key[0]] - 1];
      out[1] = v[swizzles[2][key[1]] - 1];
      return 1;
    } else if (length == 3 && swizzles[2][key[0]] && swizzles[2][key[1]] && swizzles[2][key[2]]) {
      float* out = luax_newtempvector(L, V_VEC3);
      out[0] = v[swizzles[2][key[0]] - 1];
      out[1] = v[swizzles[2][key[1]] - 1];
      out[2] = v[swizzles[2][key[2]] - 1];
      return 1;
    } else if (length == 4 && swizzles[2][key[0]] && swizzles[2][key[1]] && swizzles[2][key[2]] && swizzles[2][key[3]]) {
      float* out = luax_newtempvector(L, V_VEC4);
      out[0] = v[swizzles[2][key[0]] - 1];
      out[1] = v[swizzles[2][key[1]] - 1];
      out[2] = v[swizzles[2][key[2]] - 1];
      out[3] = v[swizzles[2][key[3]] - 1];
      return 1;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to index field %s of vec2 (invalid property)", lua_tostring(L, -1));
  return 0;
}

int l_lovrVec2__metaindex(lua_State* L) {
  if (lua_type(L, 2) != LUA_TSTRING) {
    return 0;
  }

  size_t length;
  const char* key = lua_tolstring(L, 2, &length);

  static const struct { StringEntry name; float x, y; } properties[] = {
    { ENTRY("one"), 1.f, 1.f },
    { ENTRY("zero"), 0.f, 0.f }
  };

  for (uint32_t i = 0; i < COUNTOF(properties); i++) {
    if (length == properties[i].name.length && !memcmp(key, properties[i].name.string, length)) {
      float* v = luax_newtempvector(L, V_VEC2);
      v[0] = properties[i].x;
      v[1] = properties[i].y;
      return 1;
    }
  }

  return 0;
}

const luaL_Reg lovrVec2[] = {
  { "type", l_lovrVec2Type },
  { "equals", l_lovrVec2Equals },
  { "unpack", l_lovrVec2Unpack },
  { "set", l_lovrVec2Set },
  { "add", l_lovrVec2Add },
  { "sub", l_lovrVec2Sub },
  { "mul", l_lovrVec2Mul },
  { "div", l_lovrVec2Div },
  { "length", l_lovrVec2Length },
  { "normalize", l_lovrVec2Normalize },
  { "distance", l_lovrVec2Distance },
  { "dot", l_lovrVec2Dot },
  { "lerp", l_lovrVec2Lerp },
  { "angle", l_lovrVec2Angle },
  { "__add", l_lovrVec2__add },
  { "__sub", l_lovrVec2__sub },
  { "__mul", l_lovrVec2__mul },
  { "__div", l_lovrVec2__div },
  { "__unm", l_lovrVec2__unm },
  { "__len", l_lovrVec2__len },
  { "__tostring", l_lovrVec2__tostring },
  { "__newindex", l_lovrVec2__newindex },
  { "__index", l_lovrVec2__index },
  { NULL, NULL }
};

// vec3

static int l_lovrVec3Type(lua_State* L) {
  lua_pushliteral(L, "Vec3");
  return 1;
}

static int l_lovrVec3Equals(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC3, NULL);
  float* u = luax_checkvector(L, 2, V_VEC3, NULL);
  lua_pushboolean(L, vec3_distance2(v, u) < EQ_THRESHOLD);
  return 1;
}

static int l_lovrVec3Unpack(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  return 3;
}

int l_lovrVec3Set(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  if (lua_isnoneornil(L, 2) || lua_type(L, 2) == LUA_TNUMBER) {
    float x = luax_optfloat(L, 2, 0.f);
    vec3_set(v, x, luax_optfloat(L, 3, x), luax_optfloat(L, 4, x));
  } else {
    VectorType t;
    float* p = luax_tovector(L, 2, &t);
    if (p && t == V_VEC3) {
      vec3_init(v, p);
    } else if (p && t == V_MAT4) {
      vec3_set(v, p[12], p[13], p[14]);
    } else if (p && t == V_QUAT) {
      quat_getDirection(p, v);
    } else{
      luax_typeerror(L, 2, "vec3, quat, mat4, or number");
    }
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Add(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  vec3_add(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Sub(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  vec3_sub(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Mul(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  vec3_mul(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Div(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  vec3_div(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Length(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  lua_pushnumber(L, vec3_length(v));
  return 1;
}

static int l_lovrVec3Normalize(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  vec3_normalize(v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Distance(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  lua_pushnumber(L, vec3_distance(v, u));
  return 1;
}

static int l_lovrVec3Dot(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  lua_pushnumber(L, vec3_dot(v, u));
  return 1;
}

static int l_lovrVec3Cross(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  vec3_cross(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Lerp(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  int index = luax_readvec3(L, 2, u, NULL);
  float t = luax_checkfloat(L, index);
  vec3_lerp(v, u, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Angle(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float u[3];
  luax_readvec3(L, 2, u, NULL);
  lua_pushnumber(L, vec3_angle(v, u));
  return 1;
}

static int l_lovrVec3Transform(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float m[16];
  luax_readmat4(L, 2, m, 1);
  mat4_mulPoint(m, v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Rotate(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  float q[4];
  luax_readquat(L, 2, q, NULL);
  quat_rotate(q, v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3__add(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = luax_tofloat(L, 1);
    vec3 v = luax_checkvector(L, 2, V_VEC3, NULL);
    out[0] = x + v[0];
    out[1] = x + v[1];
    out[2] = x + v[2];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    float x = luax_tofloat(L, 2);
    out[0] = v[0] + x;
    out[1] = v[1] + x;
    out[2] = v[2] + x;
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    vec3_add(vec3_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec3__sub(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = luax_tofloat(L, 1);
    vec3 v = luax_checkvector(L, 2, V_VEC3, NULL);
    out[0] = x - v[0];
    out[1] = x - v[1];
    out[2] = x - v[2];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    float x = luax_tofloat(L, 2);
    out[0] = v[0] - x;
    out[1] = v[1] - x;
    out[2] = v[2] - x;
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    vec3_sub(vec3_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec3__mul(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 2, V_VEC3, NULL);
    vec3_scale(vec3_init(out, v), luax_tofloat(L, 1));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3_scale(vec3_init(out, v), luax_tofloat(L, 2));
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    vec3_mul(vec3_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec3__div(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 2, V_VEC3, NULL);
    vec3_scale(vec3_init(out, v), 1.f / luax_tofloat(L, 1));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3_scale(vec3_init(out, v), 1.f / luax_tofloat(L, 2));
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    vec3_div(vec3_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec3__unm(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  vec3 out = luax_newtempvector(L, V_VEC3);
  vec3_scale(vec3_init(out, v), -1.f);
  return 1;
}

static int l_lovrVec3__len(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  lua_pushnumber(L, vec3_length(v));
  return 1;
}

static int l_lovrVec3__tostring(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  lua_pushfstring(L, "(%f, %f, %f)", v[0], v[1], v[2]);
  return 1;
}

static int l_lovrVec3__newindex(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index == 1 || index == 2 || index == 3) {
      float x = luax_checkfloat(L, 3);
      v[index - 1] = x;
      return 0;
    }
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;
    const char* str = lua_tolstring(L, 2, &length);
    const unsigned char* key = (const unsigned char*) str;

    if (length == 1 && swizzles[3][key[0]]) {
      v[swizzles[3][key[0]] - 1] = luax_checkfloat(L, 3);
      return 0;
    } else if (length == 2 && swizzles[3][key[0]] && swizzles[3][key[1]]) {
      float* u = luax_checkvector(L, 3, V_VEC2, NULL);
      for (size_t i = 0; i < length; i++) {
        v[swizzles[3][key[i]] - 1] = u[i];
      }
      return 0;
    } else if (length == 3 && swizzles[3][key[0]] && swizzles[3][key[1]] && swizzles[3][key[2]]) {
      float* u = luax_checkvector(L, 3, V_VEC3, NULL);
      for (size_t i = 0; i < length; i++) {
        v[swizzles[3][key[i]] - 1] = u[i];
      }
      return 0;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to assign property %s of vec3 (invalid property)", lua_tostring(L, -1));
  return 0;
}

static int l_lovrVec3__index(lua_State* L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      return 1;
    } else {
      lua_pop(L, 2);
    }
  }

  float* v = luax_checkvector(L, 1, V_VEC3, NULL);
  int type = lua_type(L, 2);
  if (type == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index == 1 || index == 2 || index == 3) {
      lua_pushnumber(L, v[index - 1]);
      return 1;
    }
  } else if (type == LUA_TSTRING) {
    size_t length;
    const char* str = lua_tolstring(L, 2, &length);
    const unsigned char* key = (const unsigned char*) str;

    if (length == 1 && swizzles[3][key[0]]) {
      lua_pushnumber(L, v[swizzles[3][key[0]] - 1]);
      return 1;
    } else if (length == 2 && swizzles[3][key[0]] && swizzles[3][key[1]]) {
      float* out = luax_newtempvector(L, V_VEC2);
      out[0] = v[swizzles[3][key[0]] - 1];
      out[1] = v[swizzles[3][key[1]] - 1];
      return 1;
    } else if (length == 3 && swizzles[3][key[0]] && swizzles[3][key[1]] && swizzles[3][key[2]]) {
      float* out = luax_newtempvector(L, V_VEC3);
      out[0] = v[swizzles[3][key[0]] - 1];
      out[1] = v[swizzles[3][key[1]] - 1];
      out[2] = v[swizzles[3][key[2]] - 1];
      return 1;
    } else if (length == 4 && swizzles[3][key[0]] && swizzles[3][key[1]] && swizzles[3][key[2]] && swizzles[3][key[3]]) {
      float* out = luax_newtempvector(L, V_VEC4);
      out[0] = v[swizzles[3][key[0]] - 1];
      out[1] = v[swizzles[3][key[1]] - 1];
      out[2] = v[swizzles[3][key[2]] - 1];
      out[3] = v[swizzles[3][key[3]] - 1];
      return 1;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to index field %s of vec3 (invalid property)", lua_tostring(L, -1));
  return 0;
}

int l_lovrVec3__metaindex(lua_State* L) {
  if (lua_type(L, 2) != LUA_TSTRING) {
    return 0;
  }

  size_t length;
  const char* key = lua_tolstring(L, 2, &length);

  static const struct { StringEntry name; float x, y, z; } properties[] = {
    { ENTRY("one"), 1.f, 1.f, 1.f },
    { ENTRY("zero"), 0.f, 0.f, 0.f },
    { ENTRY("left"), -1.f, 0.f, 0.f },
    { ENTRY("right"), 1.f, 0.f, 0.f },
    { ENTRY("up"), 0.f, 1.f, 0.f },
    { ENTRY("down"), 0.f, -1.f, 0.f },
    { ENTRY("back"), 0.f, 0.f, 1.f },
    { ENTRY("forward"), 0.f, 0.f, -1.f }
  };

  for (uint32_t i = 0; i < COUNTOF(properties); i++) {
    if (length == properties[i].name.length && !memcmp(key, properties[i].name.string, length)) {
      float* v = luax_newtempvector(L, V_VEC3);
      vec3_set(v, properties[i].x, properties[i].y, properties[i].z);
      return 1;
    }
  }

  return 0;
}

const luaL_Reg lovrVec3[] = {
  { "type", l_lovrVec3Type },
  { "equals", l_lovrVec3Equals },
  { "unpack", l_lovrVec3Unpack },
  { "set", l_lovrVec3Set },
  { "add", l_lovrVec3Add },
  { "sub", l_lovrVec3Sub },
  { "mul", l_lovrVec3Mul },
  { "div", l_lovrVec3Div },
  { "length", l_lovrVec3Length },
  { "normalize", l_lovrVec3Normalize },
  { "distance", l_lovrVec3Distance },
  { "dot", l_lovrVec3Dot },
  { "cross", l_lovrVec3Cross },
  { "lerp", l_lovrVec3Lerp },
  { "angle", l_lovrVec3Angle },
  { "transform", l_lovrVec3Transform },
  { "rotate", l_lovrVec3Rotate },
  { "__add", l_lovrVec3__add },
  { "__sub", l_lovrVec3__sub },
  { "__mul", l_lovrVec3__mul },
  { "__div", l_lovrVec3__div },
  { "__unm", l_lovrVec3__unm },
  { "__len", l_lovrVec3__len },
  { "__tostring", l_lovrVec3__tostring },
  { "__newindex", l_lovrVec3__newindex },
  { "__index", l_lovrVec3__index },
  { NULL, NULL }
};

// vec4

static int l_lovrVec4Type(lua_State* L) {
  lua_pushliteral(L, "Vec4");
  return 1;
}

static int l_lovrVec4Equals(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float* u = luax_checkvector(L, 2, V_VEC4, NULL);
  lua_pushboolean(L, vec4_distance2(v, u) < EQ_THRESHOLD);
  return 1;
}

static int l_lovrVec4Unpack(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  lua_pushnumber(L, v[3]);
  return 4;
}

int l_lovrVec4Set(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  vec4_init(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Add(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  vec4_add(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Sub(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  vec4_sub(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Mul(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  vec4_mul(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Div(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  vec4_div(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Length(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  lua_pushnumber(L, vec4_length(v));
  return 1;
}

static int l_lovrVec4Normalize(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  vec4_normalize(v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Distance(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  lua_pushnumber(L, vec4_distance(v, u));
  return 1;
}

static int l_lovrVec4Dot(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  lua_pushnumber(L, vec4_dot(v, u));
  return 1;
}

static int l_lovrVec4Lerp(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  int index = luax_readvec4(L, 2, u, NULL);
  float t = luax_checkfloat(L, index);
  vec4_lerp(v, u, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Angle(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float u[4];
  luax_readvec4(L, 2, u, NULL);
  lua_pushnumber(L, vec4_angle(v, u));
  return 1;
}

static int l_lovrVec4Transform(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float m[16];
  luax_readmat4(L, 2, m, 1);
  mat4_mulVec4(m, v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4__add(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = x + u[0];
    out[1] = x + u[1];
    out[2] = x + u[2];
    out[3] = x + u[3];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] + x;
    out[1] = v[1] + x;
    out[2] = v[2] + x;
    out[3] = v[3] + x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    vec4_add(vec4_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec4__sub(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = x - u[0];
    out[1] = x - u[1];
    out[2] = x - u[2];
    out[3] = x - u[3];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] - x;
    out[1] = v[1] - x;
    out[2] = v[2] - x;
    out[3] = v[3] - x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    vec4_sub(vec4_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec4__mul(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = x * u[0];
    out[1] = x * u[1];
    out[2] = x * u[2];
    out[3] = x * u[3];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] * x;
    out[1] = v[1] * x;
    out[2] = v[2] * x;
    out[3] = v[3] * x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    vec4_mul(vec4_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec4__div(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = x / u[0];
    out[1] = x / u[1];
    out[2] = x / u[2];
    out[3] = x / u[3];
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] / x;
    out[1] = v[1] / x;
    out[2] = v[2] / x;
    out[3] = v[3] / x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC4, NULL);
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    vec4_div(vec4_init(out, v), u);
  }
  return 1;
}

static int l_lovrVec4__unm(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float* out = luax_newtempvector(L, V_VEC4);
  vec4_scale(vec4_init(out, v), -1.f);
  return 1;
}

static int l_lovrVec4__len(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  lua_pushnumber(L, vec4_length(v));
  return 1;
}

static int l_lovrVec4__tostring(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  lua_pushfstring(L, "(%f, %f, %f, %f)", v[0], v[1], v[2], v[3]);
  return 1;
}

static int l_lovrVec4__newindex(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index >= 1 && index <= 4) {
      float x = luax_checkfloat(L, 3);
      v[index - 1] = x;
      return 0;
    }
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;
    const char* str = lua_tolstring(L, 2, &length);
    const unsigned char* key = (const unsigned char*) str;

    if (length == 1 && swizzles[4][key[0]]) {
      v[swizzles[4][key[0]] - 1] = luax_checkfloat(L, 3);
      return 0;
    } else if (length == 2 && swizzles[4][key[0]] && swizzles[4][key[1]]) {
      float* u = luax_checkvector(L, 3, V_VEC2, NULL);
      for (size_t i = 0; i < length; i++) {
        v[swizzles[4][key[i]] - 1] = u[i];
      }
      return 0;
    } else if (length == 3 && swizzles[4][key[0]] && swizzles[4][key[1]] && swizzles[4][key[2]]) {
      float* u = luax_checkvector(L, 3, V_VEC3, NULL);
      for (size_t i = 0; i < length; i++) {
        v[swizzles[4][key[i]] - 1] = u[i];
      }
      return 0;
    } else if (length == 4 && swizzles[4][key[0]] && swizzles[4][key[1]] && swizzles[4][key[2]] && swizzles[4][key[3]]) {
      float* u = luax_checkvector(L, 3, V_VEC4, NULL);
      for (size_t i = 0; i < length; i++) {
        v[swizzles[4][key[i]] - 1] = u[i];
      }
      return 0;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to assign property %s of vec4 (invalid property)", lua_tostring(L, -1));
  return 0;
}

static int l_lovrVec4__index(lua_State* L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      return 1;
    } else {
      lua_pop(L, 2);
    }
  }

  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  int type = lua_type(L, 2);
  if (type == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index >= 1 && index <= 4) {
      lua_pushnumber(L, v[index - 1]);
      return 1;
    }
  } else if (type == LUA_TSTRING) {
    size_t length;
    const char* str = lua_tolstring(L, 2, &length);
    const unsigned char* key = (const unsigned char*) str;

    if (length == 1 && swizzles[4][key[0]]) {
      lua_pushnumber(L, v[swizzles[4][key[0]] - 1]);
      return 1;
    } else if (length == 2 && swizzles[4][key[0]] && swizzles[4][key[1]]) {
      float* out = luax_newtempvector(L, V_VEC2);
      out[0] = v[swizzles[4][key[0]] - 1];
      out[1] = v[swizzles[4][key[1]] - 1];
      return 1;
    } else if (length == 3 && swizzles[4][key[0]] && swizzles[4][key[1]] && swizzles[4][key[2]]) {
      float* out = luax_newtempvector(L, V_VEC3);
      out[0] = v[swizzles[4][key[0]] - 1];
      out[1] = v[swizzles[4][key[1]] - 1];
      out[2] = v[swizzles[4][key[2]] - 1];
      return 1;
    } else if (length == 4 && swizzles[4][key[0]] && swizzles[4][key[1]] && swizzles[4][key[2]] && swizzles[4][key[3]]) {
      float* out = luax_newtempvector(L, V_VEC4);
      out[0] = v[swizzles[4][key[0]] - 1];
      out[1] = v[swizzles[4][key[1]] - 1];
      out[2] = v[swizzles[4][key[2]] - 1];
      out[3] = v[swizzles[4][key[3]] - 1];
      return 1;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to index field %s of vec4 (invalid property)", lua_tostring(L, -1));
  return 0;
}

int l_lovrVec4__metaindex(lua_State* L) {
  if (lua_type(L, 2) != LUA_TSTRING) {
    return 0;
  }

  size_t length;
  const char* key = lua_tolstring(L, 2, &length);

  static const struct { StringEntry name; float x, y, z, w; } properties[] = {
    { ENTRY("one"), 1.f, 1.f, 1.f, 1.f },
    { ENTRY("zero"), 0.f, 0.f, 0.f, 0.f }
  };

  for (uint32_t i = 0; i < COUNTOF(properties); i++) {
    if (length == properties[i].name.length && !memcmp(key, properties[i].name.string, length)) {
      float* v = luax_newtempvector(L, V_VEC4);
      v[0] = properties[i].x;
      v[1] = properties[i].y;
      v[2] = properties[i].z;
      v[3] = properties[i].w;
      return 1;
    }
  }

  return 0;
}

const luaL_Reg lovrVec4[] = {
  { "type", l_lovrVec4Type },
  { "equals", l_lovrVec4Equals },
  { "unpack", l_lovrVec4Unpack },
  { "set", l_lovrVec4Set },
  { "add", l_lovrVec4Add },
  { "sub", l_lovrVec4Sub },
  { "mul", l_lovrVec4Mul },
  { "div", l_lovrVec4Div },
  { "length", l_lovrVec4Length },
  { "normalize", l_lovrVec4Normalize },
  { "distance", l_lovrVec4Distance },
  { "dot", l_lovrVec4Dot },
  { "lerp", l_lovrVec4Lerp },
  { "angle", l_lovrVec4Angle },
  { "transform", l_lovrVec4Transform },
  { "__add", l_lovrVec4__add },
  { "__sub", l_lovrVec4__sub },
  { "__mul", l_lovrVec4__mul },
  { "__div", l_lovrVec4__div },
  { "__unm", l_lovrVec4__unm },
  { "__len", l_lovrVec4__len },
  { "__tostring", l_lovrVec4__tostring },
  { "__newindex", l_lovrVec4__newindex },
  { "__index", l_lovrVec4__index },
  { NULL, NULL }
};

// quat

static int l_lovrQuatType(lua_State* L) {
  lua_pushliteral(L, "Quat");
  return 1;
}

static int l_lovrQuatEquals(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  quat r = luax_checkvector(L, 2, V_QUAT, NULL);
  float dot = q[0] * r[0] + q[1] * r[1] + q[2] * r[2] + q[3] * r[3];
  bool equal = fabsf(dot) >= 1.f - 1e-5f;
  lua_pushboolean(L, equal);
  return 1;
}

static int l_lovrQuatUnpack(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
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
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  if (lua_isnoneornil(L, 2)) {
    quat_identity(q);
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
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
    VectorType type;
    float* p = luax_tovector(L, 2, &type);
    if (!p) return luax_typeerror(L, 2, "vec3, quat, or number");

    if (type == V_VEC3) {
      if (lua_gettop(L) > 2) {
        vec3 u = luax_checkvector(L, 3, V_VEC3, "vec3");
        quat_between(q, p, u);
      } else {
        float forward[3] = { 0.f, 0.f, -1.f };
        quat_between(q, forward, p);
      }
    } else if (type == V_QUAT) {
      quat_init(q, p);
    } else if (type == V_MAT4) {
      quat_fromMat4(q, p);
    } else {
      return luax_typeerror(L, 2, "vec3, quat, mat4, or number");
    }
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuatMul(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  VectorType type;
  float* r = luax_tovector(L, 2, &type);
  if (r && type == V_VEC3) {
    vec3 v = luax_newtempvector(L, V_VEC3);
    quat_rotate(q, vec3_init(v, r));
  } else if (r && type == V_QUAT) {
    quat_mul(q, q, r);
    lua_settop(L, 1);
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    lua_settop(L, 4);
    vec3 v = luax_newtempvector(L, V_VEC3);
    v[0] = luax_tofloat(L, 2);
    v[1] = luax_checkfloat(L, 3);
    v[2] = luax_checkfloat(L, 4);
    quat_rotate(q, v);
  } else {
    return luax_typeerror(L, 2, "number, vec3, or quat");
  }
  return 1;
}

static int l_lovrQuatLength(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  lua_pushnumber(L, quat_length(q));
  return 1;
}

static int l_lovrQuatNormalize(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  quat_normalize(q);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuatDirection(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  vec3 v = luax_newtempvector(L, V_VEC3);
  quat_getDirection(q, v);
  return 1;
}

static int l_lovrQuatConjugate(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  quat_conjugate(q);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuatSlerp(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  quat r = luax_checkvector(L, 2, V_QUAT, NULL);
  float t = luax_checkfloat(L, 3);
  quat_slerp(q, r, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuatGetEuler(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  float pitch, yaw, roll;
  quat_getEuler(q, &pitch, &yaw, &roll);
  lua_pushnumber(L, pitch);
  lua_pushnumber(L, yaw);
  lua_pushnumber(L, roll);
  return 3;
}

static int l_lovrQuatSetEuler(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  float pitch = luax_checkfloat(L, 2);
  float yaw = luax_checkfloat(L, 3);
  float roll = luax_checkfloat(L, 4);
  quat_setEuler(q, pitch, yaw, roll);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrQuat__mul(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  VectorType type;
  float* r = luax_tovector(L, 2, &type);
  if (!r) return luax_typeerror(L, 2, "quat or vec3");
  if (type == V_VEC3) {
    vec3 out = luax_newtempvector(L, V_VEC3);
    quat_rotate(q, vec3_init(out, r));
  } else {
    quat out = luax_newtempvector(L, V_QUAT);
    quat_mul(out, q, r);
  }
  return 1;
}

static int l_lovrQuat__len(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  lua_pushnumber(L, quat_length(q));
  return 1;
}

static int l_lovrQuat__tostring(lua_State* L) {
  quat q = luax_checkvector(L, 1, V_QUAT, NULL);
  lua_pushfstring(L, "(%f, %f, %f, %f)", q[0], q[1], q[2], q[3]);
  return 1;
}

static int l_lovrQuat__newindex(lua_State* L) {
  float* q = luax_checkvector(L, 1, V_QUAT, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index == 1 || index == 2 || index == 3 || index == 4) {
      float x = luax_checkfloat(L, 3);
      q[index - 1] = x;
      return 0;
    }
  } else if (lua_type(L, 2) == LUA_TSTRING) {
    size_t length;
    const char* key = lua_tolstring(L, 2, &length);
    float x = luax_checkfloat(L, 3);
    if (length == 1 && key[0] >= 'w' && key[0] <= 'z') {
      int index = key[0] == 'w' ? 3 : key[0] - 'x';
      q[index] = x;
      return 0;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to assign property %s of quat (invalid property)", lua_tostring(L, -1));
  return 0;
}

static int l_lovrQuat__index(lua_State* L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      return 1;
    } else {
      lua_pop(L, 2);
    }
  }

  float* q = luax_checkvector(L, 1, V_QUAT, NULL);
  int type = lua_type(L, 2);
  if (type == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index == 1 || index == 2 || index == 3 || index == 4) {
      lua_pushnumber(L, q[index - 1]);
      return 1;
    }
  } else if (type == LUA_TSTRING) {
    size_t length;
    const char* key = lua_tolstring(L, 2, &length);
    if (length == 1 && key[0] >= 'w' && key[0] <= 'z') {
      int index = key[0] == 'w' ? 3 : key[0] - 'x';
      lua_pushnumber(L, q[index]);
      return 1;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to index field %s of quat (invalid property)", lua_tostring(L, -1));
  return 0;
}

int l_lovrQuat__metaindex(lua_State* L) {
  if (lua_type(L, 2) != LUA_TSTRING) {
    return 0;
  }

  size_t length;
  const char* key = lua_tolstring(L, 2, &length);

  static const struct { StringEntry name; float x, y, z, w; } properties[] = {
    { ENTRY("identity"), 0.f, 0.f, 0.f, 1.f }
  };

  for (uint32_t i = 0; i < COUNTOF(properties); i++) {
    if (length == properties[i].name.length && !memcmp(key, properties[i].name.string, length)) {
      float* q = luax_newtempvector(L, V_QUAT);
      quat_set(q, properties[i].x, properties[i].y, properties[i].z, properties[i].w);
      return 1;
    }
  }

  return 0;
}

const luaL_Reg lovrQuat[] = {
  { "type", l_lovrQuatType },
  { "equals", l_lovrQuatEquals },
  { "unpack", l_lovrQuatUnpack },
  { "set", l_lovrQuatSet },
  { "mul", l_lovrQuatMul },
  { "length", l_lovrQuatLength },
  { "normalize", l_lovrQuatNormalize },
  { "direction", l_lovrQuatDirection },
  { "conjugate", l_lovrQuatConjugate },
  { "slerp", l_lovrQuatSlerp },
  { "getEuler", l_lovrQuatGetEuler },
  { "setEuler", l_lovrQuatSetEuler },
  { "__mul", l_lovrQuat__mul },
  { "__len", l_lovrQuat__len },
  { "__tostring", l_lovrQuat__tostring },
  { "__newindex", l_lovrQuat__newindex },
  { "__index", l_lovrQuat__index },
  { NULL, NULL }
};

// mat4

static int l_lovrMat4Type(lua_State* L) {
  lua_pushliteral(L, "Mat4");
  return 1;
}

static int l_lovrMat4Equals(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  mat4 n = luax_checkvector(L, 2, V_MAT4, NULL);
  for (int i = 0; i < 16; i += 4) {
    float dx = m[i + 0] - n[i + 0];
    float dy = m[i + 1] - n[i + 1];
    float dz = m[i + 2] - n[i + 2];
    float dw = m[i + 3] - n[i + 3];
    float distance2 = dx * dx + dy * dy + dz * dz + dw * dw;
    if (distance2 > EQ_THRESHOLD) {
      lua_pushboolean(L, false);
      return 1;
    }
  }
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrMat4Unpack(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
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
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  float position[3];
  mat4_getPosition(m, position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrMat4GetOrientation(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  float angle, ax, ay, az;
  mat4_getAngleAxis(m, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrMat4GetScale(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  float scale[3];
  mat4_getScale(m, scale);
  lua_pushnumber(L, scale[0]);
  lua_pushnumber(L, scale[1]);
  lua_pushnumber(L, scale[2]);
  return 3;
}

static int l_lovrMat4GetPose(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
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
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
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
    VectorType vectorType;
    float* n = luax_tovector(L, 2, &vectorType);
    if (vectorType == V_MAT4) {
      mat4_init(m, n);
    } else {
      int index = 2;
      mat4_identity(m);

      float position[3];
      index = luax_readvec3(L, index, position, "nil, number, vec3, or mat4");
      m[12] = position[0];
      m[13] = position[1];
      m[14] = position[2];

      float* v = luax_tovector(L, index, &vectorType);
      if (vectorType == V_QUAT) {
        mat4_rotateQuat(m, v);
      } else if ((top - index) == 3 && lua_type(L, top) == LUA_TNUMBER) {
        float angle = luax_checkfloat(L, index++);
        float ax = luax_checkfloat(L, index++);
        float ay = luax_checkfloat(L, index++);
        float az = luax_checkfloat(L, index++);
        mat4_rotate(m, angle, ax, ay, az);
      } else {
        float sx, sy, sz;

        if (vectorType == V_VEC3) {
          sx = v[0];
          sy = v[1];
          sz = v[2];
          index++;
        } else if (lua_type(L, index) == LUA_TNUMBER) {
          sx = luax_checkfloat(L, index++);
          sy = luax_checkfloat(L, index++);
          sz = luax_checkfloat(L, index++);
        } else {
          sx = sy = sz = 1.f;
        }

        float rotation[4];
        luax_readquat(L, index, rotation, NULL);
        mat4_rotateQuat(m, rotation);
        mat4_scale(m, sx, sy, sz);
      }
    }
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Mul(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  VectorType type;
  float* n = luax_tovector(L, 2, &type);
  if (n && type == V_MAT4) {
    mat4_mul(m, n);
    lua_settop(L, 1);
  } else if (n && type == V_VEC3) {
    vec3 v = luax_newtempvector(L, V_VEC3);
    mat4_mulPoint(m, vec3_init(v, n));
  } else if (n && type == V_VEC4) {
    vec4 v = luax_newtempvector(L, V_VEC4);
    mat4_mulVec4(m, vec4_init(v, n));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    lua_settop(L, 4);
    vec3 v = luax_newtempvector(L, V_VEC3);
    v[0] = luax_tofloat(L, 2);
    v[1] = luax_checkfloat(L, 3);
    v[2] = luax_checkfloat(L, 4);
    mat4_mulPoint(m, v);
  } else {
    return luax_typeerror(L, 2, "mat4, vec3, vec4, or number");
  }
  return 1;
}

static int l_lovrMat4Identity(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  mat4_identity(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Invert(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  mat4_invert(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Transpose(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  mat4_transpose(m);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Translate(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    mat4_translate(m, luax_checkfloat(L, 2), luax_checkfloat(L, 3), luax_checkfloat(L, 4));
  } else {
    float* v = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    mat4_translate(m, v[0], v[1], v[2]);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Rotate(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    mat4_rotate(m, luax_checkfloat(L, 2), luax_optfloat(L, 3, 0.f), luax_optfloat(L, 4, 1.f), luax_optfloat(L, 5, 0.f));
  } else {
    float* q = luax_checkvector(L, 2, V_QUAT, "quat or number");
    mat4_rotateQuat(m, q);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Scale(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = luax_checkfloat(L, 2);
    mat4_scale(m, x, luax_optfloat(L, 3, x), luax_optfloat(L, 4, x));
  } else {
    float* s = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    mat4_scale(m, s[0], s[1], s[2]);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Orthographic(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
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
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  float fovy = luax_checkfloat(L, 2);
  float aspect = luax_checkfloat(L, 3);
  float n = luax_checkfloat(L, 4);
  float f = luax_optfloat(L, 5, 0.);
  mat4_perspective(m, fovy, aspect, n, f);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Fov(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
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
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  vec3 from = luax_checkvector(L, 2, V_VEC3, NULL);
  vec3 to = luax_checkvector(L, 3, V_VEC3, NULL);
  vec3 up = lua_isnoneornil(L, 4) ? (float[3]) { 0.f, 1.f, 0.f } : luax_checkvector(L, 4, V_VEC3, NULL);
  mat4_lookAt(m, from, to, up);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Target(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  vec3 from = luax_checkvector(L, 2, V_VEC3, NULL);
  vec3 to = luax_checkvector(L, 3, V_VEC3, NULL);
  vec3 up = lua_isnoneornil(L, 4) ? (float[3]) { 0.f, 1.f, 0.f } : luax_checkvector(L, 4, V_VEC3, NULL);
  mat4_target(m, from, to, up);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Reflect(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  vec3 position = luax_checkvector(L, 2, V_VEC3, NULL);
  vec3 normal = luax_checkvector(L, 3, V_VEC3, NULL);
  mat4_reflect(m, position, normal);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4__mul(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  VectorType type;
  float* n = luax_tovector(L, 2, &type);
  if (!n || (type == V_VEC2 || type == V_QUAT)) return luax_typeerror(L, 2, "mat4, vec3, or vec4");
  if (type == V_MAT4) {
    mat4 out = luax_newtempvector(L, V_MAT4);
    mat4_mul(mat4_init(out, m), n);
  } else if (type == V_VEC3) {
    vec3 out = luax_newtempvector(L, V_VEC3);
    vec3_init(out, n);
    mat4_mulPoint(m, out);
  } else if (type == V_VEC4) {
    float* out = luax_newtempvector(L, V_VEC4);
    memcpy(out, n, 4 * sizeof(float));
    mat4_mulVec4(m, out);
  } else {
    lovrUnreachable();
  }
  return 1;
}

static int l_lovrMat4__tostring(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  const char* format = "(%f, %f, %f, %f,\n %f, %f, %f, %f,\n %f, %f, %f, %f,\n %f, %f, %f, %f)";
  lua_pushfstring(L, format,
    m[0], m[4], m[8], m[12],
    m[1], m[5], m[9], m[13],
    m[2], m[6], m[10], m[14],
    m[3], m[7], m[11], m[15]);
  return 1;
}

static int l_lovrMat4__newindex(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index >= 1 && index <= 16) {
      m[index - 1] = luax_checkfloat(L, 3);
      return 0;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to assign property %s of mat4 (invalid property)", lua_tostring(L, -1));
  return 0;
}

static int l_lovrMat4__index(lua_State* L) {
  if (lua_type(L, 1) == LUA_TUSERDATA) {
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
      return 1;
    } else {
      lua_pop(L, 2);
    }
  }

  float* m = luax_checkvector(L, 1, V_MAT4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    int index = lua_tointeger(L, 2);
    if (index >= 1 && index <= 16) {
      lua_pushnumber(L, m[index - 1]);
      return 1;
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  luaL_error(L, "attempt to index field %s of mat4 (invalid property)", lua_tostring(L, -1));
  return 0;
}

int l_lovrMat4__metaindex(lua_State* L) {
  return 0; // No properties currently, 'identity' is already taken
}

const luaL_Reg lovrMat4[] = {
  { "type", l_lovrMat4Type },
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
  { "__newindex", l_lovrMat4__newindex },
  { "__index", l_lovrMat4__index },
  { NULL, NULL }
};
