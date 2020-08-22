#include "api.h"
#include "math/math.h"
#include "core/maf.h"

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

int luax_readvec3(lua_State* L, int index, vec3 v, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = 0.f;
      return ++index;
    case LUA_TNUMBER:
      v[0] = luax_optfloat(L, index++, 0.f);
      v[1] = luax_optfloat(L, index++, 0.f);
      v[2] = luax_optfloat(L, index++, 0.f);
      return index;
    default:
      vec3_init(v, luax_checkvector(L, index++, V_VEC3, expected ? expected : "vec3 or number"));
      return index;
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
        v[0] = v[1] = v[2] = luax_optfloat(L, index++, 0.f);
      } else {
        v[0] = 1.f;
        for (int i = 0; i < components; i++) {
          v[i] = luax_optfloat(L, index++, v[0]);
        }
      }
      return index;
    default:
      vec3_init(v, luax_checkvector(L, index++, V_VEC3, expected ? expected : "vec3 or number"));
      return index;
  }
}

int luax_readquat(lua_State* L, int index, quat q, const char* expected) {
  float angle, ax, ay, az;
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      quat_set(q, 0.f, 0.f, 0.f, 1.f);
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
      float S[4];
      float R[4];
      mat4_identity(m);
      index = luax_readvec3(L, index, m + 12, "mat4, vec3, or number");
      index = luax_readscale(L, index, S, scaleComponents, NULL);
      index = luax_readquat(L, index, R, NULL);
      mat4_rotateQuat(m, R);
      mat4_scale(m, S[0], S[1], S[2]);
      m[15] = 1.f;
      return index;
    }
  }
}

// vec2

static int l_lovrVec2Unpack(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  return 2;
}

int l_lovrVec2Set(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  if (lua_isnoneornil(L, 2) || lua_type(L, 2) == LUA_TNUMBER) {
    v[0] = luax_optfloat(L, 2, 0.f);
    v[1] = luax_optfloat(L, 3, v[0]);
  } else {
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    v[0] = u[0];
    v[1] = u[1];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Add(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] += x;
    v[1] += x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    v[0] += u[0];
    v[1] += u[1];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Sub(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] -= x;
    v[1] -= x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    v[0] -= u[0];
    v[1] -= u[1];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Mul(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] *= x;
    v[1] *= x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    v[0] *= u[0];
    v[1] *= u[1];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Div(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] /= x;
    v[1] /= x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    v[0] /= u[0];
    v[1] /= u[1];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Length(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushnumber(L, sqrtf(v[0] * v[0] + v[1] * v[1]));
  return 1;
}

static int l_lovrVec2Normalize(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float length = v[0] * v[0] + v[1] * v[1];
  if (length != 0.f) {
    length = 1.f / sqrtf(length);
    v[0] *= length;
    v[1] *= length;
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2Distance(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float* u = luax_checkvector(L, 2, V_VEC2, NULL);
  float dx = v[0] - u[0];
  float dy = v[1] - u[1];
  lua_pushnumber(L, sqrtf(dx * dx + dy * dy));
  return 1;
}

static int l_lovrVec2Dot(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float* u = luax_checkvector(L, 2, V_VEC2, NULL);
  lua_pushnumber(L, v[0] * u[0] + v[1] * u[1]);
  return 1;
}

static int l_lovrVec2Lerp(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float* u = luax_checkvector(L, 2, V_VEC2, NULL);
  float t = luax_checkfloat(L, 3);
  v[0] = v[0] + (u[0] - v[0]) * t;
  v[1] = v[1] + (u[1] - v[1]) * t;
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec2__add(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = u[0] + x;
    out[1] = u[1] + x;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] + x;
    out[1] = v[1] + x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    out[0] = v[0] + u[0];
    out[1] = v[1] + u[1];
  }
  return 1;
}

static int l_lovrVec2__sub(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = u[0] - x;
    out[1] = u[1] - x;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] - x;
    out[1] = v[1] - x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    out[0] = v[0] - u[0];
    out[1] = v[1] - u[1];
  }
  return 1;
}

static int l_lovrVec2__mul(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = u[0] * x;
    out[1] = u[1] * x;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] * x;
    out[1] = v[1] * x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    out[0] = v[0] * u[0];
    out[1] = v[1] * u[1];
  }
  return 1;
}

