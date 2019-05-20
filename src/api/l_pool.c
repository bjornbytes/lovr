#include "api.h"
#include "math/pool.h"
#include "core/maf.h"

int l_lovrVec3Set(lua_State* L);
int l_lovrQuatSet(lua_State* L);
int l_lovrMat4Set(lua_State* L);

static int l_lovrPoolVec3(lua_State* L) {
  Pool* pool = luax_checktype(L, 1, Pool);
  vec3 v = lovrPoolAllocate(pool, MATH_VEC3);
  if (v) {
    luax_pushlightmathtype(L, v, MATH_VEC3);
    lua_replace(L, 1);
    return l_lovrVec3Set(L);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrPoolQuat(lua_State* L) {
  Pool* pool = luax_checktype(L, 1, Pool);
  quat q = lovrPoolAllocate(pool, MATH_QUAT);
  if (q) {
    luax_pushlightmathtype(L, q, MATH_QUAT);
    lua_replace(L, 1);
    return l_lovrQuatSet(L);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrPoolMat4(lua_State* L) {
  Pool* pool = luax_checktype(L, 1, Pool);
  mat4 m = lovrPoolAllocate(pool, MATH_MAT4);
  if (m) {
    luax_pushlightmathtype(L, m, MATH_MAT4);
    lua_replace(L, 1);
    return l_lovrMat4Set(L);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrPoolDrain(lua_State* L) {
  Pool* pool = luax_checktype(L, 1, Pool);
  lovrPoolDrain(pool);
  return 0;
}

static int l_lovrPoolGetSize(lua_State* L) {
  Pool* pool = luax_checktype(L, 1, Pool);
  lua_pushinteger(L, lovrPoolGetSize(pool));
  return 1;
}

static int l_lovrPoolGetUsage(lua_State* L) {
  Pool* pool = luax_checktype(L, 1, Pool);
  lua_pushinteger(L, lovrPoolGetUsage(pool));
  return 1;
}

const luaL_Reg lovrPool[] = {
  { "vec3", l_lovrPoolVec3 },
  { "quat", l_lovrPoolQuat },
  { "mat4", l_lovrPoolMat4 },
  { "drain", l_lovrPoolDrain },
  { "getSize", l_lovrPoolGetSize },
  { "getUsage", l_lovrPoolGetUsage },
  { NULL, NULL }
};
