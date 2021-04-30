#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

StringEntry lovrBlendAlphaMode[] = {
  [BLEND_ALPHA_MULTIPLY] = ENTRY("alphamultiply"),
  [BLEND_PREMULTIPLIED] = ENTRY("premultiplied"),
  { 0 }
};

StringEntry lovrBlendMode[] = {
  [BLEND_ALPHA] = ENTRY("alpha"),
  [BLEND_ADD] = ENTRY("add"),
  [BLEND_SUBTRACT] = ENTRY("subtract"),
  [BLEND_MULTIPLY] = ENTRY("multiply"),
  [BLEND_LIGHTEN] = ENTRY("lighten"),
  [BLEND_DARKEN] = ENTRY("darken"),
  [BLEND_SCREEN] = ENTRY("screen"),
  { 0 }
};

StringEntry lovrBufferFlag[] = {
  [0] = ENTRY("vertex"),
  [1] = ENTRY("index"),
  [2] = ENTRY("uniform"),
  [3] = ENTRY("compute"),
  [4] = ENTRY("parameter"),
  [5] = ENTRY("copyfrom"),
  [6] = ENTRY("copyto"),
  [7] = ENTRY("write"),
  [8] = ENTRY("retain"),
  { 0 }
};

StringEntry lovrCompareMode[] = {
  [COMPARE_NONE] = ENTRY("none"),
  [COMPARE_EQUAL] = ENTRY("equal"),
  [COMPARE_NEQUAL] = ENTRY("notequal"),
  [COMPARE_LESS] = ENTRY("less"),
  [COMPARE_LEQUAL] = ENTRY("lequal"),
  [COMPARE_GREATER] = ENTRY("greater"),
  [COMPARE_GEQUAL] = ENTRY("gequal"),
  { 0 }
};

StringEntry lovrCullMode[] = {
  [CULL_NONE] = ENTRY("none"),
  [CULL_FRONT] = ENTRY("front"),
  [CULL_BACK] = ENTRY("back"),
  { 0 }
};

StringEntry lovrFieldType[] = {
  [FIELD_I8] = ENTRY("i8"),
  [FIELD_U8] = ENTRY("u8"),
  [FIELD_I16] = ENTRY("i16"),
  [FIELD_U16] = ENTRY("u16"),
  [FIELD_I32] = ENTRY("i32"),
  [FIELD_U32] = ENTRY("u32"),
  [FIELD_F32] = ENTRY("f32"),
  [FIELD_F64] = ENTRY("f64"),
  [FIELD_I8x2] = ENTRY("i8x2"),
  [FIELD_U8x2] = ENTRY("u8x2"),
  [FIELD_I8Nx2] = ENTRY("i8nx2"),
  [FIELD_U8Nx2] = ENTRY("u8nx2"),
  [FIELD_I16x2] = ENTRY("i16x2"),
  [FIELD_U16x2] = ENTRY("u16x2"),
  [FIELD_I16Nx2] = ENTRY("i16nx2"),
  [FIELD_U16Nx2] = ENTRY("u16nx2"),
  [FIELD_I32x2] = ENTRY("i32x2"),
  [FIELD_U32x2] = ENTRY("u32x2"),
  [FIELD_F32x2] = ENTRY("f32x2"),
  [FIELD_I32x3] = ENTRY("i32x3"),
  [FIELD_U32x3] = ENTRY("u32x3"),
  [FIELD_F32x3] = ENTRY("f32x3"),
  [FIELD_I8x4] = ENTRY("i8x4"),
  [FIELD_U8x4] = ENTRY("u8x4"),
  [FIELD_I8Nx4] = ENTRY("i8nx4"),
  [FIELD_U8Nx4] = ENTRY("u8nx4"),
  [FIELD_I16x4] = ENTRY("i16x4"),
  [FIELD_U16x4] = ENTRY("u16x4"),
  [FIELD_I16Nx4] = ENTRY("i16nx4"),
  [FIELD_U16Nx4] = ENTRY("u16nx4"),
  [FIELD_I32x4] = ENTRY("i32x4"),
  [FIELD_U32x4] = ENTRY("u32x4"),
  [FIELD_F32x4] = ENTRY("f32x4"),
  [FIELD_MAT2] = ENTRY("mat2"),
  [FIELD_MAT3] = ENTRY("mat3"),
  [FIELD_MAT4] = ENTRY("mat4"),
  { 0 }
};

StringEntry lovrStencilAction[] = {
  [STENCIL_KEEP] = ENTRY("keep"),
  [STENCIL_REPLACE] = ENTRY("replace"),
  [STENCIL_INCREMENT] = ENTRY("increment"),
  [STENCIL_DECREMENT] = ENTRY("decrement"),
  [STENCIL_INCREMENT_WRAP] = ENTRY("incrementwrap"),
  [STENCIL_DECREMENT_WRAP] = ENTRY("decrementwrap"),
  [STENCIL_INVERT] = ENTRY("invert"),
  { 0 }
};

StringEntry lovrTextureType[] = {
  [TEXTURE_2D] = ENTRY("2d"),
  [TEXTURE_CUBE] = ENTRY("cube"),
  [TEXTURE_VOLUME] = ENTRY("volume"),
  [TEXTURE_ARRAY] = ENTRY("array"),
  { 0 }
};

StringEntry lovrTextureUsage[] = {
  [TEXTURE_SAMPLE] = ENTRY("sample"),
  [TEXTURE_RENDER] = ENTRY("render"),
  [TEXTURE_COMPUTE] = ENTRY("compute"),
  [TEXTURE_UPLOAD] = ENTRY("upload"),
  [TEXTURE_DOWNLOAD] = ENTRY("download"),
  { 0 }
};

StringEntry lovrWinding[] = {
  [WINDING_COUNTERCLOCKWISE] = ENTRY("counterclockwise"),
  [WINDING_CLOCKWISE] = ENTRY("clockwise"),
  { 0 }
};

