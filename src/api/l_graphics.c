#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/os.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

StringEntry lovrBatchType[] = {
  [BATCH_RENDER] = ENTRY("render"),
  [BATCH_COMPUTE] = ENTRY("compute"),
  { 0 }
};

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

StringEntry lovrBufferUsage[] = {
  [0] = ENTRY("vertex"),
  [1] = ENTRY("index"),
  [2] = ENTRY("uniform"),
  [3] = ENTRY("storage"),
  [4] = ENTRY("copyfrom"),
  [5] = ENTRY("copyto"),
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

StringEntry lovrDefaultSampler[] = {
  [SAMPLER_NEAREST] = ENTRY("nearest"),
  [SAMPLER_BILINEAR] = ENTRY("bilinear"),
  [SAMPLER_TRILINEAR] = ENTRY("trilinear"),
  [SAMPLER_ANISOTROPIC] = ENTRY("anisotropic"),
  { 0 }
};

StringEntry lovrDrawStyle[] = {
  [DRAW_LINE] = ENTRY("line"),
  [DRAW_FILL] = ENTRY("fill"),
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

StringEntry lovrFilterMode[] = {
  [FILTER_NEAREST] = ENTRY("nearest"),
  [FILTER_LINEAR] = ENTRY("linear"),
  { 0 }
};

StringEntry lovrShaderType[] = {
  [SHADER_GRAPHICS] = ENTRY("graphics"),
  [SHADER_COMPUTE] = ENTRY("compute"),
  { 0 }
};

StringEntry lovrStackType[] = {
  [STACK_TRANSFORM] = ENTRY("transform"),
  [STACK_PIPELINE] = ENTRY("pipeline"),
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
  [0] = ENTRY("sample"),
  [1] = ENTRY("render"),
  [2] = ENTRY("storage"),
  [3] = ENTRY("copyfrom"),
  [4] = ENTRY("copyto"),
  { 0 }
};

StringEntry lovrWinding[] = {
  [WINDING_COUNTERCLOCKWISE] = ENTRY("counterclockwise"),
  [WINDING_CLOCKWISE] = ENTRY("clockwise"),
  { 0 }
};

StringEntry lovrWrapMode[] = {
  [WRAP_CLAMP] = ENTRY("clamp"),
  [WRAP_REPEAT] = ENTRY("repeat"),
  [WRAP_MIRROR] = ENTRY("mirror"),
  { 0 }
};

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

uint32_t luax_checkfieldtype(lua_State* L, int index) {
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

static void luax_checkbufferformat(lua_State* L, int index, BufferInfo* info) {
  if (lua_isstring(L, index)) {
    FieldType type = luax_checkfieldtype(L, index);
    info->types[0] = type;
    info->offsets[0] = 0;
    info->fieldCount = 1;
    info->stride = fieldInfo[type].size;
  } else if (lua_istable(L, index)) {
    uint16_t offset = 0;
    int length = luax_len(L, index);
    bool blocky = info->usage & (BUFFER_UNIFORM | BUFFER_STORAGE);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, index, i + 1);
      switch (lua_type(L, -1)) {
        case LUA_TNUMBER: offset += lua_tonumber(L, -1); break;
        case LUA_TSTRING: {
          uint32_t index = info->fieldCount++;
          FieldType type = luax_checkfieldtype(L, -1);
          uint16_t align = blocky ? fieldInfo[type].baseAlign : fieldInfo[type].scalarAlign;
          info->types[index] = type;
          info->offsets[index] = ALIGN(offset, align);
          offset = info->offsets[index] + fieldInfo[type].size;
          break;
        }
        default: lovrThrow("Buffer format table may only contain FieldTypes and numbers (found %s)", lua_typename(L, lua_type(L, -1)));
      }
      lua_pop(L, 1);
    }
    info->stride = offset;
  } else {
    lovrThrow("Expected FieldType or table for Buffer format");
  }

  // std140 (only needed for uniform buffers, also as special case 'byte' formats skip this)
  if (info->usage & BUFFER_UNIFORM && info->stride > 1) {
    info->stride = ALIGN(info->stride, 16);
  }
}

static int l_lovrGraphicsInit(lua_State* L) {
  bool debug = false;
  uint32_t blockSize = 1 << 24;
  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "blocksize");
    blockSize = luaL_optinteger(L, -1, blockSize);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (lovrGraphicsInit(debug, blockSize)) {
    luax_atexit(L, lovrGraphicsDestroy);
  }

  return 0;
}

