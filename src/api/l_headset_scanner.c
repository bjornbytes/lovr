#include "api.h"
#include "headset/headset.h"
#include "util.h"

static int l_lovrScannerGetType(lua_State* L) {
  Scanner* scanner = luax_checktype(L, 1, Scanner);
  luax_pushenum(L, ScannerType, lovrScannerGetType(scanner));
  return 1;
}

static int l_lovrScannerScan(lua_State* L) {
  Scanner* scanner = luax_checktype(L, 1, Scanner);
  return 0;
}

static int l_lovrScannerUpdate(lua_State* L) {
  Scanner* scanner = luax_checktype(L, 1, Scanner);
  luax_assert(L, lovrScannerUpdate(scanner));
  return 0;
}

static int l_lovrScannerGetAnchors(lua_State* L) {
  Scanner* scanner = luax_checktype(L, 1, Scanner);

  if (lua_istable(L, 2)) {
    lua_settop(L, 2);
  } else {
    lua_settop(L, 1);
    lua_newtable(L);
  }

  int i = 1;
  Anchor* anchor = NULL;
  while ((anchor = lovrScannerGetAnchors(scanner, anchor)) != NULL) {
    luax_pushtype(L, Anchor, anchor);
    lua_rawseti(L, 2, i++);
  }

  int length = luax_len(L, 2);
  while (i++ <= length) {
    lua_pushnil(L);
    lua_rawseti(L, 2, i);
  }

  return 1;
}

int luax_anchoriterator(lua_State* L) {
  Scanner* scanner = luax_checktype(L, 1, Scanner);
  Anchor* anchor = luax_totype(L, 2, Anchor);
  Anchor* next = lovrScannerGetAnchors(scanner, anchor);
  luax_pushtype(L, Anchor, next);
  return 1;
}

int l_lovrScannerAnchors(lua_State* L) {
  luax_checktype(L, 1, Scanner);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, 1);
  lua_pushnil(L);
  return 3;
}

const luaL_Reg lovrScanner[] = {
  { "getType", l_lovrScannerGetType },
  { "scan", l_lovrScannerScan },
  { "update", l_lovrScannerUpdate },
  { "getAnchors", l_lovrScannerGetAnchors },
  { NULL, NULL }
};