// Must be released when done
static Image* luax_checkimage(lua_State* L, int index, bool flip) {
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

static void luax_readcanvas(lua_State* L, int index, Canvas* canvas) {
  canvas->depth.enabled = true;
  canvas->depth.texture = NULL;
  canvas->depth.format = FORMAT_D16;
  canvas->depth.load = LOAD_CLEAR;
  canvas->depth.save = SAVE_DISCARD;
  canvas->depth.clear = 1.f;
  canvas->samples = 1;

  // Texture instead of table
  int type = lua_type(L, index);
  if (type == LUA_TUSERDATA) {
    canvas->color[0].texture = luax_checktype(L, index, Texture);
    canvas->color[0].load = LOAD_KEEP;
    canvas->color[0].save = SAVE_KEEP;
    return;
  } else if (type != LUA_TTABLE) {
    luaL_argerror(L, index, "Expected a Texture or table for a render target");
    return;
  }

  // Color targets
  int count = luax_len(L, index);
  for (int i = 0; i < count; i++) {
    lua_rawgeti(L, index, i + 1);
    canvas->color[i].texture = luax_totype(L, -1, Texture);
    lovrAssert(canvas->color[i].texture, "The numeric keys of a render target table must be Textures");
    lua_pop(L, 1);
  }

  lua_getfield(L, index, "load");
  if (lua_istable(L, -1)) {
    lua_rawgeti(L, -1, 1);
    if (lua_type(L, -1) == LUA_TNUMBER) {
      lua_rawgeti(L, -2, 2);
      lua_rawgeti(L, -3, 3);
      lua_rawgeti(L, -4, 4);
      canvas->color[0].load = LOAD_CLEAR;
      canvas->color[0].clear[0] = luax_checkfloat(L, -4);
      canvas->color[0].clear[1] = luax_checkfloat(L, -3);
      canvas->color[0].clear[2] = luax_checkfloat(L, -2);
      canvas->color[0].clear[3] = luax_optfloat(L, -1, 1.f);
      lua_pop(L, 4);
      for (int i = 1; i < count; i++) {
        canvas->color[i].load = LOAD_CLEAR;
        memcpy(canvas->color[i].clear, canvas->color[0].clear, 4 * sizeof(float));
      }
    } else {
      lua_pop(L, 1);
      for (int i = 0; i < count; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (lua_istable(L, -1)) {
          lua_rawgeti(L, -1, 1);
          lua_rawgeti(L, -2, 2);
          lua_rawgeti(L, -3, 3);
          lua_rawgeti(L, -4, 4);
          canvas->color[i].load = LOAD_CLEAR;
          canvas->color[i].clear[0] = luax_checkfloat(L, -4);
          canvas->color[i].clear[1] = luax_checkfloat(L, -3);
          canvas->color[i].clear[2] = luax_checkfloat(L, -2);
          canvas->color[i].clear[3] = luax_optfloat(L, -1, 1.f);
          lua_pop(L, 4);
        } else {
          canvas->color[i].load = lua_isnil(L, -1) || lua_toboolean(L, -1) ? LOAD_KEEP : LOAD_DISCARD;
        }
        lua_pop(L, 1);
      }
    }
  } else {
    LoadOp load = lua_isnil(L, -1) || lua_toboolean(L, -1) ? LOAD_KEEP : LOAD_DISCARD;
    for (int i = 0; i < count; i++) {
      canvas->color[i].load = load;
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "save");
  if (lua_istable(L, -1)) {
    for (int i = 0; i < count; i++) {
      lua_rawgeti(L, -1, i + 1);
      canvas->color[i].save = lua_isnil(L, -1) || lua_toboolean(L, -1) ? SAVE_KEEP : SAVE_DISCARD;
      lua_pop(L, 1);
    }
  } else {
    SaveOp save = lua_isnil(L, -1) || lua_toboolean(L, -1) ? SAVE_KEEP : SAVE_DISCARD;
    for (int i = 0; i < count; i++) {
      canvas->color[i].save = save;
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "multisamples");
  if (lua_toboolean(L, -1)) {
    for (int i = 0; i < count; i++) {
      canvas->color[i].resolve = canvas->color[i].texture;
      canvas->color[i].texture = NULL;
    }
    switch (lua_type(L, -1)) {
      case LUA_TBOOLEAN:
        canvas->samples = 4;
        break;
      case LUA_TNUMBER:
        canvas->samples = lua_tointeger(L, -1); // TODO PO2, zero, negative
        break;
      case LUA_TTABLE:
        for (int i = 0; i < count; i++) {
          lua_rawgeti(L, -1, i + 1);
          canvas->color[i].texture = luax_totype(L, -1, Texture);
          lua_pop(L, 1);
        }
        break;
      case LUA_TUSERDATA:
        canvas->color[0].texture = luax_checktype(L, -1, Texture);
        break;
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, index, "depth");
  switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
      canvas->depth.enabled = lua_toboolean(L, -1);
      break;
    case LUA_TSTRING:
      canvas->depth.format = luax_checkenum(L, -1, TextureFormat, NULL);
      break;
    case LUA_TUSERDATA:
      canvas->depth.texture = luax_checktype(L, -1, Texture);
      break;
    case LUA_TTABLE:
      lua_rawgeti(L, -1, 1);
      canvas->depth.texture = luax_totype(L, -1, Texture);
      lua_pop(L, 1);

      lua_getfield(L, -1, "format");
      canvas->depth.format = luax_checkenum(L, -1, TextureFormat, NULL);
      lua_pop(L, 1);

      lua_getfield(L, -1, "load");
      switch (lua_type(L, -1)) {
        case LUA_TNIL:
          canvas->depth.load = LOAD_KEEP;
          break;
        case LUA_TBOOLEAN:
          canvas->depth.load = lua_toboolean(L, -1) ? LOAD_KEEP : LOAD_DISCARD;
          break;
        case LUA_TNUMBER:
          canvas->depth.load = LOAD_CLEAR;
          canvas->depth.clear = lua_tonumber(L, -1);
          break;
        case LUA_TTABLE:
          //
          break;
      }
      lua_pop(L, 1);
  }
  lua_pop(L, 1);
}

static int l_lovrGraphicsCreateWindow(lua_State* L) {
  os_window_config window;
  memset(&window, 0, sizeof(window));

  if (!lua_toboolean(L, 1)) {
    return 0;
  }

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "width");
  window.width = luaL_optinteger(L, -1, 1080);
  lua_pop(L, 1);

  lua_getfield(L, 1, "height");
  window.height = luaL_optinteger(L, -1, 600);
  lua_pop(L, 1);

  lua_getfield(L, 1, "fullscreen");
  window.fullscreen = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "resizable");
  window.resizable = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "msaa");
  window.msaa = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "title");
  window.title = luaL_optstring(L, -1, "LÖVR");
  lua_pop(L, 1);

  lua_getfield(L, 1, "icon");
  Image* image = NULL;
  if (!lua_isnil(L, -1)) {
    image = luax_checkimage(L, -1, false);
    window.icon.data = image->blob->data;
    window.icon.width = image->width;
    window.icon.height = image->height;
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "vsync");
  window.vsync = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lovrGraphicsCreateWindow(&window);
  luax_atexit(L, lovrGraphicsDestroy); // The lua_State that creates the window shall be the one to destroy it
  lovrRelease(image, lovrImageDestroy);
  return 0;
}