static int l_lovrVec2__div(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC2);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC2, NULL);
    out[0] = u[0] / x;
    out[1] = u[1] / x;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] / x;
    out[1] = v[1] / x;
  } else {
    float* v = luax_checkvector(L, 1, V_VEC2, NULL);
    float* u = luax_checkvector(L, 2, V_VEC2, "vec2 or number");
    out[0] = v[0] / u[0];
    out[1] = v[1] / u[1];
  }
  return 1;
}

static int l_lovrVec2__unm(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  float* out = luax_newtempvector(L, V_VEC2);
  out[0] = -v[0];
  out[1] = -v[1];
  return 1;
}

static int l_lovrVec2__len(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC2, NULL);
  lua_pushnumber(L, sqrtf(v[0] * v[0] + v[1] * v[1]));
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
  return luaL_error(L, "attempt to assign property %s of vec2 (invalid property)", lua_tostring(L, -1));
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
  return luaL_error(L, "attempt to index field %s of vec2 (invalid property)", lua_tostring(L, -1));
}

const luaL_Reg lovrVec2[] = {
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
    } else{
      luax_typeerror(L, 2, "vec3, mat4, or number");
    }
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Add(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] += x, v[1] += x, v[2] += x, v[3] += x;
  } else {
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    vec3_add(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Sub(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] -= x, v[1] -= x, v[2] -= x, v[3] -= x;
  } else {
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    vec3_sub(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Mul(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_scale(v, lua_tonumber(L, 2));
  } else {
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    v[0] = v[0] * u[0], v[1] = v[1] * u[1], v[2] = v[2] * u[2];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Div(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_scale(v, 1.f / (float) lua_tonumber(L, 2));
  } else {
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    v[0] = v[0] / u[0], v[1] = v[1] / u[1], v[2] = v[2] / u[2];
  }
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
  vec3 u = luax_checkvector(L, 2, V_VEC3, NULL);
  lua_pushnumber(L, vec3_distance(v, u));
  return 1;
}

static int l_lovrVec3Dot(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  vec3 u = luax_checkvector(L, 2, V_VEC3, NULL);
  lua_pushnumber(L, vec3_dot(v, u));
  return 1;
}

static int l_lovrVec3Cross(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  vec3 u = luax_checkvector(L, 2, V_VEC3, NULL);
  vec3_cross(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Lerp(lua_State* L) {
  vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
  vec3 u = luax_checkvector(L, 2, V_VEC3, NULL);
  float t = luax_checkfloat(L, 3);
  vec3_lerp(v, u, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3__add(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    vec3 v = luax_checkvector(L, 2, V_VEC3, NULL);
    out[0] = v[0] + x;
    out[1] = v[1] + x;
    out[2] = v[2] + x;
    out[3] = v[3] + x;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] + x;
    out[1] = v[1] + x;
    out[2] = v[2] + x;
    out[3] = v[3] + x;
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    out[0] = v[0] + u[0], out[1] = v[1] + u[1], out[2] = v[2] + u[2], out[3] = v[3] + u[3];
  }
  return 1;
}

static int l_lovrVec3__sub(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    vec3 v = luax_checkvector(L, 2, V_VEC3, NULL);
    out[0] = v[0] - x;
    out[1] = v[1] - x;
    out[2] = v[2] - x;
    out[3] = v[3] - x;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    float x = lua_tonumber(L, 2);
    out[0] = v[0] - x;
    out[1] = v[1] - x;
    out[2] = v[2] - x;
    out[3] = v[3] - x;
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    out[0] = v[0] - u[0], out[1] = v[1] - u[1], out[2] = v[2] - u[2], out[3] = v[3] - u[3];
  }
  return 1;
}

static int l_lovrVec3__mul(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    vec3 u = luax_checkvector(L, 2, V_VEC3, NULL);
    vec3_scale(vec3_init(out, u), lua_tonumber(L, 1));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3_scale(vec3_init(out, v), lua_tonumber(L, 2));
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    out[0] = v[0] * u[0], out[1] = v[1] * u[1], out[2] = v[2] * u[2], out[3] = v[3] * u[3];
  }
  return 1;
}

static int l_lovrVec3__div(lua_State* L) {
  vec3 out = luax_newtempvector(L, V_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    vec3 u = luax_checkvector(L, 2, V_VEC3, NULL);
    vec3_init(out, u);
    vec3_scale(out, 1. / lua_tonumber(L, 1));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3_init(out, v);
    vec3_scale(out, 1. / lua_tonumber(L, 2));
  } else {
    vec3 v = luax_checkvector(L, 1, V_VEC3, NULL);
    vec3 u = luax_checkvector(L, 2, V_VEC3, "vec3 or number");
    out[0] = v[0] / u[0], out[1] = v[1] / u[1], out[2] = v[2] / u[2], out[3] = v[3] / u[3];
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
    }
  }
  lua_getglobal(L, "tostring");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  return luaL_error(L, "attempt to assign property %s of vec3 (invalid property)", lua_tostring(L, -1));
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
  return luaL_error(L, "attempt to index field %s of vec3 (invalid property)", lua_tostring(L, -1));
}

const luaL_Reg lovrVec3[] = {
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
  if (lua_isnoneornil(L, 2) || lua_type(L, 2) == LUA_TNUMBER) {
    v[0] = luax_optfloat(L, 2, 0.f);
    v[1] = luax_optfloat(L, 3, v[0]);
    v[2] = luax_optfloat(L, 4, v[0]);
    v[3] = luax_optfloat(L, 5, v[0]);
  } else {
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Add(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] += x;
    v[1] += x;
    v[2] += x;
    v[3] += x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    v[0] += u[0];
    v[1] += u[1];
    v[2] += u[2];
    v[3] += u[3];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Sub(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] -= x;
    v[1] -= x;
    v[2] -= x;
    v[3] -= x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    v[0] -= u[0];
    v[1] -= u[1];
    v[2] -= u[2];
    v[3] -= u[3];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Mul(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] *= x;
    v[1] *= x;
    v[2] *= x;
    v[3] *= x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    v[0] *= u[0];
    v[1] *= u[1];
    v[2] *= u[2];
    v[3] *= u[3];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Div(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    v[0] /= x;
    v[1] /= x;
    v[2] /= x;
    v[3] /= x;
  } else {
    float* u = luax_checkvector(L, 2, V_VEC4, "vec4 or number");
    v[0] /= u[0];
    v[1] /= u[1];
    v[2] /= u[2];
    v[3] /= u[3];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Length(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  lua_pushnumber(L, sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]));
  return 1;
}

static int l_lovrVec4Normalize(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
  if (length != 0.f) {
    length = 1.f / sqrtf(length);
    v[0] *= length;
    v[1] *= length;
    v[2] *= length;
    v[3] *= length;
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4Distance(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float* u = luax_checkvector(L, 2, V_VEC4, NULL);
  float dx = v[0] - u[0];
  float dy = v[1] - u[1];
  float dz = v[2] - u[2];
  float dw = v[3] - u[3];
  lua_pushnumber(L, sqrtf(dx * dx + dy * dy + dz * dz + dw * dw));
  return 1;
}

static int l_lovrVec4Dot(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float* u = luax_checkvector(L, 2, V_VEC4, NULL);
  lua_pushnumber(L, v[0] * u[0] + v[1] * u[1] + v[2] * u[2] + v[3] * u[3]);
  return 1;
}

static int l_lovrVec4Lerp(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float* u = luax_checkvector(L, 2, V_VEC4, NULL);
  float t = luax_checkfloat(L, 3);
  v[0] = v[0] + (u[0] - v[0]) * t;
  v[1] = v[1] + (u[1] - v[1]) * t;
  v[2] = v[2] + (u[2] - v[2]) * t;
  v[3] = v[3] + (u[3] - v[3]) * t;
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec4__add(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = u[0] + x;
    out[1] = u[1] + x;
    out[2] = u[2] + x;
    out[3] = u[3] + x;
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
    out[0] = v[0] + u[0];
    out[1] = v[1] + u[1];
    out[2] = v[2] + u[2];
    out[3] = v[3] + u[3];
  }
  return 1;
}

static int l_lovrVec4__sub(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = u[0] - x;
    out[1] = u[1] - x;
    out[2] = u[2] - x;
    out[3] = u[3] - x;
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
    out[0] = v[0] - u[0];
    out[1] = v[1] - u[1];
    out[2] = v[2] - u[2];
    out[3] = v[3] - u[3];
  }
  return 1;
}

static int l_lovrVec4__mul(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = u[0] * x;
    out[1] = u[1] * x;
    out[2] = u[2] * x;
    out[3] = u[3] * x;
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
    out[0] = v[0] * u[0];
    out[1] = v[1] * u[1];
    out[2] = v[2] * u[2];
    out[3] = v[3] * u[3];
  }
  return 1;
}

static int l_lovrVec4__div(lua_State* L) {
  float* out = luax_newtempvector(L, V_VEC4);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 1);
    float* u = luax_checkvector(L, 2, V_VEC4, NULL);
    out[0] = u[0] / x;
    out[1] = u[1] / x;
    out[2] = u[2] / x;
    out[3] = u[3] / x;
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
    out[0] = v[0] / u[0];
    out[1] = v[1] / u[1];
    out[2] = v[2] / u[2];
    out[3] = v[3] / u[3];
  }
  return 1;
}

static int l_lovrVec4__unm(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  float* out = luax_newtempvector(L, V_VEC4);
  out[0] = -v[0];
  out[1] = -v[1];
  out[2] = -v[2];
  out[3] = -v[3];
  return 1;
}

static int l_lovrVec4__len(lua_State* L) {
  float* v = luax_checkvector(L, 1, V_VEC4, NULL);
  lua_pushnumber(L, sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]));
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
  return luaL_error(L, "attempt to assign property %s of vec4 (invalid property)", lua_tostring(L, -1));
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
  return luaL_error(L, "attempt to index field %s of vec4 (invalid property)", lua_tostring(L, -1));
}