static int l_lovrGraphicsGetHardware(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  GraphicsHardware hardware;
  lovrGraphicsGetHardware(&hardware);
  lua_pushinteger(L, hardware.deviceId), lua_setfield(L, -2, "id");
  lua_pushstring(L, hardware.deviceName), lua_setfield(L, -2, "name");
  lua_pushinteger(L, hardware.vendorId), lua_setfield(L, -2, "vendor");
  lua_pushfstring(L, "%d.%d.%d", hardware.driverMajor, hardware.driverMinor, hardware.driverPatch), lua_setfield(L, -2, "version");
  lua_pushboolean(L, hardware.discrete), lua_setfield(L, -2, "discrete");
  lua_pushstring(L, hardware.renderer), lua_setfield(L, -2, "renderer");
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
  lua_pushboolean(L, features.multiblend), lua_setfield(L, -2, "multiblend");
  lua_pushboolean(L, features.anisotropy), lua_setfield(L, -2, "anisotropy");
  lua_pushboolean(L, features.depthClamp), lua_setfield(L, -2, "depthClamp");
  lua_pushboolean(L, features.depthNudgeClamp), lua_setfield(L, -2, "depthNudgeClamp");
  lua_pushboolean(L, features.clipDistance), lua_setfield(L, -2, "clipDistance");
  lua_pushboolean(L, features.cullDistance), lua_setfield(L, -2, "cullDistance");
  lua_pushboolean(L, features.fullIndexBufferRange), lua_setfield(L, -2, "fullIndexBufferRange");
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
  lua_pushinteger(L, limits.renderWidth), lua_setfield(L, -2, "renderWidth");
  lua_pushinteger(L, limits.renderHeight), lua_setfield(L, -2, "renderHeight");
  lua_pushinteger(L, limits.renderViews), lua_setfield(L, -2, "renderViews");
  lua_pushinteger(L, limits.bundleCount), lua_setfield(L, -2, "bundleCount");
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

static int l_lovrGraphicsSubmit(lua_State* L) {
  lovrGraphicsSubmit();
  return 0;
}

static int l_lovrGraphicsRender(lua_State* L) {
  Canvas canvas = {
    .load = { .color = { LOAD_CLEAR, LOAD_CLEAR, LOAD_CLEAR, LOAD_CLEAR }, .depth = LOAD_CLEAR, .stencil = LOAD_CLEAR },
    .store = { .color = { STORE_KEEP, STORE_KEEP, STORE_KEEP, STORE_KEEP }, .depth = STORE_DISCARD, .stencil = STORE_DISCARD },
    .clear = { .depth = 1.f, .stencil = 0 },
    .depthFormat = FORMAT_D16,
    .samples = 4,
    .views = 2
  };

  if (lua_type(L, 1) == LUA_TSTRING && !strcmp(lua_tostring(L, 1), "window")) {
    canvas.textures.color[0] = lovrGraphicsGetWindowTexture();
  } else if (lua_isuserdata(L, 1)) {
    canvas.textures.color[0] = luax_checktype(L, 1, Texture);
  } else {
    luaL_checktype(L, 1, LUA_TTABLE);

    for (uint32_t i = 0; i < 4; i++) {
      lua_rawgeti(L, 1, i + 1);
      if (lua_isnil(L, -1)) {
        break;
      } else if (lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "window")) {
        canvas.textures.color[i] = lovrGraphicsGetWindowTexture();
      } else {
        canvas.textures.color[i] = luax_checktype(L, -1, Texture);
      }
      lua_pop(L, 1);
    }

    lua_getfield(L, 1, "depth");
    switch (lua_type(L, -1)) {
      case LUA_TBOOLEAN: canvas.depthFormat = lua_toboolean(L, -1) ? canvas.depthFormat : 0; break;
      case LUA_TSTRING: canvas.depthFormat = luax_checkenum(L, -1, TextureFormat, NULL); break;
      case LUA_TUSERDATA: canvas.textures.depth = luax_checktype(L, -1, Texture); break;
      case LUA_TTABLE:
        lua_getfield(L, -1, "format");
        canvas.depthFormat = lua_isnil(L, -1) ? canvas.depthFormat : luax_checkenum(L, -1, TextureFormat, NULL);
        lua_pop(L, 1);

        lua_getfield(L, -1, "texture");
        canvas.textures.depth = lua_isnil(L, -1) ? NULL : luax_checktype(L, -1, Texture);
        lua_pop(L, 1);

        lua_getfield(L, -1, "clear");
        switch (lua_type(L, -1)) {
          case LUA_TNIL: break;
          case LUA_TBOOLEAN: canvas.load.depth = canvas.load.stencil = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP; break;
          case LUA_TNUMBER: canvas.clear.depth = lua_tonumber(L, -1); break;
          case LUA_TTABLE:
            lua_rawgeti(L, -1, 1);
            if (lua_isboolean(L, -1)) {
              canvas.load.depth = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
            } else if (lua_isnumber(L, -1)) {
              canvas.clear.depth = lua_tonumber(L, -1);
            } else {
              lovrThrow("Expected boolean or number for depth load");
            }

            lua_rawgeti(L, -1, 2);
            if (lua_isboolean(L, -1)) {
              canvas.load.stencil = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
            } else if (lua_isnumber(L, -1)) {
              canvas.clear.stencil = (uint8_t) lua_tointeger(L, -1);
            } else {
              lovrThrow("Expected boolean or number for stencil load");
            }
            lua_pop(L, 2);
            break;
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "save");
        if (lua_istable(L, -1)) {
          lua_rawgeti(L, -1, 1);
          canvas.store.depth = lua_toboolean(L, -1) ? STORE_KEEP : STORE_DISCARD;
          lua_rawgeti(L, -1, 2);
          canvas.store.stencil = lua_toboolean(L, -1) ? STORE_KEEP : STORE_DISCARD;
          lua_pop(L, 2);
        } else {
          canvas.store.depth = canvas.store.stencil = lua_toboolean(L, -1) ? STORE_KEEP : STORE_DISCARD;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "clear");
    if (lua_istable(L, -1)) {
      lua_rawgeti(L, -1, 1);
      if (lua_type(L, -1) == LUA_TNUMBER) {
        lua_rawgeti(L, -2, 2);
        lua_rawgeti(L, -3, 3);
        lua_rawgeti(L, -4, 4);
        canvas.clear.color[0][0] = luax_checkfloat(L, -4);
        canvas.clear.color[0][1] = luax_checkfloat(L, -3);
        canvas.clear.color[0][2] = luax_checkfloat(L, -2);
        canvas.clear.color[0][3] = luax_optfloat(L, -1, 1.f);
        lua_pop(L, 4);
        for (uint32_t i = 1; i < 4; i++) {
          memcpy(canvas.clear.color[i], canvas.clear.color[0], 4 * sizeof(float));
        }
      } else {
        lua_pop(L, 1);
        for (uint32_t i = 0; i < 4; i++) {
          lua_rawgeti(L, -1, i + 1);
          if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            lua_rawgeti(L, -2, 2);
            lua_rawgeti(L, -3, 3);
            lua_rawgeti(L, -4, 4);
            canvas.clear.color[i][0] = luax_checkfloat(L, -4);
            canvas.clear.color[i][1] = luax_checkfloat(L, -3);
            canvas.clear.color[i][2] = luax_checkfloat(L, -2);
            canvas.clear.color[i][3] = luax_optfloat(L, -1, 1.f);
            lua_pop(L, 4);
          } else {
            canvas.load.color[i] = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
          }
          lua_pop(L, 1);
        }
      }
    } else if (!lua_isnil(L, -1)) {
      LoadOp load = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
      canvas.load.color[0] = canvas.load.color[1] = canvas.load.color[2] = canvas.load.color[3] = load;
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "samples");
    canvas.samples = lua_isnil(L, -1) ? canvas.samples : lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "views");
    canvas.views = lua_isnil(L, -1) ? canvas.views : luaL_checkinteger(L, -1);
    lua_pop(L, 1);
  }

  Batch* batch;
  if (lua_type(L, 2) == LUA_TFUNCTION) {
    batch = lovrGraphicsGetBatch(&(BatchInfo) { .capacity = 1024 });
    lua_settop(L, 2);
    luax_pushtype(L, Batch, batch);
    lua_call(L, 1, 0);
  } else {
    batch = luax_checktype(L, 2, Batch);
    lovrRetain(batch);
  }

  lovrGraphicsRender(&canvas, &batch, 1, 0);
  lovrRelease(batch, lovrBatchDestroy);
  return 0;
}

