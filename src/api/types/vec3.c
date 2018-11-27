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
      v[0] = luaL_optnumber(L, index++, 0.);
      v[1] = luaL_optnumber(L, index++, 0.);
      v[2] = luaL_optnumber(L, index++, 0.);
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
        v[0] = v[1] = v[2] = luaL_optnumber(L, index++, 0.);
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
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_set(v, luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4));
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    vec3_init(v, u);
  }
  lua_settop(L, 1);
  return 1;
}

static int l_lovrVec3Copy(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  if (out) {
    vec3_init(out, v);
    luax_pushlightmathtype(L, out, MATH_VEC3);
  } else {
    lua_pushnil(L);
  }
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

static int l_lovrVec3Normalize(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3_normalize(v);
  lua_settop(L, 1);
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
  out[0] = v[0] + u[0], out[1] = v[1] + u[1], out[2] = v[2] + u[2];
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__sub(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  out[0] = v[0] - u[0], out[1] = v[1] - u[1], out[2] = v[2] - u[2];
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__mul(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_scale(vec3_init(out, v), lua_tonumber(L, 2));
  } else {
    vec3 u = luax_checkmathtype(L, 2, MATH_VEC3, "vec3 or number");
    out[0] = v[0] * u[0], out[1] = v[1] * u[1], out[2] = v[2] * u[2];
  }
  luax_pushlightmathtype(L, out, MATH_VEC3);
  return 1;
}

static int l_lovrVec3__div(lua_State* L) {
  vec3 v = luax_checkmathtype(L, 1, MATH_VEC3, NULL);
  vec3 out = lovrPoolAllocate(lovrMathGetPool(), MATH_VEC3);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    vec3_scale(vec3_init(out, v), 1 / lua_tonumber(L, 2));
  } else {
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
  { "normalize", l_lovrVec3Normalize },
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
