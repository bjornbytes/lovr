#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "core/os.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
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

StringEntry lovrDefaultAttribute[] = {
  [ATTRIBUTE_POSITION] = ENTRY("position"),
  [ATTRIBUTE_NORMAL] = ENTRY("normal"),
  [ATTRIBUTE_TEXCOORD] = ENTRY("texcoord"),
  [ATTRIBUTE_COLOR] = ENTRY("color"),
  { 0 }
};

StringEntry lovrDefaultSampler[] = {
  [SAMPLER_NEAREST] = ENTRY("nearest"),
  [SAMPLER_BILINEAR] = ENTRY("bilinear"),
  [SAMPLER_TRILINEAR] = ENTRY("trilinear"),
  [SAMPLER_ANISOTROPIC] = ENTRY("anisotropic"),
  { 0 }
};

StringEntry lovrDefaultShader[] = {
  [SHADER_UNLIT] = ENTRY("unlit"),
  { 0 }
};

StringEntry lovrDrawMode[] = {
  [DRAW_POINTS] = ENTRY("points"),
  [DRAW_LINES] = ENTRY("lines"),
  [DRAW_TRIANGLES] = ENTRY("triangles"),
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
  [FIELD_U10Nx3] = ENTRY("u10nx3"),
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

StringEntry lovrHorizontalAlign[] = {
  [ALIGN_LEFT] = ENTRY("left"),
  [ALIGN_CENTER] = ENTRY("center"),
  [ALIGN_RIGHT] = ENTRY("right"),
  { 0 }
};

StringEntry lovrPassType[] = {
  [PASS_RENDER] = ENTRY("render"),
  [PASS_COMPUTE] = ENTRY("compute"),
  [PASS_TRANSFER] = ENTRY("transfer"),
  [PASS_BATCH] = ENTRY("batch"),
  { 0 }
};

StringEntry lovrShaderType[] = {
  [SHADER_GRAPHICS] = ENTRY("graphics"),
  [SHADER_COMPUTE] = ENTRY("compute"),
  { 0 }
};

StringEntry lovrSortMode[] = {
  [SORT_OPAQUE] = ENTRY("opaque"),
  [SORT_TRANSPARENT] = ENTRY("transparent"),
  { 0 }
};

StringEntry lovrStackType[] = {
  [STACK_TRANSFORM] = ENTRY("transform"),
  [STACK_PIPELINE] = ENTRY("pipeline"),
  [STACK_LABEL] = ENTRY("label"),
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

StringEntry lovrTextureFeature[] = {
  [0] = ENTRY("sample"),
  [1] = ENTRY("filter"),
  [2] = ENTRY("render"),
  [3] = ENTRY("blend"),
  [4] = ENTRY("storage"),
  [5] = ENTRY("blit"),
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

StringEntry lovrVerticalAlign[] = {
  [ALIGN_TOP] = ENTRY("top"),
  [ALIGN_MIDDLE] = ENTRY("middle"),
  [ALIGN_BOTTOM] = ENTRY("bottom"),
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

static struct { uint32_t size, scalarAlign, baseAlign, components; } fieldInfo[] = {
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
  [FIELD_U10Nx3] = { 4, 4, 4, 3 },
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
    info->stride = fieldInfo[type].size;
    info->fieldCount = 1;
    info->locations[0] = 0;
    info->offsets[0] = 0;
    info->types[0] = type;
  } else if (lua_istable(L, index)) {
    uint32_t offset = 0;
    int length = luax_len(L, index);
    bool blocky = info->usage & (BUFFER_UNIFORM | BUFFER_STORAGE);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, index, i + 1);
      switch (lua_type(L, -1)) {
        case LUA_TNUMBER: offset += lua_tonumber(L, -1); break;
        case LUA_TSTRING: {
          uint32_t idx = info->fieldCount++;
          FieldType type = luax_checkfieldtype(L, -1);
          uint16_t align = blocky ? fieldInfo[type].baseAlign : fieldInfo[type].scalarAlign;
          info->locations[idx] = idx;
          info->offsets[idx] = ALIGN(offset, align);
          info->types[idx] = type;
          offset = info->offsets[idx] + fieldInfo[type].size;
          break;
        }
        case LUA_TTABLE:
          lua_rawgeti(L, -1, 1);
          lua_rawgeti(L, -2, 2);
          uint32_t idx = info->fieldCount++;
          if (lua_type(L, -2) == LUA_TNUMBER) {
            info->locations[idx] = lua_tointeger(L, -2);
          } else {
            info->locations[idx] = luax_checkenum(L, -2, DefaultAttribute, NULL);
          }
          FieldType type = luax_checkfieldtype(L, -1);
          uint16_t align = blocky ? fieldInfo[type].baseAlign : fieldInfo[type].scalarAlign;
          info->offsets[idx] = ALIGN(offset, align);
          info->types[idx] = type;
          offset = info->offsets[idx] + fieldInfo[type].size;
          lua_pop(L, 2);
          break;
        default: lovrThrow("Buffer format table may only contain FieldTypes, numbers, and tables (found %s)", lua_typename(L, lua_type(L, -1)));
      }
      lua_pop(L, 1);
    }
    info->stride = offset;
  } else {
    lovrThrow("Expected FieldType or table for Buffer format");
  }
}

static Canvas luax_checkcanvas(lua_State* L, int index) {
  Canvas canvas = {
    .loads = { LOAD_CLEAR, LOAD_CLEAR, LOAD_CLEAR, LOAD_CLEAR },
    .depth.format = FORMAT_D16,
    .depth.load = LOAD_CLEAR,
    .depth.clear = 1.f,
    .samples = 4
  };

  for (uint32_t i = 0; i < 4; i++) {
    lovrGraphicsGetBackgroundColor(canvas.clears[i]);
  }

  if (lua_type(L, index) == LUA_TSTRING && !strcmp(lua_tostring(L, index), "window")) {
    canvas.textures[0] = lovrGraphicsGetWindowTexture();
  } else if (lua_isuserdata(L, index)) {
    canvas.textures[0] = luax_checktype(L, index, Texture);
  } else {
    luaL_checktype(L, index, LUA_TTABLE);

    for (uint32_t i = 0; i < 4; i++) {
      lua_rawgeti(L, 1, index + 1);
      if (lua_isnil(L, -1)) {
        break;
      } else if (lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "window")) {
        canvas.textures[i] = lovrGraphicsGetWindowTexture();
      } else {
        canvas.textures[i] = luax_checktype(L, -1, Texture);
      }
      lua_pop(L, 1);
    }

    lua_getfield(L, index, "depth");
    switch (lua_type(L, -1)) {
      case LUA_TBOOLEAN: canvas.depth.format = lua_toboolean(L, -1) ? canvas.depth.format : 0; break;
      case LUA_TSTRING: canvas.depth.format = luax_checkenum(L, -1, TextureFormat, NULL); break;
      case LUA_TUSERDATA: canvas.depth.texture = luax_checktype(L, -1, Texture); break;
      case LUA_TTABLE:
        lua_getfield(L, -1, "format");
        canvas.depth.format = lua_isnil(L, -1) ? canvas.depth.format : luax_checkenum(L, -1, TextureFormat, NULL);
        lua_pop(L, 1);

        lua_getfield(L, -1, "texture");
        canvas.depth.texture = lua_isnil(L, -1) ? NULL : luax_checktype(L, -1, Texture);
        lua_pop(L, 1);

        lua_getfield(L, -1, "clear");
        switch (lua_type(L, -1)) {
          case LUA_TNIL: break;
          case LUA_TBOOLEAN: canvas.depth.load = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP; break;
          case LUA_TNUMBER: canvas.depth.clear = lua_tonumber(L, -1); break;
          default: lovrThrow("Expected boolean or number for depth clear");
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "clear");
    if (lua_istable(L, -1)) {
      lua_rawgeti(L, -1, 1);
      if (lua_type(L, -1) == LUA_TNUMBER) {
        lua_rawgeti(L, -2, 2);
        lua_rawgeti(L, -3, 3);
        lua_rawgeti(L, -4, 4);
        canvas.clears[0][0] = luax_checkfloat(L, -4);
        canvas.clears[0][1] = luax_checkfloat(L, -3);
        canvas.clears[0][2] = luax_checkfloat(L, -2);
        canvas.clears[0][3] = luax_optfloat(L, -1, 1.f);
        lua_pop(L, 4);
        for (uint32_t i = 1; i < 4; i++) {
          memcpy(canvas.clears[i], canvas.clears[0], 4 * sizeof(float));
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
            canvas.clears[i][0] = luax_checkfloat(L, -4);
            canvas.clears[i][1] = luax_checkfloat(L, -3);
            canvas.clears[i][2] = luax_checkfloat(L, -2);
            canvas.clears[i][3] = luax_optfloat(L, -1, 1.f);
            lua_pop(L, 4);
          } else {
            canvas.loads[i] = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
          }
          lua_pop(L, 1);
        }
      }
    } else if (!lua_isnil(L, -1)) {
      LoadAction load = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
      canvas.loads[0] = canvas.loads[1] = canvas.loads[2] = canvas.loads[3] = load;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "samples");
    canvas.samples = lua_isnil(L, -1) ? canvas.samples : lua_tointeger(L, -1);
    lua_pop(L, 1);
  }

  return canvas;
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
  lua_pushinteger(L, hardware.subgroupSize), lua_setfield(L, -2, "subgroupSize");
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
  lua_pushboolean(L, features.wireframe), lua_setfield(L, -2, "wireframe");
  lua_pushboolean(L, features.depthClamp), lua_setfield(L, -2, "depthClamp");
  lua_pushboolean(L, features.clipDistance), lua_setfield(L, -2, "clipDistance");
  lua_pushboolean(L, features.cullDistance), lua_setfield(L, -2, "cullDistance");
  lua_pushboolean(L, features.fullIndexBufferRange), lua_setfield(L, -2, "fullIndexBufferRange");
  lua_pushboolean(L, features.indirectDrawFirstInstance), lua_setfield(L, -2, "indirectDrawFirstInstance");
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

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.renderSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.renderSize[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.renderSize[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "renderSize");

  lua_pushinteger(L, limits.uniformBufferRange), lua_setfield(L, -2, "uniformBufferRange");
  lua_pushinteger(L, limits.storageBufferRange), lua_setfield(L, -2, "storageBufferRange");
  lua_pushinteger(L, limits.uniformBufferAlign), lua_setfield(L, -2, "uniformBufferAlign");
  lua_pushinteger(L, limits.storageBufferAlign), lua_setfield(L, -2, "storageBufferAlign");
  lua_pushinteger(L, limits.vertexAttributes), lua_setfield(L, -2, "vertexAttributes");
  lua_pushinteger(L, limits.vertexBufferStride), lua_setfield(L, -2, "vertexBufferStride");
  lua_pushinteger(L, limits.vertexShaderOutputs), lua_setfield(L, -2, "vertexShaderOutputs");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.computeDispatchCount[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.computeDispatchCount[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.computeDispatchCount[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "computeDispatchCount");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.computeWorkgroupSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.computeWorkgroupSize[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.computeWorkgroupSize[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "computeWorkgroupSize");

  lua_pushinteger(L, limits.computeWorkgroupVolume), lua_setfield(L, -2, "computeWorkgroupVolume");
  lua_pushinteger(L, limits.computeSharedMemory), lua_setfield(L, -2, "computeSharedMemory");
  lua_pushinteger(L, limits.indirectDrawCount), lua_setfield(L, -2, "indirectDrawCount");
  lua_pushinteger(L, limits.instances), lua_setfield(L, -2, "instances");

  lua_pushnumber(L, limits.anisotropy), lua_setfield(L, -2, "anisotropy");
  lua_pushnumber(L, limits.pointSize), lua_setfield(L, -2, "pointSize");
  return 1;
}

static int l_lovrGraphicsGetStats(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  GraphicsStats stats;
  lovrGraphicsGetStats(&stats);

  lua_createtable(L, 0, 3);
  lua_pushinteger(L, stats.memory), lua_setfield(L, -2, "total");
  lua_pushinteger(L, stats.bufferMemory), lua_setfield(L, -2, "buffer");
  lua_pushinteger(L, stats.textureMemory), lua_setfield(L, -2, "texture");
  lua_setfield(L, -2, "memory");

  lua_createtable(L, 0, 4);
  lua_pushinteger(L, stats.buffers), lua_setfield(L, -2, "buffers");
  lua_pushinteger(L, stats.textures), lua_setfield(L, -2, "textures");
  lua_pushinteger(L, stats.samplers), lua_setfield(L, -2, "samplers");
  lua_pushinteger(L, stats.shaders), lua_setfield(L, -2, "shaders");
  lua_setfield(L, -2, "objects");

  lua_createtable(L, 0, 10);
  lua_pushinteger(L, stats.scratchMemory), lua_setfield(L, -2, "scratchMemory");
  lua_pushinteger(L, stats.renderPasses), lua_setfield(L, -2, "renderPasses");
  lua_pushinteger(L, stats.computePasses), lua_setfield(L, -2, "computePasses");
  lua_pushinteger(L, stats.transferPasses), lua_setfield(L, -2, "transferPasses");
  lua_pushinteger(L, stats.pipelineBinds), lua_setfield(L, -2, "pipelineBinds");
  lua_pushinteger(L, stats.bundleBinds), lua_setfield(L, -2, "bundleBinds");
  lua_pushinteger(L, stats.drawCalls), lua_setfield(L, -2, "drawCalls");
  lua_pushinteger(L, stats.dispatches), lua_setfield(L, -2, "dispatches");
  lua_pushinteger(L, stats.workgroups), lua_setfield(L, -2, "workgroups");
  lua_pushinteger(L, stats.copies), lua_setfield(L, -2, "copies");
  lua_setfield(L, -2, "frame");

  lua_createtable(L, 0, 5);
  lua_pushnumber(L, stats.blocks), lua_setfield(L, -2, "blocks");
  lua_pushnumber(L, stats.canvases), lua_setfield(L, -2, "canvases");
  lua_pushnumber(L, stats.pipelines), lua_setfield(L, -2, "pipelines");
  lua_pushnumber(L, stats.layouts), lua_setfield(L, -2, "layouts");
  lua_pushnumber(L, stats.bunches), lua_setfield(L, -2, "bunches");
  lua_setfield(L, -2, "internal");
  return 1;
}

static int l_lovrGraphicsIsFormatSupported(lua_State* L) {
  TextureFormat format = luax_checkenum(L, 1, TextureFormat, NULL);
  uint32_t features = 0;
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++) {
    features |= 1 << luax_checkenum(L, i, TextureFeature, NULL);
  }
  bool supported = lovrGraphicsIsFormatSupported(format, features);
  lua_pushboolean(L, supported);
  return 1;
}

static int l_lovrGraphicsInit(lua_State* L) {
  bool debug = false;
  bool vsync = false;
  uint32_t blockSize = 1 << 24;
  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "vsync");
    vsync = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "blocksize");
    blockSize = luaL_optinteger(L, -1, blockSize);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (lovrGraphicsInit(debug, vsync, blockSize)) {
    luax_atexit(L, lovrGraphicsDestroy);
  }

  return 0;
}

static int l_lovrGraphicsPrepare(lua_State* L) {
  lovrGraphicsPrepare();
  return 0;
}

static int l_lovrGraphicsBegin(lua_State* L) {
  PassType type = luax_checkenum(L, 1, PassType, NULL);
  switch (type) {
    case PASS_RENDER: lovrGraphicsBeginRender((Canvas[1]) { luax_checkcanvas(L, 2) }, lua_tointeger(L, 3)); return 0;
    case PASS_COMPUTE: lovrGraphicsBeginCompute(lua_tointeger(L, 2)); return 0;
    case PASS_TRANSFER: lovrGraphicsBeginTransfer(lua_tointeger(L, 2)); return 0;
    case PASS_BATCH: lovrGraphicsBeginBatch(luax_checktype(L, 2, Batch)); return 0;
    default: return 0;
  }
}

static int l_lovrGraphicsFinish(lua_State* L) {
  lovrGraphicsFinish();
  return 0;
}

static int l_lovrGraphicsSubmit(lua_State* L) {
  lovrGraphicsSubmit();
  return 0;
}

static int l_lovrGraphicsWait(lua_State* L) {
  lovrGraphicsWait();
  return 0;
}

static int l_lovrGraphicsGetBackgroundColor(lua_State* L) {
  float color[4];
  lovrGraphicsGetBackgroundColor(color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrGraphicsSetBackgroundColor(lua_State* L) {
  float color[4];
  luax_readcolor(L, 1, color);
  lovrGraphicsSetBackgroundColor(color);
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
  if (lua_type(L, 2) == LUA_TSTRING && !strcmp(lua_tostring(L, 2), "orthographic")) {
    float ortho[16];
    float width = luax_checkfloat(L, 3);
    float height = luax_checkfloat(L, 4);
    float near = luax_optfloat(L, 5, -1.f);
    float far = luax_optfloat(L, 6, 1.f);
    mat4_orthographic(ortho, 0.f, width, 0.f, height, near, far);
    lovrGraphicsSetProjection(view, ortho);
  } else if (lua_type(L, 2) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 2);
    float right = luax_checkfloat(L, 3);
    float up = luax_checkfloat(L, 4);
    float down = luax_checkfloat(L, 5);
    float clipNear = luax_optfloat(L, 6, .01f);
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

static int l_lovrGraphicsSetViewport(lua_State* L) {
  float viewport[4], depthRange[2];
  viewport[0] = luax_checkfloat(L, 1);
  viewport[1] = luax_checkfloat(L, 2);
  viewport[2] = luax_checkfloat(L, 3);
  viewport[3] = luax_checkfloat(L, 4);
  depthRange[0] = luax_optfloat(L, 5, 0.f);
  depthRange[1] = luax_optfloat(L, 6, 1.f);
  lovrGraphicsSetViewport(viewport, depthRange);
  return 0;
}

static int l_lovrGraphicsSetScissor(lua_State* L) {
  uint32_t scissor[4];
  scissor[0] = luaL_checkinteger(L, 1);
  scissor[1] = luaL_checkinteger(L, 2);
  scissor[2] = luaL_checkinteger(L, 3);
  scissor[3] = luaL_checkinteger(L, 4);
  lovrGraphicsSetScissor(scissor);
  return 0;
}

static int l_lovrGraphicsPush(lua_State* L) {
  StackType type = luax_checkenum(L, 1, StackType, "transform");
  const char* label = lua_tostring(L, 2);
  lovrGraphicsPush(type, label);
  return 0;
}

static int l_lovrGraphicsPop(lua_State* L) {
  StackType type = luax_checkenum(L, 1, StackType, "transform");
  lovrGraphicsPop(type);
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

static int l_lovrGraphicsSetAlphaToCoverage(lua_State* L) {
  lovrGraphicsSetAlphaToCoverage(lua_toboolean(L, 1));
  return 1;
}

static int l_lovrGraphicsSetBlendMode(lua_State* L) {
  BlendMode mode = lua_isnoneornil(L, 1) ? BLEND_NONE : luax_checkenum(L, 1, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 2, BlendAlphaMode, "alphamultiply");
  lovrGraphicsSetBlendMode(mode, alphaMode);
  return 0;
}

static int l_lovrGraphicsSetColor(lua_State* L) {
  float color[4];
  luax_readcolor(L, 1, color);
  lovrGraphicsSetColor(color);
  return 0;
}

static int l_lovrGraphicsSetColorMask(lua_State* L) {
  bool r = lua_toboolean(L, 1);
  bool g = lua_toboolean(L, 2);
  bool b = lua_toboolean(L, 3);
  bool a = lua_toboolean(L, 4);
  lovrGraphicsSetColorMask(r, g, b, a);
  return 0;
}

static int l_lovrGraphicsSetCullMode(lua_State* L) {
  CullMode mode = luax_checkenum(L, 1, CullMode, "none");
  lovrGraphicsSetCullMode(mode);
  return 0;
}

static int l_lovrGraphicsSetDepthTest(lua_State* L) {
  CompareMode test = lua_isnoneornil(L, 1) ? COMPARE_NONE : luax_checkenum(L, 1, CompareMode, NULL);
  lovrGraphicsSetDepthTest(test);
  return 0;
}

static int l_lovrGraphicsSetDepthWrite(lua_State* L) {
  bool write = lua_toboolean(L, 1);
  lovrGraphicsSetDepthWrite(write);
  return 0;
}

static int l_lovrGraphicsSetDepthNudge(lua_State* L) {
  float nudge = luax_optfloat(L, 1, 0.f);
  float sloped = luax_optfloat(L, 2, 0.f);
  lovrGraphicsSetDepthNudge(nudge, sloped);
  return 0;
}

static int l_lovrGraphicsSetDepthClamp(lua_State* L) {
  bool clamp = lua_toboolean(L, 1);
  lovrGraphicsSetDepthClamp(clamp);
  return 0;
}

static int l_lovrGraphicsSetStencilTest(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrGraphicsSetStencilTest(COMPARE_NONE, 0, 0xff);
  } else {
    CompareMode test = luax_checkenum(L, 1, CompareMode, NULL);
    uint8_t value = luaL_checkinteger(L, 2);
    uint8_t mask = luaL_optinteger(L, 3, 0xff);
    lovrGraphicsSetStencilTest(test, value, mask);
  }
  return 0;
}

static int l_lovrGraphicsSetStencilWrite(lua_State* L) {
  StencilAction actions[3];
  if (lua_isnoneornil(L, 1)) {
    actions[0] = actions[1] = actions[2] = STENCIL_KEEP;
    lovrGraphicsSetStencilWrite(actions, 0, 0xff);
  } else {
    if (lua_istable(L, 1)) {
      lua_rawgeti(L, 1, 1);
      lua_rawgeti(L, 1, 2);
      lua_rawgeti(L, 1, 3);
      actions[0] = luax_checkenum(L, -3, StencilAction, NULL);
      actions[1] = luax_checkenum(L, -2, StencilAction, NULL);
      actions[2] = luax_checkenum(L, -1, StencilAction, NULL);
      lua_pop(L, 3);
    } else {
      actions[0] = actions[1] = actions[2] = luax_checkenum(L, 1, StencilAction, NULL);
    }
    uint8_t value = luaL_optinteger(L, 2, 1);
    uint8_t mask = luaL_optinteger(L, 3, 0xff);
    lovrGraphicsSetStencilWrite(actions, value, mask);
  }
  return 0;
}

static int l_lovrGraphicsSetWinding(lua_State* L) {
  Winding winding = luax_checkenum(L, 1, Winding, NULL);
  lovrGraphicsSetWinding(winding);
  return 0;
}

static int l_lovrGraphicsSetWireframe(lua_State* L) {
  bool wireframe = lua_toboolean(L, 1);
  lovrGraphicsSetWireframe(wireframe);
  return 0;
}

static int l_lovrGraphicsSetShader(lua_State* L) {
  switch (lua_type(L, 1)) {
    case LUA_TNONE:
    case LUA_TNIL:
      lovrGraphicsSetShader(NULL);
      return 0;
    case LUA_TSTRING:
      lovrGraphicsSetShader(lovrGraphicsGetDefaultShader(luax_checkenum(L, 1, DefaultShader, NULL)));
      return 0;
    default:
      lovrGraphicsSetShader(luax_checktype(L, 1, Shader));
      return 0;
  }
}

static int l_lovrGraphicsBind(lua_State* L) {
  const char* name = NULL;
  size_t length = 0;
  uint32_t slot = ~0u;

  switch (lua_type(L, 1)) {
    case LUA_TSTRING: name = lua_tolstring(L, 1, &length); break;
    case LUA_TNUMBER: slot = lua_tointeger(L, 1) - 1; break;
    default: return luax_typeerror(L, 1, "string or number");
  }

  Buffer* buffer = luax_totype(L, 2, Buffer);
  Texture* texture = NULL;
  uint32_t offset = 0;

  if (buffer) {
    offset = lua_tointeger(L, 3);
  } else {
    texture = luax_totype(L, 2, Texture);
    if (!texture) {
      return luax_typeerror(L, 2, "Buffer or Texture");
    }
  }

  lovrGraphicsBind(name, length, slot, buffer, offset, texture);
  return 0;
}

static int l_lovrGraphicsMesh(lua_State* L) {
  int index = 1;
  DrawMode mode = lua_type(L, index) == LUA_TSTRING ? luax_checkenum(L, index++, DrawMode, NULL) : DRAW_TRIANGLES;
  Buffer* vertices = luax_totype(L, index++, Buffer);
  Buffer* indices = luax_totype(L, index, Buffer);
  if (indices) index++;
  float transform[16];
  index = luax_readmat4(L, index, transform, 1);
  Buffer* indirect = luax_totype(L, index, Buffer);
  uint32_t id;
  if (indirect) {
    uint32_t count = luaL_optinteger(L, ++index, 1);
    uint32_t offset = luaL_optinteger(L, ++index, 0);
    id = lovrGraphicsDraw(&(DrawInfo) {
      .mode = mode,
      .vertex.buffer = vertices,
      .index.buffer = indices,
      .count = count,
      .indirect = indirect,
      .offset = offset
    }, transform);
  } else {
    uint32_t start = luaL_optinteger(L, index++, 1) - 1;
    uint32_t limit = lovrBufferGetInfo(indices ? indices : vertices)->length;
    uint32_t count = luaL_optinteger(L, index++, limit);
    uint32_t instances = luaL_optinteger(L, index++, 1);
    id = lovrGraphicsDraw(&(DrawInfo) {
      .mode = mode,
      .vertex.buffer = vertices,
      .index.buffer = indices,
      .start = start,
      .count = count,
      .instances = instances
    }, transform);
  }
  lua_pushinteger(L, id);
  return 1;
}

static uint32_t luax_getvertexcount(lua_State* L, int index) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
      return 0;
    case LUA_TNUMBER:
      return (lua_gettop(L) - index + 1) / 3;
    case LUA_TTABLE:
      lua_rawgeti(L, index, 1);
      int innerType = lua_type(L, -1);
      lua_pop(L, 1);
      return luax_len(L, index) / (innerType == LUA_TNUMBER ? 3 : 1);
    case LUA_TUSERDATA:
      return lua_gettop(L) - index + 1;
    default:
      return luax_typeerror(L, index, "number, table, or vector");
  }
}

static void luax_readvertices(lua_State* L, int index, float* vertices, uint32_t count) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
    default:
      break;
    case LUA_TNUMBER:
      for (uint32_t i = 0; i < 3 * count; i++) {
        *vertices++ = luax_tofloat(L, index + i);
      }
      break;
    case LUA_TTABLE:
      lua_rawgeti(L, index, 1);
      int innerType = lua_type(L, -1);
      lua_pop(L, 1);
      if (innerType == LUA_TNUMBER) {
        for (uint32_t i = 0; i < 3 * count; i++) {
          lua_rawgeti(L, index, i + 1);
          *vertices++ = luax_tofloat(L, -1);
          lua_pop(L, 1);
        }
      } else if (innerType == LUA_TUSERDATA) {
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, index, i + 1);
          vec3_init(vertices, luax_checkvector(L, -1, V_VEC3, NULL));
          lua_pop(L, 1);
          vertices += 3;
        }
      }
      break;
    case LUA_TUSERDATA:
      for (uint32_t i = 0; i < count; i++) {
        vec3_init(vertices, luax_checkvector(L, index + i, V_VEC3, NULL));
        vertices += 3;
      }
      break;
  }
}