static int l_lovrGraphicsHasWindow(lua_State* L) {
  bool window = lovrGraphicsHasWindow();
  lua_pushboolean(L, window);
  return 1;
}

static int l_lovrGraphicsGetWidth(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  return 1;
}

static int l_lovrGraphicsGetHeight(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 1;
}

static int l_lovrGraphicsGetDimensions(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 2;
}

static int l_lovrGraphicsGetPixelDensity(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetPixelDensity());
  return 1;
}

static int l_lovrGraphicsGetFeatures(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  GraphicsFeatures features;
  lovrGraphicsGetFeatures(&features);
  lua_pushboolean(L, features.bptc), lua_setfield(L, -2, "bptc");
  lua_pushboolean(L, features.astc), lua_setfield(L, -2, "astc");
  lua_pushboolean(L, features.pointSize), lua_setfield(L, -2, "pointSize");
  lua_pushboolean(L, features.wireframe), lua_setfield(L, -2, "wireframe");
  lua_pushboolean(L, features.multiview), lua_setfield(L, -2, "multiview");
  lua_pushboolean(L, features.multiblend), lua_setfield(L, -2, "multiblend");
  lua_pushboolean(L, features.anisotropy), lua_setfield(L, -2, "anisotropy");
  lua_pushboolean(L, features.depthClamp), lua_setfield(L, -2, "depthClamp");
  lua_pushboolean(L, features.depthNudgeClamp), lua_setfield(L, -2, "depthNudgeClamp");
  lua_pushboolean(L, features.clipDistance), lua_setfield(L, -2, "clipDistance");
  lua_pushboolean(L, features.cullDistance), lua_setfield(L, -2, "cullDistance");
  lua_pushboolean(L, features.fullIndexBufferRange), lua_setfield(L, -2, "fullIndexBufferRange");
  lua_pushboolean(L, features.indirectDrawCount), lua_setfield(L, -2, "indirectDrawCount");
  lua_pushboolean(L, features.indirectDrawFirstInstance), lua_setfield(L, -2, "indirectDrawFirstInstance");
  lua_pushboolean(L, features.extraShaderInputs), lua_setfield(L, -2, "extraShaderInputs");
  lua_pushboolean(L, features.dynamicIndexing), lua_setfield(L, -2, "dynamicIndexing");
  lua_pushboolean(L, features.float64), lua_setfield(L, -2, "float64");
  lua_pushboolean(L, features.int64), lua_setfield(L, -2, "int64");
  lua_pushboolean(L, features.int16), lua_setfield(L, -2, "int16");
  return 1;
}

