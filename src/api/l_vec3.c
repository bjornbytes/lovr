#include "api.h"
#include "math/math.h"
#include "core/maf.h"

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
      vec3_init(v, luax_checkmathtype(L, index++, MATH_VEC3, expected ? expected : "vec3 or number"));
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
      vec3_init(v, luax_checkmathtype(L, index++, MATH_VEC3, expected ? expected : "vec3 or number"));
      return index;
  }
}

static int l_lovrVec3Unpack(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  lua_pushnumber(L, v[0]);
  lua_pushnumber(L, v[1]);
  lua_pushnumber(L, v[2]);
  return 3;
}

int l_lovrVec3Set(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  if (lua_isnoneornil(L, 2) || lua_type(L, 2) == LUA_TNUMBER) {
    float x = luax_optfloat(L, 2, 0.f);
    vec3_set(v, x, luax_optfloat(L, 3, x), luax_optfloat(L, 4, x));
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    vec3_init(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Add(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    vec3_add(v, (float[3]) { x, luaL_optnumber(L, 3, x), luaL_optnumber(L, 4, x) });
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    vec3_add(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Sub(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float x = lua_tonumber(L, 2);
    vec3_sub(v, (float[3]) { x, luaL_optnumber(L, 3, x), luaL_optnumber(L, 4, x) });
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    vec3_sub(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Mul(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_scale(v, lua_tonumber(L, 2));
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    v[0] = v[0] * u[0], v[1] = v[1] * u[1], v[2] = v[2] * u[2];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Div(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_scale(v, 1.f / (float) lua_tonumber(L, 2));
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    v[0] = v[0] / u[0], v[1] = v[1] / u[1], v[2] = v[2] / u[2];
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Length(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  lua_pushnumber(L, vec3_length(v));
  return 1;
}

static int l_lovrVec3Normalize(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3_normalize(v);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Distance(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  lua_pushnumber(L, vec3_distance(v, u));
  return 1;
}

static int l_lovrVec3Dot(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  lua_pushnumber(L, vec3_dot(v, u));
  return 1;
}

static int l_lovrVec3Cross(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3_cross(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Lerp(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  float t = luax_checkfloat(L, 3);
  vec3_lerp(v, u, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Min(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3_min(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Max(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3_max(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3__add(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3 out = luax_newmathtype(L, MATH_VEC3);
  vec3_add(vec3_init(out, v), u);
  return 1;
}

static int l_lovrVec3__sub(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3 out = luax_newmathtype(L, MATH_VEC3);
  vec3_sub(vec3_init(out, v), u);
  return 1;
}

static int l_lovrVec3__mul(lua_State* L) {
  vec3 out = luax_newmathtype(L, MATH_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
    vec3_scale(vec3_init(out, u), lua_tonumber(L, 1));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
    vec3_scale(vec3_init(out, v), lua_tonumber(L, 2));
  } else {
    vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    out[0] = v[0] * u[0], out[1] = v[1] * u[1], out[2] = v[2] * u[2];
  }
  return 1;
}

static int l_lovrVec3__div(lua_State* L) {
  vec3 out = luax_newmathtype(L, MATH_VEC3);
  if (lua_type(L, 1) == LUA_TNUMBER) {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
    vec3_scale(vec3_init(out, u), 1. / lua_tonumber(L, 1));
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
    vec3_scale(vec3_init(out, v), 1. / lua_tonumber(L, 2));
  } else {
    vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    out[0] = v[0] / u[0], out[1] = v[1] / u[1], out[2] = v[2] / u[2];
  }
  return 1;
}

static int l_lovrVec3__unm(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 out = luax_newmathtype(L, MATH_VEC3);
  vec3_scale(vec3_init(out, v), -1.f);
  return 1;
}

static int l_lovrVec3__len(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  lua_pushnumber(L, vec3_length(v));
  return 1;
}

static int l_lovrVec3__tostring(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  lua_pushfstring(L, "(%f, %f, %f)", v[0], v[1], v[2]);
  return 1;
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
  { "min", l_lovrVec3Min },
  { "max", l_lovrVec3Max },
  { "__add", l_lovrVec3__add },
  { "__sub", l_lovrVec3__sub },
  { "__mul", l_lovrVec3__mul },
  { "__div", l_lovrVec3__div },
  { "__unm", l_lovrVec3__unm },
  { "__len", l_lovrVec3__len },
  { "__tostring", l_lovrVec3__tostring },
  { NULL, NULL }
};
