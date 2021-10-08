#include "api.h"
#include "data/modelData.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrModelDataGetBlobCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->blobCount);
  return 1;
}

static int l_lovrModelDataGetBlob(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luaL_checkinteger(L, 2) - 1;
  lovrCheck(index < model->blobCount, "Invalid Blob index #%d", index + 1);
  luax_pushtype(L, Blob, model->blobs[index]);
  return 1;
}

static int l_lovrModelDataGetImageCount(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  lua_pushinteger(L, model->imageCount);
  return 1;
}

static int l_lovrModelDataGetImage(lua_State* L) {
  ModelData* model = luax_checktype(L, 1, ModelData);
  uint32_t index = luaL_checkinteger(L, 2) - 1;
  lovrCheck(index < model->imageCount, "Invalid Image index #%d", index + 1);
  luax_pushtype(L, Image, model->images[index]);
  return 1;
}

const luaL_Reg lovrModelData[] = {
  { "getBlobCount", l_lovrModelDataGetBlobCount },
  { "getBlob", l_lovrModelDataGetBlob },
  { "getImageCount", l_lovrModelDataGetImageCount },
  { "getImage", l_lovrModelDataGetImage },
  { NULL, NULL }
};
