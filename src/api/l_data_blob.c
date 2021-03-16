#include "api.h"
#include "data/blob.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrBlobGetName(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushstring(L, blob->name);
  return 1;
}

static int l_lovrBlobGetPointer(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushlightuserdata(L, blob->data);
  return 1;
}

static int l_lovrBlobGetSize(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushinteger(L, blob->size);
  return 1;
}

static int l_lovrBlobGetString(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushlstring(L, blob->data, blob->size);
  return 1;
}

const luaL_Reg lovrBlob[] = {
  { "getName", l_lovrBlobGetName },
  { "getPointer", l_lovrBlobGetPointer },
  { "getSize", l_lovrBlobGetSize },
  { "getString", l_lovrBlobGetString },
  { NULL, NULL }
};
