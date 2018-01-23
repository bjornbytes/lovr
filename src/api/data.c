#include "api.h"
#include "data/data.h"
#include "data/audioStream.h"
#include "data/model.h"
#include "data/rasterizer.h"
#include "data/texture.h"

int l_lovrDataInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrData);
  luax_registertype(L, "AudioStream", lovrAudioStream);
  luax_registertype(L, "ModelData", lovrModelData);
  luax_registertype(L, "Rasterizer", lovrRasterizer);
  luax_registertype(L, "TextureData", lovrTextureData);
  return 1;
}

int l_lovrDataNewAudioStream(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Sound");
  size_t bufferSize = luaL_optinteger(L, 2, 4096);
  AudioStream* stream = lovrAudioStreamCreate(blob, bufferSize);
  luax_pushtype(L, AudioStream, stream);
  lovrRelease(&blob->ref);
  lovrRelease(&stream->ref);
  return 1;
}

int l_lovrDataNewModelData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Model");
  ModelData* modelData = lovrModelDataCreate(blob);
  luax_pushtype(L, ModelData, modelData);
  lovrRelease(&blob->ref);
  lovrRelease(&modelData->ref);
  return 1;
}

int l_lovrDataNewTextureData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Texture");
  TextureData* textureData = lovrTextureDataFromBlob(blob);
  luax_pushtype(L, TextureData, textureData);
  lovrRelease(&blob->ref);
  lovrRelease(&textureData->ref);
  return 1;
}

int l_lovrDataNewRasterizer(lua_State* L) {
  Blob* blob = NULL;
  float size;

  if (lua_type(L, 1) == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
    size = luaL_optnumber(L, 1, 32);
  } else {
    blob = luax_readblob(L, 1, "Font");
    size = luaL_optnumber(L, 2, 32);
  }

  Rasterizer* rasterizer = lovrRasterizerCreate(blob, size);
  luax_pushtype(L, Rasterizer, rasterizer);

  if (blob) {
    lovrRelease(&blob->ref);
  }

  lovrRelease(&rasterizer->ref);
  return 1;
}

const luaL_Reg lovrData[] = {
  { "newAudioStream", l_lovrDataNewAudioStream },
  { "newModelData", l_lovrDataNewModelData },
  { "newRasterizer", l_lovrDataNewRasterizer },
  { "newTextureData", l_lovrDataNewTextureData },
  { NULL, NULL }
};
