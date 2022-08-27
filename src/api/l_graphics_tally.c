#include "api.h"
#include "graphics/graphics.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrTallyGetType(lua_State* L) {
  Tally* tally = luax_checktype(L, 1, Tally);
  const TallyInfo* info = lovrTallyGetInfo(tally);
  luax_pushenum(L, TallyType, info->type);
  return 1;
}

static int l_lovrTallyGetCount(lua_State* L) {
  Tally* tally = luax_checktype(L, 1, Tally);
  const TallyInfo* info = lovrTallyGetInfo(tally);
  lua_pushinteger(L, info->count);
  return 1;
}

static int l_lovrTallyGetViewCount(lua_State* L) {
  Tally* tally = luax_checktype(L, 1, Tally);
  const TallyInfo* info = lovrTallyGetInfo(tally);
  lua_pushinteger(L, info->views);
  return 1;
}

const luaL_Reg lovrTally[] = {
  { "getType", l_lovrTallyGetType },
  { "getCount", l_lovrTallyGetCount },
  { "getViewCount", l_lovrTallyGetViewCount },
  { NULL, NULL }
};
