#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "util.h"

static int l_lovrReadbackIsComplete(lua_State* L) {
  Readback* readback = luax_checktype(L, 1, Readback);
  bool complete = lovrReadbackIsComplete(readback);
  lua_pushboolean(L, complete);
  return 1;
}

static int l_lovrReadbackWait(lua_State* L) {
  Readback* readback = luax_checktype(L, 1, Readback);
  bool waited;
  luax_assert(L, lovrReadbackWait(readback, &waited));
  lua_pushboolean(L, waited);
  return 1;
}

static int l_lovrReadbackGetData(lua_State* L) {
  Readback* readback = luax_checktype(L, 1, Readback);
  DataField* format;
  uint32_t count;
  void* data = lovrReadbackGetData(readback, &format, &count);
  if (data && format) {
    return luax_pushbufferdata(L, format, count, data);
  } else {
    lua_pushnil(L);
    return 1;
  }
}

static int l_lovrReadbackGetBlob(lua_State* L) {
  Readback* readback = luax_checktype(L, 1, Readback);
  Blob* blob = lovrReadbackGetBlob(readback);
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrReadbackGetImage(lua_State* L) {
  Readback* readback = luax_checktype(L, 1, Readback);
  Image* image = lovrReadbackGetImage(readback);
  luax_pushtype(L, Image, image);
  return 1;
}

const luaL_Reg lovrReadback[] = {
  { "isComplete", l_lovrReadbackIsComplete },
  { "wait", l_lovrReadbackWait },
  { "getData", l_lovrReadbackGetData },
  { "getBlob", l_lovrReadbackGetBlob },
  { "getImage", l_lovrReadbackGetImage },
  { NULL, NULL }
};
