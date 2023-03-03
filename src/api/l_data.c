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
  { 0 }
};

StringEntry lovrAttributeType[] = {
  [I8] = ENTRY("i8"),
  [U8] = ENTRY("u8"),
  [I16] = ENTRY("i16"),
  [U16] = ENTRY("u16"),
  [I32] = ENTRY("i32"),
  [U32] = ENTRY("u32"),
  [F32] = ENTRY("f32"),
  { 0 }
};

StringEntry lovrDefaultAttribute[] = {
  [ATTR_POSITION] = ENTRY("position"),
  [ATTR_NORMAL] = ENTRY("normal"),
  [ATTR_UV] = ENTRY("uv"),
  [ATTR_COLOR] = ENTRY("color"),
  [ATTR_TANGENT] = ENTRY("tangent"),
  [ATTR_JOINTS] = ENTRY("joints"),
  [ATTR_WEIGHTS] = ENTRY("weights"),
  { 0 }
};

StringEntry lovrDrawMode[] = {
  [DRAW_POINTS] = ENTRY("points"),
  [DRAW_LINES] = ENTRY("lines"),
  [DRAW_LINE_STRIP] = ENTRY("linestrip"),
  [DRAW_LINE_LOOP] = ENTRY("lineloop"),
  [DRAW_TRIANGLE_STRIP] = ENTRY("strip"),
  [DRAW_TRIANGLES] = ENTRY("triangles"),
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

static int l_lovrDataNewImage(lua_State* L) {
  Image* image = NULL;
  if (lua_type(L, 1) == LUA_TNUMBER) {
    uint32_t width = luax_checku32(L, 1);
    uint32_t height = luax_checku32(L, 2);
    TextureFormat format = luax_checkenum(L, 3, TextureFormat, "rgba8");
    image = lovrImageCreateRaw(width, height, format);
    size_t size = lovrImageGetLayerSize(image, 0);
    void* data = lovrImageGetLayerData(image, 0, 0);
    if (lua_gettop(L) >= 4) {
      Blob* blob = luax_checktype(L, 4, Blob);
      lovrCheck(blob->size == size, "Blob size (%d) does not match the Image size (%d)", blob->size, size);
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
      image = lovrImageCreateRaw(width, height, format);
      memcpy(lovrImageGetLayerData(image, 0, 0), lovrImageGetLayerData(source, 0, 0), lovrImageGetLayerSize(image, 0));
    } else {
      Blob* blob = luax_readblob(L, 1, "Texture");
      image = lovrImageCreateFromFile(blob);
      lovrRelease(blob, lovrBlobDestroy);
    }
  }

  luax_pushtype(L, Image, image);
  lovrRelease(image, lovrImageDestroy);
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
    uint32_t frames = luax_checku32(L, 1);
    SampleFormat format = luax_checkenum(L, 2, SampleFormat, "f32");
    ChannelLayout layout = luax_checkenum(L, 3, ChannelLayout, "stereo");
    uint32_t sampleRate = luax_optu32(L, 4, 48000);
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
