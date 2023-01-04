#include "api.h"
#include "data/blob.h"
#include "util.h"

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

  uint32_t offset = luax_optu32(L, 2, 0);
  lovrCheck(offset < blob->size, "Blob byte offset must be less than the size of the Blob");

  lua_Integer length = luaL_optinteger(L, 3, blob->size - offset);
  lovrCheck(length >= 0, "Length can not be negative");
  lovrCheck((size_t) length <= blob->size - offset, "Blob:getString range overflows the length of the Blob");

  lua_pushlstring(L, (char*) blob->data + offset, (size_t) length);
  return 1;
}

const luaL_Reg lovrBlob[] = {
  { "getName", l_lovrBlobGetName },
  { "getPointer", l_lovrBlobGetPointer },
  { "getSize", l_lovrBlobGetSize },
  { "getString", l_lovrBlobGetString },
  { NULL, NULL }
};
