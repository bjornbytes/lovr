#include "api.h"
#include "data/blob.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "data/sound.h"
#include "data/image.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

StringEntry lovrAnimationProperty[] = {
  [PROP_TRANSLATION] = ENTRY("translation"),
  [PROP_ROTATION] = ENTRY("rotation"),
  [PROP_SCALE] = ENTRY("scale"),
  [PROP_WEIGHTS] = ENTRY("weights"),
  { 0 }
};

StringEntry lovrModelDrawMode[] = {
  [DRAW_POINT_LIST] = ENTRY("points"),
  [DRAW_LINE_LIST] = ENTRY("lines"),
  [DRAW_LINE_STRIP] = ENTRY("linestrip"),
  [DRAW_LINE_LOOP] = ENTRY("lineloop"),
  [DRAW_TRIANGLE_LIST] = ENTRY("triangles"),
  [DRAW_TRIANGLE_STRIP] = ENTRY("strip"),
  [DRAW_TRIANGLE_FAN] = ENTRY("fan"),
  { 0 }
};

StringEntry lovrSmoothMode[] = {
  [SMOOTH_STEP] = ENTRY("step"),
  [SMOOTH_LINEAR] = ENTRY("linear"),
  [SMOOTH_CUBIC] = ENTRY("cubic"),
  { 0 }
};

// Must be released when done
Image* luax_checkimage(lua_State* L, int index) {
  Image* image = luax_totype(L, index, Image);

  if (image) {
    lovrRetain(image);
  } else {
    Blob* blob = luax_readblob(L, index, "Image");
    image = lovrImageCreateFromFile(blob);
    lovrRelease(blob, lovrBlobDestroy);
    luax_assert(L, image);
  }

  return image;
}

static int l_lovrDataNewAudioStream(lua_State* L) {
  uint32_t frames = luax_checku32(L, 1);
  SampleFormat format = luax_checkenum(L, 2, SampleFormat, "f32");
  uint32_t channels = luax_checku32(L, 3);
  uint32_t sampleRate = luax_optu32(L, 4, 48000);
  AudioStream* stream = lovrAudioStreamCreate(frames, format, channels, sampleRate);
  luax_assert(L, stream);
  luax_pushtype(L, AudioStream, stream);
  lovrRelease(stream, lovrAudioStreamDestroy);
  return 1;
}

static int l_lovrDataNewBlob(lua_State* L) {
  size_t size;
  uint8_t* data = NULL;
  int type = lua_type(L, 1);
  if (type == LUA_TNUMBER) {
    int isize = lua_tonumber(L, 1);
    luax_check(L, isize > 0, "Blob size must be positive");
    size = (size_t) isize;
    data = lovrCalloc(size);
  } else if (type == LUA_TSTRING) {
    const char* str = luaL_checklstring(L, 1, &size);
    data = lovrMalloc(size + 1);
    memcpy(data, str, size);
    data[size] = '\0';
  } else {
    Blob* blob = luax_checktype(L, 1, Blob);
    size = blob->size;
    data = lovrMalloc(size);
    memcpy(data, blob->data, size);
  }
  const char* name = luaL_optstring(L, 2, "");
  Blob* blob = lovrBlobCreate(data, size, name);
  luax_pushtype(L, Blob, blob);
  lovrRelease(blob, lovrBlobDestroy);
  return 1;
}

static int l_lovrDataNewBlobView(lua_State* L) {
  Blob* parent = luax_checktype(L, 1, Blob);
  int ioffset = luaL_checknumber(L, 2);
  luax_check(L, ioffset >= 0, "BlobView offset must be non-negative");
  luax_check(L, ioffset < parent->size, "BlobView offset must be less than parent size");
  size_t offset = (size_t) ioffset;
  size_t size = 0;
  if (lua_isnoneornil(L, 3)) {
    size = parent->size - offset;
  } else {
    int isize = luaL_checknumber(L, 3);
    luax_check(L, isize > 0, "BlobView size must be positive");
    size = (size_t) isize;
    luax_check(L, size <= parent->size - offset, "BlobView offset + size can't be greater then parent's size");
  }
  const char* name = luaL_optstring(L, 4, "");
  Blob* blob = lovrBlobCreateView(parent, offset, size, name);
  luax_pushtype(L, Blob, blob);
  lovrRelease(blob, lovrBlobDestroy);
  return 1;
}

static bool luax_loadimage(void** context) {
  Blob* blob = *context;
  Image* image = lovrImageCreateFromFile(blob);
  lovrRelease(blob, lovrBlobDestroy);
  *context = image;
  return !!image;
}

static int luax_pushimage(lua_State* L, bool success, void* context) {
  if (!success) return 0;
  luax_pushtype(L, Image, context);
  lovrRelease(context, lovrImageDestroy);
  return 1;
}