static int l_lovrGraphicsGetLimits(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  GraphicsLimits limits;
  lovrGraphicsGetLimits(&limits);

  lua_pushinteger(L, limits.textureSize2D), lua_setfield(L, -2, "textureSize2D");
  lua_pushinteger(L, limits.textureSize3D), lua_setfield(L, -2, "textureSize3D");
  lua_pushinteger(L, limits.textureSizeCube), lua_setfield(L, -2, "textureSizeCube");
  lua_pushinteger(L, limits.textureLayers), lua_setfield(L, -2, "textureLayers");

  lua_createtable(L, 2, 0);
  lua_pushinteger(L, limits.renderSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.renderSize[1]), lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "renderSize");

  lua_pushinteger(L, limits.renderViews), lua_setfield(L, -2, "renderViews");
  lua_pushinteger(L, limits.bundleCount), lua_setfield(L, -2, "bundleCount");
  lua_pushinteger(L, limits.bundleSlots), lua_setfield(L, -2, "bundleSlots");
  lua_pushinteger(L, limits.uniformBufferRange), lua_setfield(L, -2, "uniformBufferRange");
  lua_pushinteger(L, limits.storageBufferRange), lua_setfield(L, -2, "storageBufferRange");
  lua_pushinteger(L, limits.uniformBufferAlign), lua_setfield(L, -2, "uniformBufferAlign");
  lua_pushinteger(L, limits.storageBufferAlign), lua_setfield(L, -2, "storageBufferAlign");
  lua_pushinteger(L, limits.vertexAttributes), lua_setfield(L, -2, "vertexAttributes");
  lua_pushinteger(L, limits.vertexAttributeOffset), lua_setfield(L, -2, "vertexAttributeOffset");
  lua_pushinteger(L, limits.vertexBuffers), lua_setfield(L, -2, "vertexBuffers");
  lua_pushinteger(L, limits.vertexBufferStride), lua_setfield(L, -2, "vertexBufferStride");
  lua_pushinteger(L, limits.vertexShaderOutputs), lua_setfield(L, -2, "vertexShaderOutputs");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.computeCount[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.computeCount[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.computeCount[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "computeCount");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.computeGroupSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.computeGroupSize[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.computeGroupSize[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "computeGroupSize");

  lua_pushinteger(L, limits.computeGroupVolume), lua_setfield(L, -2, "computeGroupVolume");
  lua_pushinteger(L, limits.computeSharedMemory), lua_setfield(L, -2, "computeSharedMemory");
  lua_pushinteger(L, limits.indirectDrawCount), lua_setfield(L, -2, "indirectDrawCount");
  lua_pushinteger(L, limits.allocationSize), lua_setfield(L, -2, "allocationSize");

  lua_createtable(L, 2, 0);
  lua_pushinteger(L, limits.pointSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.pointSize[1]), lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "pointSize");

  lua_pushnumber(L, limits.anisotropy), lua_setfield(L, -2, "anisotropy");
  return 1;
}

static int l_lovrGraphicsBegin(lua_State* L) {
  lovrGraphicsBegin();
  return 0;
}

static int l_lovrGraphicsFlush(lua_State* L) {
  lovrGraphicsFlush();
  return 0;
}

static int l_lovrGraphicsRender(lua_State* L) {
  Canvas canvas;
  memset(&canvas, 0, sizeof(canvas));
  luax_readcanvas(L, 1, &canvas);
  lovrGraphicsRender(&canvas);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_settop(L, 2);
  lua_call(L, 0, 0);
  lovrGraphicsEndPass();
  return 0;
}

static int l_lovrGraphicsBind(lua_State* L) {
  uint32_t group = luaL_checkinteger(L, 1);
  Bundle* bundle = luax_checktype(L, 2, Bundle);
  lovrGraphicsBind(group, bundle);
  return 0;
}

static int l_lovrGraphicsGetAlphaToCoverage(lua_State* L) {
  lua_pushboolean(L, lovrGraphicsGetAlphaToCoverage());
  return 1;
}

static int l_lovrGraphicsSetAlphaToCoverage(lua_State* L) {
  lovrGraphicsSetAlphaToCoverage(lua_toboolean(L, 1));
  return 1;
}

static int l_lovrGraphicsGetBlendMode(lua_State* L) {
  uint32_t target = luaL_optinteger(L, 1, 1) - 1;
  lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
  BlendMode mode;
  BlendAlphaMode alphaMode;
  lovrGraphicsGetBlendMode(target, &mode, &alphaMode);
  if (mode == BLEND_NONE) {
    lua_pushnil(L);
    return 1;
  } else {
    luax_pushenum(L, BlendMode, mode);
    luax_pushenum(L, BlendAlphaMode, alphaMode);
    return 2;
  }
}

static int l_lovrGraphicsSetBlendMode(lua_State* L) {
  if (lua_type(L, 1) == LUA_TNUMBER) {
    uint32_t target = lua_tonumber(L, 1) - 1;
    lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
    BlendMode mode = lua_isnoneornil(L, 2) ? BLEND_NONE : luax_checkenum(L, 2, BlendMode, NULL);
    BlendAlphaMode alphaMode = luax_checkenum(L, 3, BlendAlphaMode, "alphamultiply");
    lovrGraphicsSetBlendMode(target, mode, alphaMode);
    return 0;
  }

  BlendMode mode = lua_isnoneornil(L, 1) ? BLEND_NONE : luax_checkenum(L, 1, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 2, BlendAlphaMode, "alphamultiply");
  for (uint32_t i = 0; i < 4; i++) {
    lovrGraphicsSetBlendMode(i, mode, alphaMode);
  }
  return 0;
}

static int l_lovrGraphicsGetColorMask(lua_State* L) {
  uint32_t target = luaL_optinteger(L, 1, 1) - 1;
  lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
  bool r, g, b, a;
  lovrGraphicsGetColorMask(target, &r, &g, &b, &a);
  lua_pushboolean(L, r);
  lua_pushboolean(L, g);
  lua_pushboolean(L, b);
  lua_pushboolean(L, a);
  return 4;
}

static int l_lovrGraphicsSetColorMask(lua_State* L) {
  if (lua_type(L, 1) == LUA_TNUMBER) {
    uint32_t target = lua_tonumber(L, 1) - 1;
    lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
    bool r = lua_toboolean(L, 2);
    bool g = lua_toboolean(L, 3);
    bool b = lua_toboolean(L, 4);
    bool a = lua_toboolean(L, 5);
    lovrGraphicsSetColorMask(target, r, g, b, a);
    return 0;
  }

  bool r = lua_toboolean(L, 1);
  bool g = lua_toboolean(L, 2);
  bool b = lua_toboolean(L, 3);
  bool a = lua_toboolean(L, 4);
  for (uint32_t i = 0; i < 4; i++) {
    lovrGraphicsSetColorMask(i, r, g, b, a);
  }
  return 0;
}

static int l_lovrGraphicsGetCullMode(lua_State* L) {
  luax_pushenum(L, CullMode, lovrGraphicsGetCullMode());
  return 1;
}

static int l_lovrGraphicsSetCullMode(lua_State* L) {
  CullMode mode = luax_checkenum(L, 1, CullMode, "none");
  lovrGraphicsSetCullMode(mode);
  return 0;
}

static int l_lovrGraphicsGetDepthTest(lua_State* L) {
  CompareMode test;
  bool write;
  lovrGraphicsGetDepthTest(&test, &write);
  if (test == COMPARE_NONE) {
    lua_pushnil(L);
  } else {
    luax_pushenum(L, CompareMode, test);
  }
  lua_pushboolean(L, write);
  return 2;
}

static int l_lovrGraphicsSetDepthTest(lua_State* L) {
  CompareMode test = lua_isnoneornil(L, 1) ? COMPARE_NONE : luax_checkenum(L, 1, CompareMode, NULL);
  bool write = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
  lovrGraphicsSetDepthTest(test, write);
  return 0;
}

static int l_lovrGraphicsGetDepthNudge(lua_State* L) {
  float nudge, sloped, clamp;
  lovrGraphicsGetDepthNudge(&nudge, &sloped, &clamp);
  lua_pushnumber(L, nudge);
  lua_pushnumber(L, sloped);
  lua_pushnumber(L, clamp);
  return 3;
}

static int l_lovrGraphicsSetDepthNudge(lua_State* L) {
  float nudge = luax_optfloat(L, 1, 0.f);
  float sloped = luax_optfloat(L, 2, 0.f);
  float clamp = luax_optfloat(L, 3, 0.f);
  lovrGraphicsSetDepthNudge(nudge, sloped, clamp);
  return 0;
}

static int l_lovrGraphicsGetDepthClamp(lua_State* L) {
  bool clamp = lovrGraphicsGetDepthClamp();
  lua_pushboolean(L, clamp);
  return 1;
}

static int l_lovrGraphicsSetDepthClamp(lua_State* L) {
  bool clamp = lua_toboolean(L, 1);
  lovrGraphicsSetDepthClamp(clamp);
  return 0;
}

static int l_lovrGraphicsGetShader(lua_State* L) {
  Shader* shader = lovrGraphicsGetShader();
  luax_pushtype(L, Shader, shader);
  return 1;
}

static int l_lovrGraphicsSetShader(lua_State* L) {
  Shader* shader = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Shader);
  lovrGraphicsSetShader(shader);
  return 0;
}

