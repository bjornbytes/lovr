#include "api.h"
#include "filesystem/blob.h"

int l_lovrBlobGetFilename(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushstring(L, blob->name);
  return 1;
}

int l_lovrBlobGetPointer(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushlightuserdata(L, blob->data);
  return 1;
}

int l_lovrBlobGetSize(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushinteger(L, blob->size);
  return 1;
}

int l_lovrBlobGetString(lua_State* L) {
  Blob* blob = luax_checktype(L, 1, Blob);
  lua_pushlstring(L, blob->data, blob->size);
  return 1;
}

const luaL_Reg lovrBlob[] = {
  { "getFilename", l_lovrBlobGetFilename },
  { "getPointer", l_lovrBlobGetPointer },
  { "getSize", l_lovrBlobGetSize },
  { "getString", l_lovrBlobGetString },
  { NULL, NULL }
};