static int l_lovrGraphicsPoints(lua_State* L) {
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 1);
  uint32_t id = lovrGraphicsPoints(count, &vertices);
  luax_readvertices(L, 1, vertices, count);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsLine(lua_State* L) {
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 1);
  uint32_t id = lovrGraphicsLine(count, &vertices);
  luax_readvertices(L, 1, vertices, count);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsPlane(lua_State* L) {
  float transform[16];
  int index = luax_readmat4(L, 1, transform, 2);
  uint32_t segments = luaL_optinteger(L, index, 0);
  uint32_t id = lovrGraphicsPlane(transform, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsCube(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 1);
  uint32_t id = lovrGraphicsBox(transform);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsBox(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 3);
  uint32_t id = lovrGraphicsBox(transform);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsCircle(lua_State* L) {
  float transform[16];
  int index = luax_readmat4(L, 1, transform, 1);
  uint32_t segments = luaL_optinteger(L, index, 32);
  uint32_t id = lovrGraphicsCircle(transform, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsCylinder(lua_State* L) {
  float transform[16];
  int index = luax_readmat4(L, 1, transform, 1);
  float r1 = luax_optfloat(L, index++, 1.f);
  float r2 = luax_optfloat(L, index++, 1.f);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  uint32_t segments = luaL_optinteger(L, index, 32);
  uint32_t id = lovrGraphicsCylinder(transform, r1, r2, capped, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsSphere(lua_State* L) {
  float transform[16];
  int index = luax_readmat4(L, 1, transform, 1);
  uint32_t segments = luaL_optinteger(L, index, 32);
  uint32_t id = lovrGraphicsSphere(transform, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsSkybox(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  uint32_t id = lovrGraphicsSkybox(texture);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsFill(lua_State* L) {
  Texture* texture = luax_totype(L, 1, Texture);
  uint32_t id = lovrGraphicsFill(texture);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsModel(lua_State* L) {
  float transform[16];
  Model* model = luax_checktype(L, 1, Model);
  int index = luax_readmat4(L, 2, transform, 1);
  uint32_t node = luaL_optinteger(L, index++, ~0u); // TODO string
  bool children = lua_isnil(L, index) ? (index++, true) : lua_toboolean(L, index++);
  uint32_t instances = luaL_optinteger(L, index++, 1);
  uint32_t id = lovrGraphicsModel(model, transform, node, children, instances);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsPrint(lua_State* L) {
  size_t length;
  float transform[16];
  Font* font = luax_checktype(L, 1, Font);
  const char* text = luaL_checklstring(L, 2, &length);
  int index = luax_readmat4(L, 3, transform, 1);
  float wrap = luax_optfloat(L, index++, 0.f);
  HorizontalAlign halign = luax_checkenum(L, index++, HorizontalAlign, "center");
  VerticalAlign valign = luax_checkenum(L, index++, VerticalAlign, "middle");
  uint32_t id = lovrGraphicsPrint(font, text, length, transform, wrap, halign, valign);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrGraphicsReplay(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrGraphicsReplay(batch);
  return 0;
}

static int l_lovrGraphicsCompute(lua_State* L) {
  Buffer* buffer = luax_totype(L, 1, Buffer);
  if (buffer) {
    uint32_t offset = lua_tointeger(L, 2);
    lovrGraphicsCompute(0, 0, 0, buffer, offset);
  } else {
    uint32_t x = luaL_optinteger(L, 1, 1);
    uint32_t y = luaL_optinteger(L, 2, 1);
    uint32_t z = luaL_optinteger(L, 3, 1);
    lovrGraphicsCompute(x, y, z, NULL, 0);
  }
  return 0;
}

static int l_lovrGraphicsGetBuffer(lua_State* L) {
  BufferInfo info = { .usage = BUFFER_VERTEX | BUFFER_INDEX | BUFFER_UNIFORM | BUFFER_COPYFROM };

  // Format
  luax_checkbufferformat(L, 2, &info);
  info.stride = info.stride > 1 ? ALIGN(info.stride, 16) : info.stride;

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
  BufferInfo info = { .usage = ~0u };

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
  }

  // Format
  luax_checkbufferformat(L, 2, &info);

  // std140 (only needed for uniform buffers, also as special case 'byte' formats skip this)
  if (info.usage & BUFFER_UNIFORM && info.stride > 1) {
    info.stride = ALIGN(info.stride, 16);
  }

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
  ShaderInfo info = { 0 };
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

  ShaderFlag flags[32];
  info.flags = flags;

  int index = table ? 2 : 3;
  if (table || lua_istable(L, index)) {
    lua_getfield(L, index, "flags");
    luaL_checktype(L, -1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      lovrCheck(info.flagCount < COUNTOF(flags), "Too many shader flags (max is %d), please report this encounter", COUNTOF(flags));
      double value = lua_isboolean(L, -1) ? (double) lua_toboolean(L, -1) : lua_tonumber(L, -1);
      switch (lua_type(L, -2)) {
        case LUA_TSTRING:
          flags[info.flagCount++] = (ShaderFlag) { .name = lua_tostring(L, -2), .value = value };
          break;
        case LUA_TNUMBER:
          flags[info.flagCount++] = (ShaderFlag) { .id = lua_tointeger(L, -2), .value = value };
          break;
        default: lovrThrow("Unexpected ShaderFlag key type (%s)", lua_typename(L, lua_type(L, -2)));
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  Shader* shader = lovrShaderCreate(&info);
  luax_pushtype(L, Shader, shader);
  lovrRelease(shader, lovrShaderDestroy);
  lovrRelease(blobs[0], lovrBlobDestroy);
  lovrRelease(blobs[1], lovrBlobDestroy);
  return 1;
}

static int l_lovrGraphicsNewBatch(lua_State* L) {
  BatchInfo info;
  Canvas canvas = luax_checkcanvas(L, 1);
  info.canvas = &canvas;
  info.capacity = lua_tointeger(L, 2);
  info.bufferSize = luaL_optinteger(L, 3, 1 << 20);
  Batch* batch = lovrBatchCreate(&info);
  luax_pushtype(L, Batch, batch);
  lovrRelease(batch, lovrBatchDestroy);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {
  { "getHardware", l_lovrGraphicsGetHardware },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "getStats", l_lovrGraphicsGetStats },
  { "isFormatSupported", l_lovrGraphicsIsFormatSupported },

  { "init", l_lovrGraphicsInit },
  { "prepare", l_lovrGraphicsPrepare },
  { "begin", l_lovrGraphicsBegin },
  { "finish", l_lovrGraphicsFinish },
  { "submit", l_lovrGraphicsSubmit },
  { "wait", l_lovrGraphicsWait },

  { "getBackgroundColor", l_lovrGraphicsGetBackgroundColor },
  { "setBackgroundColor", l_lovrGraphicsSetBackgroundColor },
  { "getViewPose", l_lovrGraphicsGetViewPose },
  { "setViewPose", l_lovrGraphicsSetViewPose },
  { "getProjection", l_lovrGraphicsGetProjection },
  { "setProjection", l_lovrGraphicsSetProjection },
  { "setViewport", l_lovrGraphicsSetViewport },
  { "setScissor", l_lovrGraphicsSetScissor },

  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
  { "transform", l_lovrGraphicsTransform },

  { "setAlphaToCoverage", l_lovrGraphicsSetAlphaToCoverage },
  { "setBlendMode", l_lovrGraphicsSetBlendMode },
  { "setColor", l_lovrGraphicsSetColor },
  { "setColorMask", l_lovrGraphicsSetColorMask },
  { "setCullMode", l_lovrGraphicsSetCullMode },
  { "setDepthTest", l_lovrGraphicsSetDepthTest },
  { "setDepthWrite", l_lovrGraphicsSetDepthWrite },
  { "setDepthNudge", l_lovrGraphicsSetDepthNudge },
  { "setDepthClamp", l_lovrGraphicsSetDepthClamp },
  { "setStencilTest", l_lovrGraphicsSetStencilTest },
  { "setStencilWrite", l_lovrGraphicsSetStencilWrite },
  { "setWinding", l_lovrGraphicsSetWinding },
  { "setWireframe", l_lovrGraphicsSetWireframe },

  { "setShader", l_lovrGraphicsSetShader },
  { "bind", l_lovrGraphicsBind },

  { "mesh", l_lovrGraphicsMesh },
  { "points", l_lovrGraphicsPoints },
  { "line", l_lovrGraphicsLine },
  { "plane", l_lovrGraphicsPlane },
  { "cube", l_lovrGraphicsCube },
  { "box", l_lovrGraphicsBox },
  { "circle", l_lovrGraphicsCircle },
  { "cylinder", l_lovrGraphicsCylinder },
  { "sphere", l_lovrGraphicsSphere },
  { "skybox", l_lovrGraphicsSkybox },
  { "fill", l_lovrGraphicsFill },
  { "model", l_lovrGraphicsModel },
  { "print", l_lovrGraphicsPrint },
  { "replay", l_lovrGraphicsReplay },

  { "compute", l_lovrGraphicsCompute },

  { "getBuffer", l_lovrGraphicsGetBuffer },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newTexture", l_lovrGraphicsNewTexture },
  { "newSampler", l_lovrGraphicsNewSampler },
  { "newShader", l_lovrGraphicsNewShader },
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
