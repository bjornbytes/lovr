#include "api.h"
#include "data/data.h"
#include "data/texture.h"
#include "data/audioStream.h"

int l_lovrDataInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrData);
  luax_registertype(L, "TextureData", lovrTextureData);
  luax_registertype(L, "AudioStream", lovrAudioStream);
  return 1;
}

int l_lovrDataNewTextureData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Texture");
  TextureData* textureData = lovrTextureDataFromBlob(blob);
  lovrRelease(&blob->ref);
  lovrRelease(&textureData->ref);
  luax_pushtype(L, TextureData, textureData);
  return 1;
}

int l_lovrDataNewAudioStream(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Sound");
  size_t bufferSize = luaL_optinteger(L, 2, 4096);
  AudioStream* stream = lovrAudioStreamCreate(blob, bufferSize);
  lovrRelease(&blob->ref);
  lovrRelease(&stream->ref);
  luax_pushtype(L, AudioStream, stream);
  return 1;
}

const luaL_Reg lovrData[] = {
  { "newAudioStream", l_lovrDataNewAudioStream },
  { "newTextureData", l_lovrDataNewTextureData },
  { NULL, NULL }
};
