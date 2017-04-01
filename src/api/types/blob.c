#include "api/lovr.h"
#include "filesystem/blob.h"

int l_lovrBlobGetFilename(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushstring(L, lovrBlobGetName(blob));
  return 1;
}

int l_lovrBlobGetPointer(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushlightuserdata(L, lovrBlobGetData(blob));
  return 1;
}

int l_lovrBlobGetSize(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushinteger(L, lovrBlobGetSize(blob));
  return 1;
}

int l_lovrBlobGetString(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushlstring(L, lovrBlobGetData(blob), lovrBlobGetSize(blob));
  return 1;
}

const luaL_Reg lovrBlob[] = {
  { "getFilename", l_lovrBlobGetFilename },
  { "getPointer", l_lovrBlobGetPointer },
  { "getSize", l_lovrBlobGetSize },
  { "getString", l_lovrBlobGetString },
  { NULL, NULL }
};