static int l_lovrGraphicsGetBuffer(lua_State* L) {
  BufferInfo info = { .usage = BUFFER_VERTEX | BUFFER_INDEX | BUFFER_UNIFORM | BUFFER_COPYFROM };

  // Format
  luax_checkbufferformat(L, 2, &info);

  // Length/contents
  int dataType = lua_type(L, 1);
  if (dataType == LUA_TNUMBER) {
    info.length = lua_tointeger(L, 1);
  } else if (dataType == LUA_TTABLE) {
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
  } else {
    return luax_typeerror(L, 1, "number or table");
  }

  Buffer* buffer = lovrGraphicsGetBuffer(&info);

  if (dataType != LUA_TNUMBER) {
    lua_settop(L, 1);
    luax_readbufferdata(L, 1, buffer);
  }

  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewBuffer(lua_State* L) {
  BufferInfo info = { 0 };

  // Flags
  if (lua_istable(L, 3)) {
    lua_getfield(L, 3, "usage");
    switch (lua_type(L, -1)) {
      case LUA_TSTRING: info.usage = luax_checkenum(L, -1, BufferUsage, NULL); break;
      case LUA_TTABLE: {
        int length = luax_len(L, -1);
        for (int i = 0; i < length; i++) {
          lua_rawgeti(L, -1, i + 1);
          info.usage |= 1 << luax_checkenum(L, -1, BufferUsage, NULL);
          lua_pop(L, 1);
        }
      }
      case LUA_TNIL: info.usage = ~0u;
      default: return luaL_error(L, "Expected Buffer usage to be a string, table, or nil");
    }
    lua_pop(L, 1);

    lua_getfield(L, 3, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  // Format
  luax_checkbufferformat(L, 2, &info);

  // Length/contents
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
    case LUA_TSTRING: {
      Blob* blob = luax_readblob(L, 1, "Buffer data");
      info.length = blob->size / info.stride;
      luax_pushtype(L, Blob, blob);
      lua_replace(L, 1);
      lovrRelease(blob, lovrBlobDestroy);
      break;
    }
    default: {
      Blob* blob = luax_totype(L, 1, Blob);
      if (blob) {
        info.length = blob->size / info.stride;
        break;
      }
      return luax_typeerror(L, 1, "number, table, or Blob");
    }
  }

  Buffer* buffer = lovrBufferCreate(&info);

  if (!lua_isnumber(L, 1)) {
    lua_settop(L, 1);
    luax_readbufferdata(L, 1, buffer);
  }

  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewTexture(lua_State* L) {
  Texture* parent = luax_totype(L, 1, Texture);

  if (parent) {
    TextureViewInfo view = { .parent = parent };
    view.type = luax_checkenum(L, 2, TextureType, NULL);
    view.layerIndex = luaL_optinteger(L, 3, 1) - 1;
    view.layerCount = luaL_optinteger(L, 4, 1);
    view.levelIndex = luaL_optinteger(L, 5, 1) - 1;
    view.levelCount = luaL_optinteger(L, 6, 0);
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
    .usage = TEXTURE_SAMPLE,
    .srgb = !blank
  };

  if (blank) {
    info.width = lua_tointeger(L, index++);
    info.height = luaL_checkinteger(L, index++);
    info.depth = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : 0;
    switch (info.depth) {
      case 0: info.type = TEXTURE_2D; break;
      case 1: info.type = TEXTURE_2D; break;
      case 6: info.type = TEXTURE_CUBE; break;
      default: info.type = TEXTURE_ARRAY; break;
    }
  } else if (argType != LUA_TTABLE) {
    lua_createtable(L, 1, 0);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);
    lua_replace(L, 1);
    info.depth = 1;
    index++;
  } else {
    info.depth = luax_len(L, index++);
    info.type = info.depth > 0 ? TEXTURE_ARRAY : TEXTURE_CUBE;
  }

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, TextureType, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "format");
    info.format = lua_isnil(L, -1) ? info.format : luax_checkenum(L, -1, TextureFormat, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "linear");
    info.srgb = lua_isnil(L, -1) ? info.srgb : !lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "samples");
    info.samples = lua_isnil(L, -1) ? info.samples : luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    if (lua_type(L, -1) == LUA_TNUMBER) {
      info.mipmaps = lua_tonumber(L, -1);
    } else if (!lua_isnil(L, -1)) {
      info.mipmaps = lua_toboolean(L, -1) ? ~0u : 1;
    } else {
      info.mipmaps = info.samples > 1 ? 1 : ~0u;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "usage");
    switch (lua_type(L, -1)) {
      case LUA_TSTRING: info.usage = 1 << luax_checkenum(L, -1, TextureUsage, NULL); break;
      case LUA_TTABLE: {
        int length = luax_len(L, -1);
        for (int i = 0; i < length; i++) {
          lua_rawgeti(L, -1, i + 1);
          info.usage |= 1 << luax_checkenum(L, -1, TextureUsage, NULL);
          lua_pop(L, 1);
        }
        break;
      }
      case LUA_TNIL: info.usage = ~0u; break;
      default: return luaL_error(L, "Expected Texture usage to be a string, table, or nil");
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  Texture* texture;

  if (blank) {
    info.depth = info.depth > 0 ? info.depth : (info.type == TEXTURE_CUBE ? 6 : 1);
    texture = lovrTextureCreate(&info);
  } else {
    if (info.type == TEXTURE_CUBE && info.depth == 0) {
      info.depth = 6;
      const char* faces[6] = { "right", "left", "top", "bottom", "back", "front" };
      for (int i = 0; i < 6; i++) {
        lua_pushstring(L, faces[i]);
        lua_rawget(L, 1);
        lovrAssert(!lua_isnil(L, -1), "Could not load cubemap texture: missing '%s' face", faces[i]);
        lua_rawseti(L, 1, i + 1);
      }
    }

    lovrAssert(info.depth > 0, "No texture images specified");

    for (uint32_t i = 0; i < info.depth; i++) {
      lua_rawgeti(L, 1, i + 1);
      Image* image = luax_checkimage(L, -1, info.type != TEXTURE_CUBE);
      if (i == 0) {
        info.width = image->width;
        info.height = image->height;
        info.format = image->format;
        texture = lovrTextureCreate(&info);
      } else {
        lovrAssert(image->width == info.width, "Image widths must match");
        lovrAssert(image->height == info.height, "Image heights must match");
        lovrAssert(image->format == info.format, "Image formats must match");
      }
      uint16_t srcOffset[4] = { 0, 0, i, 0 };
      uint16_t dstOffset[2] = { 0, 0 };
      uint16_t extent[3] = { info.width, info.height, 1 };
      lovrTexturePaste(texture, image, srcOffset, dstOffset, extent);
      lovrRelease(image, lovrImageDestroy);
      lua_pop(L, 1);
    }
  }

  luax_pushtype(L, Texture, texture);
  lovrRelease(texture, lovrTextureDestroy);
  return 1;
}

static int l_lovrGraphicsNewSampler(lua_State* L) {
  SamplerInfo info = {
    .min = FILTER_LINEAR,
    .mag = FILTER_LINEAR,
    .mip = FILTER_LINEAR,
    .wrap = { WRAP_REPEAT, WRAP_REPEAT, WRAP_REPEAT },
    .range = { 0.f, -1.f }
  };

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "filter");
  if (lua_isstring(L, -1)) {
    info.min = info.mag = info.mip = luax_checkenum(L, -1, FilterMode, NULL);
  } else if (lua_istable(L, -1)) {
    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    lua_rawgeti(L, -3, 3);
    info.min = luax_checkenum(L, -3, FilterMode, NULL);
    info.mag = luax_checkenum(L, -2, FilterMode, NULL);
    info.mip = luax_checkenum(L, -1, FilterMode, NULL);
    lua_pop(L, 3);
  } else if (!lua_isnil(L, -1)) {
    lovrThrow("Expected string or table for Sampler filter");
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "wrap");
  if (lua_isstring(L, -1)) {
    info.wrap[0] = info.wrap[1] = info.wrap[2] = luax_checkenum(L, -1, WrapMode, NULL);
  } else if (lua_istable(L, -1)) {
    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    lua_rawgeti(L, -3, 3);
    info.wrap[0] = luax_checkenum(L, -3, WrapMode, NULL);
    info.wrap[1] = luax_checkenum(L, -2, WrapMode, NULL);
    info.wrap[2] = luax_checkenum(L, -1, WrapMode, NULL);
    lua_pop(L, 3);
  } else if (!lua_isnil(L, -1)) {
    lovrThrow("Expected string or table for Sampler filter");
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "compare");
  info.compare = luax_checkenum(L, -1, CompareMode, "none");
  lua_pop(L, 1);

  lua_getfield(L, 1, "anisotropy");
  info.anisotropy = luax_optfloat(L, -1, 0.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "mipmaprange");
  if (!lua_isnil(L, -1)) {
    lovrAssert(lua_istable(L, -1), "Sampler mipmap range must be nil or a table");
    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    info.range[0] = luax_checkfloat(L, -2);
    info.range[1] = luax_checkfloat(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);

  Sampler* sampler = lovrSamplerCreate(&info);
  luax_pushtype(L, Sampler, sampler);
  lovrRelease(sampler, lovrSamplerDestroy);
  return 1;
}

static int l_lovrGraphicsNewShader(lua_State* L) {
  const char* dynamicBuffers[64];
  ShaderInfo info = { .dynamicBuffers = dynamicBuffers };
  Blob* blobs[2] = { NULL, NULL };

  bool table = lua_istable(L, 2);

  if (lua_gettop(L) == 1 || table) {
    blobs[0] = luax_readblob(L, 1, "Shader");
    info.source[0] = blobs[0]->data;
    info.length[0] = blobs[0]->size;
    info.type = SHADER_COMPUTE;
  } else {
    blobs[0] = luax_readblob(L, 1, "Shader");
    blobs[1] = luax_readblob(L, 2, "Shader");
    info.source[0] = blobs[0]->data;
    info.length[0] = blobs[0]->size;
    info.source[1] = blobs[1]->data;
    info.length[1] = blobs[1]->size;
    info.type = SHADER_GRAPHICS;
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
  lovrRelease(blobs[0], lovrBlobDestroy);
  lovrRelease(blobs[1], lovrBlobDestroy);
  return 1;
}

static int l_lovrGraphicsGetBatch(lua_State* L) {
  BatchInfo info = { .type = BATCH_RENDER };
  info.capacity = luaL_checkinteger(L, 1);

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, BatchType, NULL);
    lua_pop(L, 1);
  }

  Batch* batch = lovrGraphicsGetBatch(&info);
  luax_pushtype(L, Batch, batch);
  lovrRelease(batch, lovrBatchDestroy);
  return 1;
}