static int l_lovrGraphicsGetStencilTest(lua_State* L) {
  CompareMode test;
  uint8_t value;
  lovrGraphicsGetStencilTest(&test, &value);
  if (test == COMPARE_NONE) {
    lua_pushnil(L);
    return 1;
  }
  luax_pushenum(L, CompareMode, test);
  lua_pushinteger(L, value);
  return 2;
}

static int l_lovrGraphicsSetStencilTest(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrGraphicsSetStencilTest(COMPARE_NONE, 0);
  } else {
    CompareMode test = luax_checkenum(L, 1, CompareMode, NULL);
    uint8_t value = luaL_checkinteger(L, 2);
    lovrGraphicsSetStencilTest(test, value);
  }
  return 0;
}

static int l_lovrGraphicsGetWinding(lua_State* L) {
  Winding winding = lovrGraphicsGetWinding();
  luax_pushenum(L, Winding, winding);
  return 1;
}

static int l_lovrGraphicsSetWinding(lua_State* L) {
  Winding winding = luax_checkenum(L, 1, Winding, NULL);
  lovrGraphicsSetWinding(winding);
  return 0;
}

static int l_lovrGraphicsIsWireframe(lua_State* L) {
  bool wireframe = lovrGraphicsIsWireframe();
  lua_pushboolean(L, wireframe);
  return 1;
}

static int l_lovrGraphicsSetWireframe(lua_State* L) {
  bool wireframe = lua_toboolean(L, 1);
  lovrGraphicsSetWireframe(wireframe);
  return 0;
}

static int l_lovrGraphicsPush(lua_State* L) {
  lovrGraphicsPush();
  return 0;
}

static int l_lovrGraphicsPop(lua_State* L) {
  lovrGraphicsPop();
  return 0;
}

static int l_lovrGraphicsOrigin(lua_State* L) {
  lovrGraphicsOrigin();
  return 0;
}

static int l_lovrGraphicsTranslate(lua_State* L) {
  float translation[4];
  luax_readvec3(L, 1, translation, NULL);
  lovrGraphicsTranslate(translation);
  return 0;
}

static int l_lovrGraphicsRotate(lua_State* L) {
  float rotation[4];
  luax_readquat(L, 1, rotation, NULL);
  lovrGraphicsRotate(rotation);
  return 0;
}

static int l_lovrGraphicsScale(lua_State* L) {
  float scale[4];
  luax_readscale(L, 1, scale, 3, NULL);
  lovrGraphicsScale(scale);
  return 0;
}

static int l_lovrGraphicsTransform(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 3);
  lovrGraphicsTransform(transform);
  return 0;
}

static int l_lovrGraphicsGetViewPose(lua_State* L) {
  uint32_t view = luaL_checkinteger(L, 1) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 1) {
    float* matrix = luax_checkvector(L, 2, V_MAT4, NULL);
    bool invert = lua_toboolean(L, 3);
    lovrGraphicsGetViewMatrix(view, matrix);
    if (!invert) mat4_invert(matrix);
    lua_settop(L, 2);
    return 1;
  } else {
    float matrix[16], angle, ax, ay, az;
    lovrGraphicsGetViewMatrix(view, matrix);
    mat4_invert(matrix);
    mat4_getAngleAxis(matrix, &angle, &ax, &ay, &az);
    lua_pushnumber(L, matrix[12]);
    lua_pushnumber(L, matrix[13]);
    lua_pushnumber(L, matrix[14]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 7;
  }
}

static int l_lovrGraphicsSetViewPose(lua_State* L) {
  uint32_t view = luaL_checkinteger(L, 1) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  VectorType type;
  float* p = luax_tovector(L, 2, &type);
  if (p && type == V_MAT4) {
    float matrix[16];
    mat4_init(matrix, p);
    bool inverted = lua_toboolean(L, 3);
    if (!inverted) mat4_invert(matrix);
    lovrGraphicsSetViewMatrix(view, matrix);
  } else {
    int index = 2;
    float position[4], orientation[4], matrix[16];
    index = luax_readvec3(L, index, position, "vec3, number, or mat4");
    index = luax_readquat(L, index, orientation, NULL);
    mat4_fromQuat(matrix, orientation);
    memcpy(matrix + 12, position, 3 * sizeof(float));
    mat4_invert(matrix);
    lovrGraphicsSetViewMatrix(view, matrix);
  }
  return 0;
}

static int l_lovrGraphicsGetProjection(lua_State* L) {
  uint32_t view = luaL_checkinteger(L, 1) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 1) {
    float* matrix = luax_checkvector(L, 2, V_MAT4, NULL);
    lovrGraphicsGetProjection(view, matrix);
    lua_settop(L, 2);
    return 1;
  } else {
    float matrix[16], left, right, up, down;
    lovrGraphicsGetProjection(view, matrix);
    mat4_getFov(matrix, &left, &right, &up, &down);
    lua_pushnumber(L, left);
    lua_pushnumber(L, right);
    lua_pushnumber(L, up);
    lua_pushnumber(L, down);
    return 4;
  }
}