static int l_lovrDataNewImage(lua_State* L) {
  Image* image = NULL;
  if (lua_type(L, 1) == LUA_TNUMBER) {
    uint32_t width = luax_checku32(L, 1);
    uint32_t height = luax_checku32(L, 2);
    TextureFormat format = luax_checkenum(L, 3, TextureFormat, "rgba8");
    image = lovrImageCreateRaw(width, height, format, true);
    luax_assert(L, image);
    size_t size = lovrImageGetLayerSize(image, 0);
    void* data = lovrImageGetLayerData(image, 0, 0);
    if (lua_gettop(L) >= 4) {
      Blob* blob = luax_checktype(L, 4, Blob);
      luax_check(L, blob->size == size, "Blob size (%d) does not match the Image size (%d)", blob->size, size);
      memcpy(data, blob->data, size);
    } else {
      memset(data, 0, size);
    }
  } else {
    Image* source = luax_totype(L, 1, Image);
    if (source) {
      uint32_t width = lovrImageGetWidth(source, 0);
      uint32_t height = lovrImageGetHeight(source, 0);
      TextureFormat format = lovrImageGetFormat(source);
      image = lovrImageCreateRaw(width, height, format, true);
      luax_assert(L, image);
      memcpy(lovrImageGetLayerData(image, 0, 0), lovrImageGetLayerData(source, 0, 0), lovrImageGetLayerSize(image, 0));
    } else {
      Blob* blob = luax_readblob(L, 1, "Texture");
      return luax_yieldjob(L, luax_loadimage, luax_pushimage, blob, 1);
    }
  }

  luax_pushtype(L, Image, image);
  lovrRelease(image, lovrImageDestroy);
  return 1;
}

static bool luax_loadmodel(void** context) {
  Blob* blob = *context;
  ModelData* modelData = lovrModelDataCreate(blob, luax_readfile);
  lovrRelease(blob, lovrBlobDestroy);
  *context = modelData;
  return !!modelData;
}

static int luax_pushmodel(lua_State* L, bool success, void* context) {
  if (!success) return 0;
  luax_pushtype(L, ModelData, context);
  lovrRelease(context, lovrModelDataDestroy);
  return 1;
}

static int l_lovrDataNewModelData(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Model");
  return luax_yieldjob(L, luax_loadmodel, luax_pushmodel, blob, 1);
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

  Rasterizer* rasterizer = lovrRasterizerCreate(blob, size, luax_readfile);
  lovrRelease(blob, lovrBlobDestroy);
  luax_assert(L, rasterizer);
  luax_pushtype(L, Rasterizer, rasterizer);
  lovrRelease(rasterizer, lovrRasterizerDestroy);
  return 1;
}

static bool luax_loadsound(void** context) {
  Blob* blob = *context;
  Sound* sound = lovrSoundLoad(blob, true);
  lovrRelease(blob, lovrBlobDestroy);
  *context = sound;
  return !!sound;
}

static int luax_pushsound(lua_State* L, bool success, void* context) {
  if (!success) return 0;
  luax_pushtype(L, Sound, context);
  lovrRelease(context, lovrSoundDestroy);
  return 1;
}

static int l_lovrDataNewSound(lua_State* L) {
  int type = lua_type(L, 1);

  if (type == LUA_TNUMBER) {
    uint32_t frames = luax_checku32(L, 1);
    SampleFormat format = luax_checkenum(L, 2, SampleFormat, "f32");
    uint32_t channels = lua_type(L, 3) == LUA_TNUMBER ? luax_checku32(L, 3) : (1 << luax_checkenum(L, 3, ChannelLayout, NULL));
    uint32_t sampleRate = luax_optu32(L, 4, 48000);
    Blob* blob = luax_totype(L, 5, Blob);
    Sound* sound = lovrSoundCreate(frames, format, channels, sampleRate);
    luax_assert(L, sound);

    if (blob) {
      Blob* dst = lovrSoundGetBlob(sound);
      memcpy(dst->data, blob->data, MIN(blob->size, dst->size));
    }

    luax_pushtype(L, Sound, sound);
    lovrRelease(sound, lovrSoundDestroy);
    return 1;
  } else if (type != LUA_TSTRING && type != LUA_TUSERDATA) {
    return luax_typeerror(L, 1, "number, string, or Blob");
  }

  Blob* blob = luax_readblob(L, 1, "Sound");
  bool decode = lua_toboolean(L, 2);

  if (decode) {
    return luax_yieldjob(L, luax_loadsound, luax_pushsound, blob, 1);
  } else {
    Sound* sound = lovrSoundLoad(blob, decode);
    lovrRelease(blob, lovrBlobDestroy);
    luax_assert(L, sound);
    luax_pushtype(L, Sound, sound);
    lovrRelease(sound, lovrSoundDestroy);
    return 1;
  }
}

static const luaL_Reg lovrData[] = {
  { "newAudioStream", l_lovrDataNewAudioStream },
  { "newBlob", l_lovrDataNewBlob },
  { "newBlobView", l_lovrDataNewBlobView },
  { "newImage", l_lovrDataNewImage },
  { "newModelData", l_lovrDataNewModelData },
  { "newRasterizer", l_lovrDataNewRasterizer },
  { "newSound", l_lovrDataNewSound },
  { NULL, NULL }
};

extern const luaL_Reg lovrAudioStream[];
extern const luaL_Reg lovrBlob[];
extern const luaL_Reg lovrImage[];
extern const luaL_Reg lovrModelData[];
extern const luaL_Reg lovrRasterizer[];
extern const luaL_Reg lovrSound[];

int luaopen_lovr_data(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrData);
  luax_registertype(L, AudioStream);
  luax_registertype(L, Blob);
  luax_registertype(L, Image);
  luax_registertype(L, ModelData);
  luax_registertype(L, Rasterizer);
  luax_registertype(L, Sound);
  float16Init();
  return 1;
}