static int l_lovrGraphicsNewBatch(lua_State* L) {
  BatchInfo info = { .type = BATCH_RENDER };
  info.capacity = luaL_checkinteger(L, 1);

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, BatchType, NULL);
    lua_pop(L, 1);
  }

  Batch* batch = lovrBatchCreate(&info);
  luax_pushtype(L, Batch, batch);
  lovrRelease(batch, lovrBatchDestroy);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {
  { "init", l_lovrGraphicsInit },
  { "getHardware", l_lovrGraphicsGetHardware },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "begin", l_lovrGraphicsBegin },
  { "submit", l_lovrGraphicsSubmit },
  { "render", l_lovrGraphicsRender },
  { "getBuffer", l_lovrGraphicsGetBuffer },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newTexture", l_lovrGraphicsNewTexture },
  { "newSampler", l_lovrGraphicsNewSampler },
  { "newShader", l_lovrGraphicsNewShader },
  { "getBatch", l_lovrGraphicsGetBatch },
  { "newBatch", l_lovrGraphicsNewBatch },
  { NULL, NULL }
};

extern const luaL_Reg lovrBuffer[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrSampler[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrBatch[];

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  luax_registertype(L, Buffer);
  luax_registertype(L, Texture);
  luax_registertype(L, Sampler);
  luax_registertype(L, Shader);
  luax_registertype(L, Batch);
  return 1;
}