static int l_lovrGraphicsSetProjection(lua_State* L) {
  uint32_t view = luaL_checkinteger(L, 1) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 2);
    float right = luax_checkfloat(L, 3);
    float up = luax_checkfloat(L, 4);
    float down = luax_checkfloat(L, 5);
    float clipNear = luax_optfloat(L, 6, .1f);
    float clipFar = luax_optfloat(L, 7, 100.f);
    float matrix[16];
    mat4_fov(matrix, left, right, up, down, clipNear, clipFar);
    lovrGraphicsSetProjection(view, matrix);
  } else {
    float* matrix = luax_checkvector(L, 2, V_MAT4, "mat4 or number");
    lovrGraphicsSetProjection(view, matrix);
  }
  return 0;
}

static void onStencil(void* userdata) {
  lua_State* L = userdata;
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_call(L, 0, 0);
}

static int l_lovrGraphicsStencil(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  StencilAction action = luax_checkenum(L, 2, StencilAction, "replace");
  uint8_t value = luaL_optinteger(L, 3, 1);
  StencilAction depthAction = luax_checkenum(L, 2, StencilAction, "keep");
  lovrGraphicsStencil(action, depthAction, value, onStencil, L);
  return 0;
}

static struct { uint16_t size, scalarAlign, baseAlign, components; } fieldInfo[] = {
  [FIELD_I8] = { 1, 1, 1, 1 },
  [FIELD_U8] = { 1, 1, 1, 1 },
  [FIELD_I16] = { 2, 2, 2, 1 },
  [FIELD_U16] = { 2, 2, 2, 1 },
  [FIELD_I32] = { 4, 4, 4, 1 },
  [FIELD_U32] = { 4, 4, 4, 1 },
  [FIELD_F32] = { 4, 4, 4, 1 },
  [FIELD_I8x2] = { 2, 1, 2, 2 },
  [FIELD_U8x2] = { 2, 1, 2, 2 },
  [FIELD_I8Nx2] = { 2, 1, 2, 2 },
  [FIELD_U8Nx2] = { 2, 1, 2, 2 },
  [FIELD_I16x2] = { 4, 2, 4, 2 },
  [FIELD_U16x2] = { 4, 2, 4, 2 },
  [FIELD_I16Nx2] = { 4, 2, 4, 2 },
  [FIELD_U16Nx2] = { 4, 2, 4, 2 },
  [FIELD_I32x2] = { 8, 4, 8, 2 },
  [FIELD_U32x2] = { 8, 4, 8, 2 },
  [FIELD_F32x2] = { 8, 4, 8, 2 },
  [FIELD_I32x3] = { 12, 4, 16, 3 },
  [FIELD_U32x3] = { 12, 4, 16, 3 },
  [FIELD_F32x3] = { 12, 4, 16, 3 },
  [FIELD_I8x4] = { 4, 1, 4, 4 },
  [FIELD_U8x4] = { 4, 1, 4, 4 },
  [FIELD_I8Nx4] = { 4, 1, 4, 4 },
  [FIELD_U8Nx4] = { 4, 1, 4, 4 },
  [FIELD_I16x4] = { 8, 2, 8, 4 },
  [FIELD_U16x4] = { 8, 2, 8, 4 },
  [FIELD_I16Nx4] = { 8, 2, 8, 4 },
  [FIELD_U16Nx4] = { 8, 2, 8, 4 },
  [FIELD_I32x4] = { 16, 4, 16, 4 },
  [FIELD_U32x4] = { 16, 4, 16, 4 },
  [FIELD_F32x4] = { 16, 4, 16, 4 },
  [FIELD_MAT2] = { 16, 4, 8, 4 },
  [FIELD_MAT3] = { 64, 4, 16, 9 },
  [FIELD_MAT4] = { 64, 4, 16, 16 }
};

static FieldType luax_checkfieldtype(lua_State* L, int index) {
  size_t length;
  const char* string = luaL_checklstring(L, index, &length);

  // Plural FieldTypes are allowed and ignored
  if (string[length - 1] == 's') {
    length--;
  }

  // vec[234] alias
  if (length == 4 && string[0] == 'v' && string[1] == 'e' && string[2] == 'c') {
    switch (string[3]) {
      case '2': return FIELD_F32x2;
      case '3': return FIELD_F32x3;
      case '4': return FIELD_F32x4;
      default: break;
    }
  }

  if (length == 4 && !memcmp(string, "byte", length)) {
    return FIELD_U8;
  }

  if (length == 3 && !memcmp(string, "int", length)) {
    return FIELD_I32;
  }

  if (length == 5 && !memcmp(string, "float", length)) {
    return FIELD_F32;
  }

  if (length == 5 && !memcmp(string, "color", length)) {
    return FIELD_U8Nx4;
  }

  for (int i = 0; lovrFieldType[i].length; i++) {
    if (length == lovrFieldType[i].length && !memcmp(string, lovrFieldType[i].string, length)) {
      return i;
    }
  }

  return luaL_error(L, "invalid FieldType '%s'", string);
}

