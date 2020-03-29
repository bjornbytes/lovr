#include "api.h"
#include "data/audioStream.h"
#include "data/blob.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "data/soundData.h"
#include "data/textureData.h"
#include "core/ref.h"
#include <stdlib.h>
#include <string.h>

static int l_lovrDataNewBlob(lua_State* L) {
  size_t size;
  uint8_t* data = NULL;
  int type = lua_type(L, 1);
  if (type == LUA_TNUMBER) {
    int isize = lua_tonumber(L, 1);
    lovrAssert(isize > 0, "Blob size must be positive");
    size = (size_t) isize;
    data = calloc(1, size);
    lovrAssert(data, "Out of memory");
  } else if (type == LUA_TSTRING) {
    const char* str = luaL_checklstring(L, 1, &size);
    data = malloc(size + 1);
    lovrAssert(data, "Out of memory");
    memcpy(data, str, size);
    data[size] = '\0';
  } else {
    Blob* blob = luax_checktype(L, 1, Blob);
    size = blob->size;
    data = malloc(size);
    lovrAssert(data, "Out of memory");
    memcpy(data, blob->data, size);
  }
  const char* name = luaL_optstring(L, 2, "");
  Blob* blob = lovrBlobCreate(data, size, name);
  luax_pushtype(L, Blob, blob);
  lovrRelease(Blob, blob);
  return 1;
}

static int l_lovrDataNewAudioStream(lua_State* L) {
  AudioStream* stream = NULL;
  if (lua_type(L, 1) == LUA_TNUMBER && lua_type(L, 2) == LUA_TNUMBER) {
    int channelCount = lua_tonumber(L, 1);
    int sampleRate = lua_tonumber(L, 2);
    int bufferSize = luaL_optinteger(L, 3, 4096);
    int queueLimit = luaL_optinteger(L, 4, sampleRate*0.5);
    stream = lovrAudioStreamCreateRaw(channelCount, sampleRate, bufferSize, queueLimit);
  } else {
    Blob* blob = luax_readblob(L, 1, "AudioStream");
    int bufferSize = luaL_optinteger(L, 2, 4096);
    stream = lovrAudioStreamCreate(blob, bufferSize);
    lovrRelease(Blob, blob);
  }
  luax_pushtype(L, AudioStream, stream);
  lovrRelease(AudioStream, stream);
  return 1;
}

static int l_lovrDataNewModelData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Model");
  ModelData* modelData = lovrModelDataCreate(blob, luax_readfile);
  luax_pushtype(L, ModelData, modelData);
  lovrRelease(Blob, blob);
  lovrRelease(ModelData, modelData);
  return 1;
}

static int l_lovrDataNewRasterizer(lua_State* L) {
  Blob* blob = NULL;
  float size;

  if (lua_type(L, 1) == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
    size = luax_optfloat(L, 1, 32.f);
  } else {
    blob = luax_readblob(L, 1, "Font");
    size = luax_optfloat(L, 2, 32.f);
  }

  Rasterizer* rasterizer = lovrRasterizerCreate(blob, size);
  luax_pushtype(L, Rasterizer, rasterizer);
  lovrRelease(Blob, blob);
  lovrRelease(Rasterizer, rasterizer);
  return 1;
}

static int l_lovrDataNewSoundData(lua_State* L) {
  if (lua_type(L, 1) == LUA_TNUMBER) {
    int samples = luaL_checkinteger(L, 1);
    int sampleRate = luaL_optinteger(L, 2, 44100);
    int bitDepth = luaL_optinteger(L, 3, 16);
    int channelCount = luaL_optinteger(L, 4, 2);
    SoundData* soundData = lovrSoundDataCreate(samples, sampleRate, bitDepth, channelCount);
    luax_pushtype(L, SoundData, soundData);
    lovrRelease(SoundData, soundData);
    return 1;
  }

  AudioStream* audioStream = luax_totype(L, 1, AudioStream);
  if (audioStream) {
    SoundData* soundData = lovrSoundDataCreateFromAudioStream(audioStream);
    luax_pushtype(L, SoundData, soundData);
    lovrRelease(SoundData, soundData);
    return 1;
  }

  Blob* blob = luax_readblob(L, 1, "SoundData");
  SoundData* soundData = lovrSoundDataCreateFromBlob(blob);
  luax_pushtype(L, SoundData, soundData);
  lovrRelease(Blob, blob);
  lovrRelease(SoundData, soundData);
  return 1;
}

static int l_lovrDataNewTextureData(lua_State* L) {
  TextureData* textureData = NULL;
  if (lua_type(L, 1) == LUA_TNUMBER) {
    int width = luaL_checkinteger(L, 1);
    int height = luaL_checkinteger(L, 2);
    TextureFormat format = luax_checkenum(L, 3, TextureFormats, "rgba", "TextureFormat");
    Blob* blob = lua_isnoneornil(L, 4) ? NULL : luax_checktype(L, 4, Blob);
    textureData = lovrTextureDataCreate(width, height, blob, 0x0, format);
  } else {
    TextureData* source = luax_totype(L, 1, TextureData);
    if (source) {
      textureData = lovrTextureDataCreate(source->width, source->height, source->blob, 0x0, source->format);
    } else {
      Blob* blob = luax_readblob(L, 1, "Texture");
      bool flip = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
      textureData = lovrTextureDataCreateFromBlob(blob, flip);
      lovrRelease(Blob, blob);
    }
  }

  luax_pushtype(L, TextureData, textureData);
  lovrRelease(TextureData, textureData);
  return 1;
}

static const luaL_Reg lovrData[] = {
  { "newBlob", l_lovrDataNewBlob },
  { "newAudioStream", l_lovrDataNewAudioStream },
  { "newModelData", l_lovrDataNewModelData },
  { "newRasterizer", l_lovrDataNewRasterizer },
  { "newSoundData", l_lovrDataNewSoundData },
  { "newTextureData", l_lovrDataNewTextureData },
  { NULL, NULL }
};

int luaopen_lovr_data(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrData);
  luax_registertype(L, Blob);
  luax_registertype(L, AudioStream);
  luax_registertype(L, ModelData);
  luax_registertype(L, Rasterizer);
  luax_registertype(L, SoundData);
  luax_registertype(L, TextureData);
  return 1;
}