const luaL_Reg lovrVec4[] = {
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
    quat_set(q, 0.f, 0.f, 0.f, 1.f);
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
        quat_between(q, (float[4]) { 0.f, 0.f, -1.f }, p);
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
  if (!r || (type != V_VEC3 && type != V_QUAT)) return luax_typeerror(L, 2, "quat or vec3");
  if (type == V_VEC3) {
    quat_rotate(q, r);
    lua_settop(L, 2);
  } else {
    quat_mul(q, q, r);
    lua_settop(L, 1);
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
  return luaL_error(L, "attempt to assign property %s of quat (invalid property)", lua_tostring(L, -1));
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
  return luaL_error(L, "attempt to index field %s of quat (invalid property)", lua_tostring(L, -1));
}

const luaL_Reg lovrQuat[] = {
  { "unpack", l_lovrQuatUnpack },
  { "set", l_lovrQuatSet },
  { "mul", l_lovrQuatMul },
  { "length", l_lovrQuatLength },
  { "normalize", l_lovrQuatNormalize },
  { "direction", l_lovrQuatDirection },
  { "conjugate", l_lovrQuatConjugate },
  { "slerp", l_lovrQuatSlerp },
  { "__mul", l_lovrQuat__mul },
  { "__len", l_lovrQuat__len },
  { "__tostring", l_lovrQuat__tostring },
  { "__newindex", l_lovrQuat__newindex },
  { "__index", l_lovrQuat__index },
  { NULL, NULL }
};

// mat4

static int l_lovrMat4Unpack(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  if (lua_toboolean(L, 2)) {
    for (int i = 0; i < 16; i++) {
      lua_pushnumber(L, m[i]);
    }
    return 16;
  } else {
    float position[4], scale[4], angle, ax, ay, az;
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

      float LOVR_ALIGN(16) position[4];
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
        if (vectorType == V_VEC3) {
          m[0] = v[0];
          m[5] = v[1];
          m[10] = v[2];
          index++;
        } else if (lua_type(L, index) == LUA_TNUMBER) {
          m[0] = luax_checkfloat(L, index++);
          m[5] = luax_checkfloat(L, index++);
          m[10] = luax_checkfloat(L, index++);
        }

        float LOVR_ALIGN(16) rotation[4];
        luax_readquat(L, index, rotation, NULL);
        mat4_rotateQuat(m, rotation);
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
    mat4_multiply(m, n);
    lua_settop(L, 1);
    return 1;
  } else if (n && type == V_VEC3) {
    mat4_transform(m, n);
    lua_settop(L, 2);
    return 1;
  } else if (n && type == V_VEC4) {
    mat4_multiplyVec4(m, n);
    lua_settop(L, 2);
    return 1;
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = luaL_checknumber(L, 2);
    float y = luaL_optnumber(L, 3, 0.f);
    float z = luaL_optnumber(L, 4, 0.f);
    float v[4] = { x, y, z };
    mat4_transform(m, v);
    lua_pushnumber(L, v[0]);
    lua_pushnumber(L, v[1]);
    lua_pushnumber(L, v[2]);
    return 3;
  } else {
    return luax_typeerror(L, 2, "mat4, vec3, or number");
  }
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

static int l_lovrMat4Perspective(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  float clipNear = luax_checkfloat(L, 2);
  float clipFar = luax_checkfloat(L, 3);
  float fov = luax_checkfloat(L, 4);
  float aspect = luax_checkfloat(L, 5);
  mat4_perspective(m, clipNear, clipFar, fov, aspect);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4Fov(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  float left = luax_checkfloat(L, 2);
  float right = luax_checkfloat(L, 3);
  float up = luax_checkfloat(L, 4);
  float down = luax_checkfloat(L, 5);
  float clipNear = luax_checkfloat(L, 6);
  float clipFar = luax_checkfloat(L, 7);
  mat4_fov(m, left, right, up, down, clipNear, clipFar);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrMat4LookAt(lua_State* L) {
  mat4 m = luax_checkvector(L, 1, V_MAT4, NULL);
  vec3 from = luax_checkvector(L, 2, V_VEC3, NULL);
  vec3 to = luax_checkvector(L, 3, V_VEC3, NULL);
  vec3 up = lua_isnoneornil(L, 4) ? (float[4]) { 0.f, 1.f, 0.f } : luax_checkvector(L, 4, V_VEC3, NULL);
  mat4_lookAt(m, from, to, up);
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
    mat4_multiply(mat4_init(out, m), n);
  } else if (type == V_VEC3) {
    vec3 out = luax_newtempvector(L, V_VEC3);
    vec3_init(out, n);
    mat4_transform(m, n);
  } else if (type == V_VEC4) {
    float* out = luax_newtempvector(L, V_VEC4);
    memcpy(out, n, 4 * sizeof(float));
    mat4_multiplyVec4(m, out);
  } else {
    lovrThrow("Unreachable");
  }
  return 1;
}

static int l_lovrMat4__tostring(lua_State* L) {
  luax_checkvector(L, 1, V_MAT4, NULL);
  lua_pushliteral(L, "mat4");
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
  return luaL_error(L, "attempt to assign property %s of mat4 (invalid property)", lua_tostring(L, -1));
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
  return luaL_error(L, "attempt to index field %s of mat4 (invalid property)", lua_tostring(L, -1));
}

const luaL_Reg lovrMat4[] = {
  { "unpack", l_lovrMat4Unpack },
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
  { "__mul", l_lovrMat4__mul },
  { "__tostring", l_lovrMat4__tostring },
  { "__newindex", l_lovrMat4__newindex },
  { "__index", l_lovrMat4__index },
  { NULL, NULL }
};
