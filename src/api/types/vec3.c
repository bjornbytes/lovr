#include "api.h"
#include "api/math.h"
#include "math/math.h"
#include "lib/math.h"

int luax_readvec3(lua_State* L, int index, vec3 v, const char* expected) {
  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      v[0] = v[1] = v[2] = 0.f;
      return ++index;
    case LUA_TNUMBER:
      v[0] = luaL_optnumber(L, index++, 0.f);
      v[1] = luaL_optnumber(L, index++, 0.f);
      v[2] = luaL_optnumber(L, index++, 0.f);
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
        v[0] = v[1] = v[2] = luaL_optnumber(L, index++, 0.f);
      } else {
        v[0] = 1.f;
        for (int i = 0; i < components; i++) {
          v[i] = luaL_optnumber(L, index++, v[0]);
        }
      }
      return index;
    default:
      vec3_init(v, luax_checkmathtype(L, index++, MATH_VEC3, expected ? expected : "vec3 or number"));
      return index;
  }
}

int luax_pushvec3(lua_State* L, vec3 v, int index) {
  vec3 out;
  if (index > 0 && !lua_isnoneornil(L, index) && (out = luax_checkmathtype(L, index, MATH_VEC3, NULL)) != NULL) {
    vec3_init(out, v);
    lua_settop(L, index);
    return 1;
  } else {
    lua_pushnumber(L, v[0]);
    lua_pushnumber(L, v[1]);
    lua_pushnumber(L, v[2]);
    return 3;
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
    float x = luaL_optnumber(L, 2, 0.f);
    vec3_set(v, x, luaL_optnumber(L, 3, x), luaL_optnumber(L, 4, x));
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    vec3_init(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Copy(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  Pool* pool = lua_isnoneornil(L, 2) ? lovrMathGetPool() : luax_checktype(L, 2, Pool);
  vec3 out = lovrPoolAllocate(pool, MATH_VEC3);
  vec3_init(out, v);
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3Save(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 copy = lua_newuserdata(L, 4 * sizeof(float));
  vec3_init(copy, v);
  luaL_getmetatable(L, "vec3");
  lua_setmetatable(L, -2);
  return 1;
}

static int l_lovrVec3Add(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3_add(v, u);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Sub(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3_sub(v, u);
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
  float t = luaL_checknumber(L, 3);
  vec3_lerp(v, u, t);
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3__add(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  vec3_add(vec3_init(out, v), u);
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__sub(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  vec3_sub(vec3_init(out, v), u);
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__mul(lua_State* L) {
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
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
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__div(lua_State* L) {
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
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
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__unm(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  vec3_scale(vec3_init(out, v), -1.f);
  luax_pushlightmathtype(L, out, MATH_VEC3);
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
  { "copy", l_lovrVec3Copy },
  { "save", l_lovrVec3Save },
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
  { NULL, NULL }
};
