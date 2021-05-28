#include "api.h"
#include "data/blob.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "data/sound.h"
#include "data/image.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

// Must be released when done
Image* luax_checkimage(lua_State* L, int index, bool flip) {
  Image* image = luax_totype(L, index, Image);

  if (image) {
    lovrRetain(image);
  } else {
    Blob* blob = luax_readblob(L, index, "Texture");
    image = lovrImageCreateFromBlob(blob, flip);
    lovrRelease(blob, lovrBlobDestroy);
  }

  return image;
}

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
  lovrRelease(blob, lovrBlobDestroy);
  return 1;
}

static int l_lovrDataNewModelData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Model");
  ModelData* modelData = lovrModelDataCreate(blob, luax_readfile);
  luax_pushtype(L, ModelData, modelData);
  lovrRelease(blob, lovrBlobDestroy);
  lovrRelease(modelData, lovrModelDataDestroy);
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
  lovrRelease(blob, lovrBlobDestroy);
  lovrRelease(rasterizer, lovrRasterizerDestroy);
  return 1;
}

static int l_lovrDataNewSound(lua_State* L) {
  int type = lua_type(L, 1);
  if (type == LUA_TNUMBER) {
    uint64_t frames = luaL_checkinteger(L, 1);
    SampleFormat format = luax_checkenum(L, 2, SampleFormat, "f32");
    ChannelLayout layout = luax_checkenum(L, 3, ChannelLayout, "stereo");
    uint32_t sampleRate = luaL_optinteger(L, 4, 48000);
    Blob* blob = luax_totype(L, 5, Blob);
    const char* other = lua_tostring(L, 5);
    bool stream = other && !strcmp(other, "stream");
    Sound* sound = stream ?
      lovrSoundCreateStream(frames, format, layout, sampleRate) :
      lovrSoundCreateRaw(frames, format, layout, sampleRate, blob);
    luax_pushtype(L, Sound, sound);
    lovrRelease(sound, lovrSoundDestroy);
    return 1;
  } else if (type != LUA_TSTRING && type != LUA_TUSERDATA) {
    return luax_typeerror(L, 1, "number, string, or Blob");
  }

  Blob* blob = luax_readblob(L, 1, "Sound");
  bool decode = lua_toboolean(L, 2);
  Sound* sound = lovrSoundCreateFromFile(blob, decode);
  luax_pushtype(L, Sound, sound);
  lovrRelease(blob, lovrBlobDestroy);
  lovrRelease(sound, lovrSoundDestroy);
  return 1;
}

static int l_lovrDataNewImage(lua_State* L) {
  Image* image = NULL;
  if (lua_type(L, 1) == LUA_TNUMBER) {
    int width = luaL_checkinteger(L, 1);
    int height = luaL_checkinteger(L, 2);
    TextureFormat format = luax_checkenum(L, 3, TextureFormat, "rgba");
    Blob* blob = lua_isnoneornil(L, 4) ? NULL : luax_checktype(L, 4, Blob);
    image = lovrImageCreate(width, height, blob, 0x0, format);
  } else {
    Image* source = luax_totype(L, 1, Image);
    if (source) {
      image = lovrImageCreate(source->width, source->height, source->blob, 0x0, source->format);
    } else {
      Blob* blob = luax_readblob(L, 1, "Texture");
      bool flip = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
      image = lovrImageCreateFromBlob(blob, flip);
      lovrRelease(blob, lovrBlobDestroy);
    }
  }

  luax_pushtype(L, Image, image);
  lovrRelease(image, lovrImageDestroy);
  return 1;
}

static const luaL_Reg lovrData[] = {
  { "newBlob", l_lovrDataNewBlob },
  { "newImage", l_lovrDataNewImage },
  { "newModelData", l_lovrDataNewModelData },
  { "newRasterizer", l_lovrDataNewRasterizer },
  { "newSound", l_lovrDataNewSound },
  { NULL, NULL }
};

extern const luaL_Reg lovrBlob[];
extern const luaL_Reg lovrImage[];
extern const luaL_Reg lovrModelData[];
extern const luaL_Reg lovrRasterizer[];
extern const luaL_Reg lovrSound[];

int luaopen_lovr_data(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrData);
  luax_registertype(L, Blob);
  luax_registertype(L, Image);
  luax_registertype(L, ModelData);
  luax_registertype(L, Rasterizer);
  luax_registertype(L, Sound);
  return 1;
}