static int l_lovrGraphicsNewBuffer(lua_State* L) {
  BufferInfo info = { .flags = BUFFER_WRITE | BUFFER_RETAIN };

  // Flags
  if (lua_istable(L, 3)) {
    for (int i = 0; lovrBufferFlag[i].length; i++) {
      lua_pushlstring(L, lovrBufferFlag[i].string, lovrBufferFlag[i].length);
      lua_gettable(L, 3);
      if (!lua_isnil(L, -1)) {
        if (lua_toboolean(L, -1)) {
          info.flags |= (1 << i);
        } else {
          info.flags &= ~(1 << i);
        }
      }
      lua_pop(L, 1);
    }

    lua_getfield(L, 3, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  // Format
  if (lua_isstring(L, 2)) {
    FieldType type = luax_checkfieldtype(L, 2);
    info.types[0] = type;
    info.offsets[0] = 0;
    info.fieldCount = 1;
    info.stride = fieldInfo[type].size;
  } else if (lua_istable(L, 2)) {
    uint16_t offset = 0;
    int length = luax_len(L, 2);
    bool blocky = info.flags & (BUFFER_UNIFORM | BUFFER_COMPUTE);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, 2, i + 1);
      switch (lua_type(L, -1)) {
        case LUA_TNUMBER: offset += lua_tonumber(L, -1); break;
        case LUA_TSTRING: {
          uint32_t index = info.fieldCount++;
          FieldType type = luax_checkfieldtype(L, -1);
          uint16_t align = blocky ? fieldInfo[type].baseAlign : fieldInfo[type].scalarAlign;
          info.types[index] = type;
          info.offsets[index] = ALIGN(offset, align);
          offset = info.offsets[index] + fieldInfo[type].size;
          break;
        }
        default: lovrThrow("Buffer format table may only contain FieldTypes and numbers (found %s)", lua_typename(L, lua_type(L, -1)));
      }
      lua_pop(L, 1);
    }
    info.stride = offset;
  } else {
    lovrThrow("Expected FieldType or table for Buffer format");
  }

  // std140 (only needed for uniform buffers, also as special case 'byte' formats skip this)
  if ((info.flags & BUFFER_UNIFORM) && info.stride > 1) {
    info.stride = ALIGN(info.stride, 16);
  }

  // Length/data
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      info.length = lua_tointeger(L, 1);
      break;
    case LUA_TTABLE: // Approximate
      lua_rawgeti(L, 1, 1);
      if (lua_istable(L, -1)) {
        info.length = luax_len(L, 1);
      } else if (lua_isuserdata(L, -1)) {
        info.length = luax_len(L, 1) / info.fieldCount;
      } else {
        uint32_t totalComponents = 0;
        for (uint32_t i = 0; i < info.fieldCount; i++) {
          totalComponents += fieldInfo[info.types[i]].components;
        }
        info.length = luax_len(L, 1) / totalComponents;
      }
      lua_pop(L, 1);
      break;
    default: {
      Blob* blob = luax_totype(L, 1, Blob);
      if (blob) {
        info.length = blob->size / info.stride;
        break;
      }
      return luax_typeerror(L, 1, "number, table, or Blob");
    }
  }

  void* data = NULL;
  info.initialContents = &data;
  Buffer* buffer = lovrBufferCreate(&info);

  if (!lua_isnumber(L, 1)) {
    lua_settop(L, 1);
    luax_readbufferdata(L, 1, buffer, data);
  }

  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewTexture(lua_State* L) {
  Texture* source = luax_totype(L, 1, Texture);

  if (source) {
    TextureView view = { .source = source };
    view.type = luax_checkenum(L, 2, TextureType, NULL);
    view.layerIndex = luaL_optinteger(L, 3, 1) - 1;
    view.layerCount = luaL_optinteger(L, 4, 1);
    view.mipmapIndex = luaL_optinteger(L, 5, 1) - 1;
    view.mipmapCount = luaL_optinteger(L, 6, 0);
    Texture* texture = lovrTextureCreateView(&view);
    luax_pushtype(L, Texture, texture);
    lovrRelease(texture, lovrTextureDestroy);
    return 1;
  }

  int index = 1;
  int argType = lua_type(L, index);
  bool blank = argType == LUA_TNUMBER;

  TextureInfo info = {
    .type = TEXTURE_2D,
    .format = FORMAT_RGBA8,
    .mipmaps = ~0u,
    .samples = 1,
    .usage = ~0u,
    .srgb = !blank
  };

  if (blank) {
    info.size[0] = lua_tointeger(L, index++);
    info.size[1] = luaL_checkinteger(L, index++);
    info.size[2] = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : 0;
  } else if (argType != LUA_TTABLE) {
    lua_createtable(L, 1, 0);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);
    lua_replace(L, 1);
    info.size[2] = 1;
    index++;
  } else {
    info.size[2] = luax_len(L, index++);
    info.type = info.size[2] > 0 ? TEXTURE_ARRAY : TEXTURE_CUBE;
  }

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "linear");
    info.srgb = lua_isnil(L, -1) ? info.srgb : !lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    if (lua_type(L, -1) == LUA_TNUMBER) {
      info.mipmaps = lua_tonumber(L, -1);
    } else {
      info.mipmaps = (lua_isnil(L, -1) || lua_toboolean(L, -1)) ? ~0u : 1;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, TextureType, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "format");
    info.format = lua_isnil(L, -1) ? info.format : luax_checkenum(L, -1, TextureFormat, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "samples");
    info.samples = lua_isnil(L, -1) ? info.samples : luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "usage");
    switch (lua_type(L, -1)) {
      case LUA_TSTRING:
        info.usage = 1 << luax_checkenum(L, -1, TextureUsage, NULL);
        break;
      case LUA_TTABLE:
        info.usage = 0;
        int length = luax_len(L, -1);
        for (int i = 0; i < length; i++) {
          lua_rawgeti(L, -1, i + 1);
          info.usage |= 1 << luax_checkenum(L, -1, TextureUsage, NULL);
          lua_pop(L, 1);
        }
        break;
      case LUA_TNIL:
        break;
      default:
        return luaL_error(L, "Texture usage flags must be a string or a table of strings");
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  Texture* texture;

  if (blank) {
    info.size[2] = info.size[2] > 0 ? info.size[2] : (info.type == TEXTURE_CUBE ? 6 : 1);
    texture = lovrTextureCreate(&info);
  } else {
    if (info.type == TEXTURE_CUBE && info.size[2] == 0) {
      info.size[2] = 6;
      const char* faces[6] = { "right", "left", "top", "bottom", "back", "front" };
      for (int i = 0; i < 6; i++) {
        lua_pushstring(L, faces[i]);
        lua_rawget(L, 1);
        lovrAssert(!lua_isnil(L, -1), "Could not load cubemap texture: missing '%s' face", faces[i]);
        lua_rawseti(L, 1, i + 1);
      }
    }

    lovrAssert(info.size[2] > 0, "No texture images specified");

    for (uint32_t i = 0; i < info.size[2]; i++) {
      lua_rawgeti(L, 1, i + 1);
      Image* image = luax_checkimage(L, -1, info.type != TEXTURE_CUBE);
      if (i == 0) {
        info.size[0] = image->width;
        info.size[1] = image->height;
        info.format = image->format;
        texture = lovrTextureCreate(&info);
      }
      //lovrTextureReplacePixels(texture, image, 0, 0, i, 0);
      lovrRelease(image, lovrImageDestroy);
      lua_pop(L, 1);
    }
  }

  luax_pushtype(L, Texture, texture);
  lovrRelease(texture, lovrTextureDestroy);
  return 1;
}

