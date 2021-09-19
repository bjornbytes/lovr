#include "api.h"
#include "graphics/graphics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrBatchGetCapacity(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  const BatchInfo* info = lovrBatchGetInfo(batch);
  lua_pushinteger(L, info->capacity);
  return 1;
}

static int l_lovrBatchGetCount(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t count = lovrBatchGetCount(batch);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrBatchReset(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchReset(batch);
  return 0;
}

static int l_lovrBatchSort(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  SortMode mode = luax_checkenum(L, 2, SortMode, "opaque");
  lovrBatchSort(batch, mode);
  return 0;
}

static bool luax_filterpredicate(void* context, uint32_t i) {
  lua_State* L = context;
  lua_pushvalue(L, -1);
  lua_pushinteger(L, i);
  lua_call(L, 1, 1);
  bool result = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return result;
}

static int l_lovrBatchFilter(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_settop(L, 2);
  lovrBatchFilter(batch, luax_filterpredicate, L);
  return 0;
}

const luaL_Reg lovrBatch[] = {
  { "getCapacity", l_lovrBatchGetCapacity },
  { "getCount", l_lovrBatchGetCount },
  { "reset", l_lovrBatchReset },
  { "sort", l_lovrBatchSort },
  { "filter", l_lovrBatchFilter },
  { NULL, NULL }
};