static int l_lovrGraphicsNewShader(lua_State* L) {
  const char* dynamicBuffers[64];

  ShaderInfo info = {
    .dynamicBuffers = dynamicBuffers
  };

  bool table = lua_istable(L, 2);

  if (lua_gettop(L) == 1 || table) {
    info.type = SHADER_COMPUTE;
    info.compute = luax_readblob(L, 1, "Shader");
  } else {
    info.type = SHADER_GRAPHICS;
    info.vertex = luax_readblob(L, 1, "Shader");
    info.fragment = luax_readblob(L, 2, "Shader");
  }

  if (table) {
    lua_getfield(L, 2, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "dynamicbuffers");
    if (lua_istable(L, -1)) {
      info.dynamicBufferCount = (uint32_t) luax_len(L, -1);
      lovrAssert(info.dynamicBufferCount <= COUNTOF(dynamicBuffers), "Too many dynamic buffers (max is %d, got %d)", COUNTOF(dynamicBuffers), info.dynamicBufferCount);
      for (uint32_t i = 0; i < info.dynamicBufferCount; i++) {
        lua_rawgeti(L, -1, i + 1);
        dynamicBuffers[i] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  Shader* shader = lovrShaderCreate(&info);
  luax_pushtype(L, Shader, shader);
  lovrRelease(shader, lovrShaderDestroy);
  lovrRelease(info.vertex, lovrBlobDestroy);
  lovrRelease(info.fragment, lovrBlobDestroy);
  lovrRelease(info.compute, lovrBlobDestroy);
  return 1;
}

static int l_lovrGraphicsNewBundle(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  uint32_t group = luaL_checkinteger(L, 2);
  Bundle* bundle = lovrBundleCreate(shader, group);

  if (lua_istable(L, 3)) {
    lua_pushnil(L);
    while (lua_next(L, 3)) {
      uint32_t id;

      switch (lua_type(L, -2)) {
        case LUA_TSTRING: {
          size_t length;
          const char* name = lua_tolstring(L, -2, &length);
          uint64_t hash = hash64(name, length);
          uint32_t g;
          bool exists = lovrShaderResolveName(shader, hash, &g, &id);
          lovrAssert(exists, "Shader has no variable named '%s', name");
          lovrAssert(g == group, "Variable '%s' is not in this Bundle's group!", name);
          break;
        }
        case LUA_TNUMBER:
          id = lua_tointeger(L, -2);
          break;
        default:
          break;
      }

      Buffer* buffer = luax_totype(L, -1, Buffer);
      Texture* texture = luax_totype(L, -1, Texture);
      lovrAssert(buffer || texture, "Expected a Buffer or a Texture for a bundle variable");
      if (buffer) {
        lovrBundleBindBuffer(bundle, id, 0, buffer, 0, ~0u);
      } else {
        lovrBundleBindTexture(bundle, id, 0, texture);
      }

      lua_pop(L, 1);
    }
  }

  luax_pushtype(L, Bundle, bundle);
  lovrRelease(bundle, lovrBundleDestroy);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {
  { "createWindow", l_lovrGraphicsCreateWindow },
  { "hasWindow", l_lovrGraphicsHasWindow },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "getPixelDensity", l_lovrGraphicsGetPixelDensity },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "begin", l_lovrGraphicsBegin },
  { "flush", l_lovrGraphicsFlush },
  { "render", l_lovrGraphicsRender },
  { "bind", l_lovrGraphicsBind },
  { "getAlphaToCoverage", l_lovrGraphicsGetAlphaToCoverage },
  { "setAlphaToCoverage", l_lovrGraphicsSetAlphaToCoverage },
  { "getBlendMode", l_lovrGraphicsGetBlendMode },
  { "setBlendMode", l_lovrGraphicsSetBlendMode },
  { "getColorMask", l_lovrGraphicsGetColorMask },
  { "setColorMask", l_lovrGraphicsSetColorMask },
  { "getCullMode", l_lovrGraphicsGetCullMode },
  { "setCullMode", l_lovrGraphicsSetCullMode },
  { "getDepthTest", l_lovrGraphicsGetDepthTest },
  { "setDepthTest", l_lovrGraphicsSetDepthTest },
  { "getDepthNudge", l_lovrGraphicsGetDepthNudge },
  { "setDepthNudge", l_lovrGraphicsSetDepthNudge },
  { "getDepthClamp", l_lovrGraphicsGetDepthClamp },
  { "setDepthClamp", l_lovrGraphicsSetDepthClamp },
  { "getShader", l_lovrGraphicsGetShader },
  { "setShader", l_lovrGraphicsSetShader },
  { "getStencilTest", l_lovrGraphicsGetStencilTest },
  { "setStencilTest", l_lovrGraphicsSetStencilTest },
  { "getWinding", l_lovrGraphicsGetWinding },
  { "setWinding", l_lovrGraphicsSetWinding },
  { "isWireframe", l_lovrGraphicsIsWireframe },
  { "setWireframe", l_lovrGraphicsSetWireframe },
  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
  { "transform", l_lovrGraphicsTransform },
  { "getViewPose", l_lovrGraphicsGetViewPose },
  { "setViewPose", l_lovrGraphicsSetViewPose },
  { "getProjection", l_lovrGraphicsGetProjection },
  { "setProjection", l_lovrGraphicsSetProjection },
  { "stencil", l_lovrGraphicsStencil },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newTexture", l_lovrGraphicsNewTexture },
  { "newShader", l_lovrGraphicsNewShader },
  { "newBundle", l_lovrGraphicsNewBundle },
  { NULL, NULL }
};

extern const luaL_Reg lovrBuffer[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrBundle[];

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  luax_registertype(L, Buffer);
  luax_registertype(L, Texture);
  luax_registertype(L, Shader);
  luax_registertype(L, Bundle);

  bool debug = false;
  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lovrGraphicsInit(debug);

  lua_pushcfunction(L, l_lovrGraphicsCreateWindow);
  lua_getfield(L, -2, "window");
  lua_call(L, 1, 0);
  lua_pop(L, 1);
  return 1;
}
