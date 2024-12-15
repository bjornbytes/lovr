#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "util.h"
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
  [BLEND_NONE] = ENTRY("none"),
  { 0 }
};

StringEntry lovrBufferLayout[] = {
  [LAYOUT_PACKED] = ENTRY("packed"),
  [LAYOUT_STD140] = ENTRY("std140"),
  [LAYOUT_STD430] = ENTRY("std430"),
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

StringEntry lovrDefaultShader[] = {
  [SHADER_UNLIT] = ENTRY("unlit"),
  [SHADER_NORMAL] = ENTRY("normal"),
  [SHADER_FONT] = ENTRY("font"),
  [SHADER_CUBEMAP] = ENTRY("cubemap"),
  [SHADER_EQUIRECT] = ENTRY("equirect"),
  [SHADER_FILL_2D] = ENTRY("fill"),
  [SHADER_FILL_ARRAY] = ENTRY("fillarray"),
  { 0 }
};

StringEntry lovrDrawMode[] = {
  [DRAW_POINTS] = ENTRY("points"),
  [DRAW_LINES] = ENTRY("lines"),
  [DRAW_TRIANGLES] = ENTRY("triangles"),
  { 0 }
};

StringEntry lovrDrawStyle[] = {
  [STYLE_FILL] = ENTRY("fill"),
  [STYLE_LINE] = ENTRY("line"),
  { 0 }
};

StringEntry lovrDataType[] = {
  [TYPE_I8x4] = ENTRY("i8x4"),
  [TYPE_U8x4] = ENTRY("u8x4"),
  [TYPE_SN8x4] = ENTRY("sn8x4"),
  [TYPE_UN8x4] = ENTRY("un8x4"),
  [TYPE_SN10x3] = ENTRY("sn10x3"),
  [TYPE_UN10x3] = ENTRY("un10x3"),
  [TYPE_I16] = ENTRY("i16"),
  [TYPE_I16x2] = ENTRY("i16x2"),
  [TYPE_I16x4] = ENTRY("i16x4"),
  [TYPE_U16] = ENTRY("u16"),
  [TYPE_U16x2] = ENTRY("u16x2"),
  [TYPE_U16x4] = ENTRY("u16x4"),
  [TYPE_SN16x2] = ENTRY("sn16x2"),
  [TYPE_SN16x4] = ENTRY("sn16x4"),
  [TYPE_UN16x2] = ENTRY("un16x2"),
  [TYPE_UN16x4] = ENTRY("un16x4"),
  [TYPE_I32] = ENTRY("i32"),
  [TYPE_I32x2] = ENTRY("i32x2"),
  [TYPE_I32x3] = ENTRY("i32x3"),
  [TYPE_I32x4] = ENTRY("i32x4"),
  [TYPE_U32] = ENTRY("u32"),
  [TYPE_U32x2] = ENTRY("u32x2"),
  [TYPE_U32x3] = ENTRY("u32x3"),
  [TYPE_U32x4] = ENTRY("u32x4"),
  [TYPE_F16x2] = ENTRY("f16x2"),
  [TYPE_F16x4] = ENTRY("f16x4"),
  [TYPE_F32] = ENTRY("f32"),
  [TYPE_F32x2] = ENTRY("f32x2"),
  [TYPE_F32x3] = ENTRY("f32x3"),
  [TYPE_F32x4] = ENTRY("f32x4"),
  [TYPE_MAT2] = ENTRY("mat2"),
  [TYPE_MAT3] = ENTRY("mat3"),
  [TYPE_MAT4] = ENTRY("mat4"),
  [TYPE_INDEX16] = ENTRY("index16"),
  [TYPE_INDEX32] = ENTRY("index32"),
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

StringEntry lovrMeshStorage[] = {
  [MESH_CPU] = ENTRY("cpu"),
  [MESH_GPU] = ENTRY("gpu"),
  { 0 }
};

StringEntry lovrOriginType[] = {
  [ORIGIN_ROOT] = ENTRY("root"),
  [ORIGIN_PARENT] = ENTRY("parent"),
  { 0 }
};

StringEntry lovrShaderStage[] = {
  [STAGE_VERTEX] = ENTRY("vertex"),
  [STAGE_FRAGMENT] = ENTRY("pixel"),
  [STAGE_COMPUTE] = ENTRY("compute"),
  { 0 }
};

StringEntry lovrShaderType[] = {
  [SHADER_GRAPHICS] = ENTRY("graphics"),
  [SHADER_COMPUTE] = ENTRY("compute"),
  { 0 }
};

StringEntry lovrStackType[] = {
  [STACK_TRANSFORM] = ENTRY("transform"),
  [STACK_STATE] = ENTRY("state"),
  { 0 }
};

StringEntry lovrStencilAction[] = {
  [STENCIL_KEEP] = ENTRY("keep"),
  [STENCIL_ZERO] = ENTRY("zero"),
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
  [1] = ENTRY("render"),
  [2] = ENTRY("storage"),
  [3] = ENTRY("blit"),
  { 0 }
};

StringEntry lovrTextureType[] = {
  [TEXTURE_2D] = ENTRY("2d"),
  [TEXTURE_3D] = ENTRY("3d"),
  [TEXTURE_CUBE] = ENTRY("cube"),
  [TEXTURE_ARRAY] = ENTRY("array"),
  { 0 }
};

StringEntry lovrTextureUsage[] = {
  [0] = ENTRY("sample"),
  [1] = ENTRY("render"),
  [2] = ENTRY("storage"),
  [3] = ENTRY("transfer"),
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
  [WRAP_BORDER] = ENTRY("border"),
  { 0 }
};

static uint32_t luax_checkdatatype(lua_State* L, int index) {
  size_t length;
  const char* string = luaL_checklstring(L, index, &length);

  if (length < 3) {
    luaL_error(L, "invalid DataType '%s'", string);
    return 0;
  }

  // Deprecated: plurals are allowed and ignored
  if (string[length - 1] == 's') {
    length--;
  }

  // x1 is allowed and ignored
  if (string[length - 2] == 'x' && string[length - 1] == '1') {
    length -= 2;
  }

  // vec[234]
  if (length == 4 && string[0] == 'v' && string[1] == 'e' && string[2] == 'c' && string[3] >= '2' && string[3] <= '4') {
    return TYPE_F32x2 + string[3] - '2';
  }

  if (length == 3 && !memcmp(string, "int", length)) {
    return TYPE_I32;
  }

  if (length == 4 && !memcmp(string, "uint", length)) {
    return TYPE_U32;
  }

  if (length == 5 && !memcmp(string, "float", length)) {
    return TYPE_F32;
  }

  if (length == 5 && !memcmp(string, "color", length)) {
    return TYPE_UN8x4;
  }

  if (length == 5 && !memcmp(string, "index", length)) {
    return TYPE_INDEX32;
  }

  for (int i = 0; lovrDataType[i].length; i++) {
    if (length == lovrDataType[i].length && !memcmp(string, lovrDataType[i].string, length)) {
      return i;
    }
  }

  luaL_error(L, "invalid DataType '%s'", string);
  return 0;
}

uint32_t luax_checkcomparemode(lua_State* L, int index) {
  size_t length;
  const char* string = lua_tolstring(L, index, &length);

  if (string && length <= 2) {
    if (string[0] == '=' && (length == 1 || string[1] == '=')) {
      return COMPARE_EQUAL;
    }

    if ((string[0] == '~' || string[0] == '!') && string[1] == '=') {
      return COMPARE_NEQUAL;
    }

    if (string[0] == '<' && length == 1) {
      return COMPARE_LESS;
    }

    if (string[0] == '<' && string[1] == '=') {
      return COMPARE_LEQUAL;
    }

    if (string[0] == '>' && length == 1) {
      return COMPARE_GREATER;
    }

    if (string[0] == '>' && string[1] == '=') {
      return COMPARE_GEQUAL;
    }
  }

  return luax_checkenum(L, index, CompareMode, "none");
}

static void luax_writeshadercache(void) {
  size_t size;
  lovrGraphicsGetShaderCache(NULL, &size);

  if (size == 0) {
    return;
  }

  void* data = lovrMalloc(size);
  lovrGraphicsGetShaderCache(data, &size);

  if (size > 0) {
    luax_writefile(".lovrshadercache", data, size);
  }

  lovrFree(data);
}

static int l_lovrGraphicsInitialize(lua_State* L) {
  GraphicsConfig config = {
    .debug = false,
    .vsync = false,
    .stencil = false,
    .antialias = true
  };

  bool shaderCache = true;

  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    config.debug = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "vsync");
    config.vsync = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "stencil");
    config.stencil = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "antialias");
    config.antialias = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "shadercache");
    shaderCache = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (shaderCache) {
    config.cacheData = luax_readfile(".lovrshadercache", &config.cacheSize);
  }

  bool success = lovrGraphicsInit(&config);
  lovrFree(config.cacheData);
  luax_assert(L, success);
  luax_atexit(L, lovrGraphicsDestroy);

  if (shaderCache) { // Finalizers run in the opposite order they were added, so this has to go last
    luax_atexit(L, luax_writeshadercache);
  }

  return 0;
}

static int l_lovrGraphicsIsInitialized(lua_State* L) {
  bool initialized = lovrGraphicsIsInitialized();
  lua_pushboolean(L, initialized);
  return 1;
}

static int l_lovrGraphicsIsTimingEnabled(lua_State* L) {
  bool enabled = lovrGraphicsIsTimingEnabled();
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrGraphicsSetTimingEnabled(lua_State* L) {
  bool enable = lua_toboolean(L, 1);
  lovrGraphicsSetTimingEnabled(enable);
  return 0;
}

static int l_lovrGraphicsSubmit(lua_State* L) {
  bool table = lua_istable(L, 1);
  int length = table ? luax_len(L, 1) : lua_gettop(L);
  uint32_t count = 0;

  Pass* stack[8];
  Pass** passes = (size_t) length > COUNTOF(stack) ? lovrMalloc(length * sizeof(Pass*)) : stack;

  if (table) {
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, 1, i + 1);
      if (lua_toboolean(L, -1)) {
        passes[count++] = luax_checktype(L, -1, Pass);
      }
      lua_pop(L, 1);
    }
  } else {
    for (int i = 0; i < length; i++) {
      if (lua_toboolean(L, i + 1)) {
        passes[count++] = luax_checktype(L, i + 1, Pass);
      }
    }
  }

  bool success = lovrGraphicsSubmit(passes, count);
  if (passes != stack) lovrFree(passes);
  luax_assert(L, success);
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrGraphicsPresent(lua_State* L) {
  luax_assert(L, lovrGraphicsPresent());
  return 0;
}

static int l_lovrGraphicsWait(lua_State* L) {
  luax_assert(L, lovrGraphicsWait());
  return 0;
}

static int l_lovrGraphicsGetDevice(lua_State* L) {
  GraphicsDevice device;
  lovrGraphicsGetDevice(&device);
  lua_newtable(L);
  lua_pushinteger(L, device.deviceId), lua_setfield(L, -2, "id");
  lua_pushinteger(L, device.vendorId), lua_setfield(L, -2, "vendor");
  lua_pushstring(L, device.name), lua_setfield(L, -2, "name");
  lua_pushstring(L, device.renderer), lua_setfield(L, -2, "renderer");
  lua_pushinteger(L, device.subgroupSize), lua_setfield(L, -2, "subgroupSize");
  lua_pushboolean(L, device.discrete), lua_setfield(L, -2, "discrete");
  return 1;
}

static int l_lovrGraphicsGetFeatures(lua_State* L) {
  GraphicsFeatures features;
  lovrGraphicsGetFeatures(&features);
  lua_newtable(L);
  lua_pushboolean(L, features.textureBC), lua_setfield(L, -2, "textureBC");
  lua_pushboolean(L, features.textureASTC), lua_setfield(L, -2, "textureASTC");
  lua_pushboolean(L, features.wireframe), lua_setfield(L, -2, "wireframe");
  lua_pushboolean(L, features.depthClamp), lua_setfield(L, -2, "depthClamp");
  lua_pushboolean(L, features.depthResolve), lua_setfield(L, -2, "depthResolve");
  lua_pushboolean(L, features.indirectDrawFirstInstance), lua_setfield(L, -2, "indirectDrawFirstInstance");
  lua_pushboolean(L, features.packedBuffers), lua_setfield(L, -2, "packedBuffers");
  lua_pushboolean(L, features.float64), lua_setfield(L, -2, "float64");
  lua_pushboolean(L, features.int64), lua_setfield(L, -2, "int64");
  lua_pushboolean(L, features.int16), lua_setfield(L, -2, "int16");
  return 1;
}

static int l_lovrGraphicsGetLimits(lua_State* L) {
  GraphicsLimits limits;
  lovrGraphicsGetLimits(&limits);

  lua_newtable(L);
  lua_pushinteger(L, limits.textureSize2D), lua_setfield(L, -2, "textureSize2D");
  lua_pushinteger(L, limits.textureSize3D), lua_setfield(L, -2, "textureSize3D");
  lua_pushinteger(L, limits.textureSizeCube), lua_setfield(L, -2, "textureSizeCube");
  lua_pushinteger(L, limits.textureLayers), lua_setfield(L, -2, "textureLayers");
  lua_pushinteger(L, limits.textureSamples), lua_setfield(L, -2, "textureSamples");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.renderSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.renderSize[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.renderSize[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "renderSize");

  lua_pushinteger(L, limits.uniformBuffersPerStage), lua_setfield(L, -2, "uniformBuffersPerStage");
  lua_pushinteger(L, limits.storageBuffersPerStage), lua_setfield(L, -2, "storageBuffersPerStage");
  lua_pushinteger(L, limits.sampledTexturesPerStage), lua_setfield(L, -2, "sampledTexturesPerStage");
  lua_pushinteger(L, limits.storageTexturesPerStage), lua_setfield(L, -2, "storageTexturesPerStage");
  lua_pushinteger(L, limits.samplersPerStage), lua_setfield(L, -2, "samplersPerStage");
  lua_pushinteger(L, limits.resourcesPerShader), lua_setfield(L, -2, "resourcesPerShader");

  lua_pushinteger(L, limits.uniformBufferRange), lua_setfield(L, -2, "uniformBufferRange");
  lua_pushinteger(L, limits.storageBufferRange), lua_setfield(L, -2, "storageBufferRange");
  lua_pushinteger(L, limits.uniformBufferAlign), lua_setfield(L, -2, "uniformBufferAlign");
  lua_pushinteger(L, limits.storageBufferAlign), lua_setfield(L, -2, "storageBufferAlign");
  lua_pushinteger(L, limits.vertexAttributes), lua_setfield(L, -2, "vertexAttributes");
  lua_pushinteger(L, limits.vertexBufferStride), lua_setfield(L, -2, "vertexBufferStride");
  lua_pushinteger(L, limits.vertexShaderOutputs), lua_setfield(L, -2, "vertexShaderOutputs");

  lua_pushinteger(L, limits.clipDistances), lua_setfield(L, -2, "clipDistances");
  lua_pushinteger(L, limits.cullDistances), lua_setfield(L, -2, "cullDistances");
  lua_pushinteger(L, limits.clipAndCullDistances), lua_setfield(L, -2, "clipAndCullDistances");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.workgroupCount[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.workgroupCount[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.workgroupCount[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "workgroupCount");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.workgroupSize[0]), lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.workgroupSize[1]), lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.workgroupSize[2]), lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "workgroupSize");

  lua_pushinteger(L, limits.totalWorkgroupSize), lua_setfield(L, -2, "totalWorkgroupSize");
  lua_pushinteger(L, limits.computeSharedMemory), lua_setfield(L, -2, "computeSharedMemory");
  lua_pushinteger(L, limits.indirectDrawCount), lua_setfield(L, -2, "indirectDrawCount");
  lua_pushinteger(L, limits.instances), lua_setfield(L, -2, "instances");

  lua_pushnumber(L, limits.anisotropy), lua_setfield(L, -2, "anisotropy");
  lua_pushnumber(L, limits.pointSize), lua_setfield(L, -2, "pointSize");
  return 1;
}

static int l_lovrGraphicsIsFormatSupported(lua_State* L) {
  TextureFormat format = luax_checkenum(L, 1, TextureFormat, NULL);
  uint32_t features = 0;
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++) {
    features |= 1 << luax_checkenum(L, i, TextureFeature, NULL);
  }
  uint32_t support = lovrGraphicsGetFormatSupport(format, features);
  lua_pushboolean(L, support & (1 << 0)); // linear
  lua_pushboolean(L, support & (1 << 1)); // srgb
  return 2;
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

static int l_lovrGraphicsGetWindowPass(lua_State* L) {
  Pass* pass = NULL;
  luax_assert(L, lovrGraphicsGetWindowPass(&pass));
  luax_pushtype(L, Pass, pass);
  return 1;
}

static int l_lovrGraphicsGetDefaultFont(lua_State* L) {
  Font* font = lovrGraphicsGetDefaultFont();
  luax_assert(L, font);
  luax_pushtype(L, Font, font);
  return 1;
}

static uint32_t luax_checkbufferformat(lua_State* L, int index, DataField* fields, uint32_t* count, uint32_t max) {
  luax_check(L, lua_istable(L, index), "Expected a table for field list");

  uint32_t length = luax_len(L, index);
  luax_check(L, length > 0, "At least one field must be provided");
  luax_check(L, *count + length <= max, "Too many buffer fields (maybe format contains a cycle?)");
  memset(fields + *count, 0, length * sizeof(DataField));
  DataField* field = fields + *count;
  *count += length;

  for (uint32_t i = 0; i < length; i++, field++) {
    lua_rawgeti(L, index, i + 1);
    luax_check(L, lua_istable(L, -1), "Expected table for type info");

    lua_getfield(L, -1, "type");
    if (lua_isnil(L, -1)) lua_pop(L, 1), lua_rawgeti(L, -1, 2);
    if (lua_istable(L, -1)) {
      field->fields = fields + *count;
      field->fieldCount = luax_checkbufferformat(L, -1, fields, count, max);
    } else if (lua_type(L, -1) == LUA_TSTRING) {
      field->type = luax_checkdatatype(L, -1);
    } else {
      luaL_error(L, "Buffer field type must be a string or a table");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "name");
    if (lua_isnil(L, -1)) lua_pop(L, 1), lua_rawgeti(L, -1, 1);
    luax_check(L, lua_type(L, -1) == LUA_TSTRING, "Buffer fields must have a 'name' key");
    field->name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "length");
    if (lua_isnil(L, -1)) lua_pop(L, 1), lua_rawgeti(L, -1, 3);
    if (!lua_isnil(L, -1)) {
      luax_check(L, lua_type(L, -1) == LUA_TNUMBER, "Field lengths must be numbers");
      double number = lua_tonumber(L, -1);
      if (number < 0.) {
        // If it's a dynamic array, it must be the last field at the top level
        luax_check(L, *count == length + 1 && i == length - 1, "Dynamic arrays must be the last field, and can not be nested");
        field->length = ~0u;
      } else {
        field->length = (uint32_t) number;
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "offset");
    field->offset = lua_isnil(L, -1) ? 0 : luax_checku32(L, -1);
    lua_pop(L, 1);

    lua_pop(L, 1);
  }

  return length;
}

static int l_lovrGraphicsNewBuffer(lua_State* L) {
  DataField format[64];
  BufferInfo info = { 0 };
  DataLayout layout = LAYOUT_PACKED;
  bool hasData = false;
  Blob* blob = NULL;

  int type = lua_type(L, 1);

  // Format
  if (type == LUA_TNUMBER) {
    info.size = luax_checku32(L, 1);
  } else if ((blob = luax_totype(L, 1, Blob)) != NULL) {
    luax_check(L, blob->size < UINT32_MAX, "Blob is too big to create a Buffer");
    info.size = (uint32_t) blob->size;
  } else if (type == LUA_TSTRING) {
    info.fieldCount = 1;
    info.format = format;
    format[0] = (DataField) { .type = luax_checkdatatype(L, 1) };
  } else if (type == LUA_TTABLE) {
    info.format = format;
    format[0] = (DataField) { .fieldCount = luax_len(L, 1), .fields = format + 1 };

    lua_rawgeti(L, 1, 1);
    bool anonymous = lua_type(L, -1) == LUA_TSTRING;
    lua_pop(L, 1);

    if (anonymous) {
      luax_check(L, format->fieldCount < COUNTOF(format), "Too many buffer fields");
      info.fieldCount = format->fieldCount + 1;

      for (uint32_t i = 1; i <= format->fieldCount; i++) {
        lua_rawgeti(L, 1, i);
        format[i] = (DataField) { .type = luax_checkdatatype(L, -1) };
        lua_pop(L, 1);
      }

      // Convert single-field anonymous formats to regular arrays
      if (format->fieldCount == 1) {
        format->type = format[1].type;
        format->fieldCount = 0;
      }
    } else {
      info.fieldCount = 1;
      luax_checkbufferformat(L, 1, format, &info.fieldCount, COUNTOF(format));
    }

    lua_getfield(L, 1, "layout");
    layout = luax_checkenum(L, -1, BufferLayout, "packed");
    lua_pop(L, 1);

    lua_getfield(L, 1, "stride");
    format->stride = luax_optu32(L, -1, 0);
    lua_pop(L, 1);
  } else {
    return luax_typeerror(L, 1, "number, Blob, table, or string");
  }

  // Length/size
  if (info.format) {
    lovrGraphicsAlignFields(format, layout);

    // Dynamic arrays
    if (format->fieldCount > 0 && format->fields[format->fieldCount - 1].length == ~0u) {
      DataField* array = &format->fields[format->fieldCount - 1];

      switch (lua_type(L, 2)) {
        case LUA_TNIL: case LUA_TNONE: array->length = 1; break;
        case LUA_TNUMBER: array->length = luax_checku32(L, 2); break;
        case LUA_TTABLE:
          lua_getfield(L, 2, array->name);
          array->length = luax_len(L, -1);
          hasData = true;
          lua_pop(L, 1);
          break;
        default:
          if ((blob = luax_totype(L, 2, Blob)) != NULL) {
            if (blob->size < array->offset) {
              array->length = 1;
            } else {
              array->length = (uint32_t) ((blob->size - array->offset) / array->stride);
            }

            hasData = true;
          } else {
            return luax_typeerror(L, 2, "nil, number, table, or Blob");
          }
          break;
      }

      format->length = 0;
      format->stride = array->offset + array->stride * array->length;
    } else {
      switch (lua_type(L, 2)) {
        case LUA_TNIL: case LUA_TNONE: format->length = 0; break;
        case LUA_TNUMBER: format->length = luax_checku32(L, 2); break;
        case LUA_TTABLE:
          lua_rawgeti(L, 2, 1);
          if (lua_type(L, -1) == LUA_TNUMBER) {
            luax_check(L, format->fieldCount <= 1, "Struct data must be provided as a table of tables");
            DataType type = format->fieldCount == 0 ? format->type : format->fields[0].type;
            format->length = luax_len(L, -2) / luax_gettablestride(L, type);
          } else {
            format->length = luax_len(L, -2);
          }
          lua_pop(L, 1);
          hasData = true;
          break;
        default:
          if ((blob = luax_totype(L, 2, Blob)) != NULL) {
            luax_check(L, blob->size < UINT32_MAX, "Blob is too big to create a Buffer (max size is 1GB)");
            info.size = (uint32_t) blob->size;
            format->length = info.size / format->stride;
            break;
          } else if (luax_tovector(L, 2, NULL)) {
            format->length = 0;
            hasData = true;
            break;
          }
          return luax_typeerror(L, 2, "nil, number, vector, table, or Blob");
      }
    }
  }

  void* data;
  Buffer* buffer = lovrBufferCreate(&info, (blob || hasData) ? &data : NULL);

  // Write data
  if (blob) {
    memcpy(data, blob->data, info.size);
  } else if (hasData) {
    luax_checkbufferdata(L, 2, format, data);
  }

  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewTexture(lua_State* L) {
  TextureInfo info = {
    .type = TEXTURE_2D,
    .format = FORMAT_RGBA8,
    .layers = 1,
    .mipmaps = ~0u,
    .samples = 1,
    .usage = TEXTURE_SAMPLE,
    .srgb = true
  };

  int index = 1;
  Image* stack[6];
  Image** images = stack;

  if (lua_isnumber(L, 1)) {
    info.width = luax_checku32(L, index++);
    info.height = luax_checku32(L, index++);
    if (lua_isnumber(L, index)) {
      info.layers = luax_checku32(L, index++);
      info.type = TEXTURE_ARRAY;
    }
    info.usage |= TEXTURE_RENDER;
    info.mipmaps = 1;
  } else if (lua_istable(L, 1)) {
    info.imageCount = luax_len(L, index++);
    images = info.imageCount > COUNTOF(stack) ? lovrMalloc(info.imageCount * sizeof(Image*)) : stack;

    if (info.imageCount == 0) {
      info.layers = 6;
      info.imageCount = 6;
      info.type = TEXTURE_CUBE;
      const char* faces[6] = { "right", "left", "top", "bottom", "back", "front" };
      const char* altFaces[6] = { "px", "nx", "py", "ny", "pz", "nz" };
      for (int i = 0; i < 6; i++) {
        lua_pushstring(L, faces[i]);
        lua_rawget(L, 1);
        if (lua_isnil(L, -1)) {
          lua_pop(L, 1);
          lua_pushstring(L, altFaces[i]);
          lua_rawget(L, 1);
        }
        luax_check(L, !lua_isnil(L, -1), "No array texture layers given and cubemap face '%s' missing", faces[i]);
        images[i] = luax_checkimage(L, -1);
      }
    } else {
      for (uint32_t i = 0; i < info.imageCount; i++) {
        lua_rawgeti(L, 1, (int) i + 1);
        images[i] = luax_checkimage(L, -1);
        lua_pop(L, 1);
      }

      info.type = info.imageCount == 6 ? TEXTURE_CUBE : TEXTURE_ARRAY;
      info.layers = info.imageCount == 1 ? lovrImageGetLayerCount(images[0]) : info.imageCount;
    }
  } else {
    info.imageCount = 1;
    info.images = images;
    images[0] = luax_checkimage(L, index++);
    info.layers = lovrImageGetLayerCount(images[0]);
    if (lovrImageIsCube(images[0])) {
      info.type = TEXTURE_CUBE;
    } else if (info.layers > 1) {
      info.type = TEXTURE_ARRAY;
    }
  }

  if (info.imageCount > 0) {
    info.images = images;
    Image* image = images[0];
    uint32_t levels = lovrImageGetLevelCount(image);
    info.format = lovrImageGetFormat(image);
    info.srgb = lovrImageIsSRGB(image);
    info.width = lovrImageGetWidth(image, 0);
    info.height = lovrImageGetHeight(image, 0);
    bool mipmappable = lovrGraphicsGetFormatSupport(info.format, TEXTURE_FEATURE_BLIT) & (1 << info.srgb);
    info.mipmaps = (levels == 1 && mipmappable) ? ~0u : levels;
    for (uint32_t i = 1; i < info.imageCount; i++) {
      luax_check(L, lovrImageGetWidth(images[0], 0) == lovrImageGetWidth(images[i], 0), "Image widths must match");
      luax_check(L, lovrImageGetHeight(images[0], 0) == lovrImageGetHeight(images[i], 0), "Image heights must match");
      luax_check(L, lovrImageGetFormat(images[0]) == lovrImageGetFormat(images[i]), "Image formats must match");
      luax_check(L, lovrImageGetLevelCount(images[0]) == lovrImageGetLevelCount(images[i]), "Image mipmap counts must match");
      luax_check(L, lovrImageGetLayerCount(images[i]) == 1, "When a list of images are provided, each must have a single layer");
    }
  }

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "type");
    info.type = lua_isnil(L, -1) ? info.type : (uint32_t) luax_checkenum(L, -1, TextureType, NULL);
    lua_pop(L, 1);

    if (info.imageCount == 0) {
      lua_getfield(L, index, "format");
      info.format = lua_isnil(L, -1) ? info.format : (uint32_t) luax_checkenum(L, -1, TextureFormat, NULL);
      lua_pop(L, 1);

      lua_getfield(L, index, "samples");
      info.samples = lua_isnil(L, -1) ? info.samples : luax_checku32(L, -1);
      lua_pop(L, 1);
    }

    lua_getfield(L, index, "linear");
    info.srgb = lua_isnil(L, -1) ? info.srgb : !lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    bool mipmappable = lovrGraphicsGetFormatSupport(info.format, TEXTURE_FEATURE_BLIT) & (1 << info.srgb);
    if (lua_type(L, -1) == LUA_TNUMBER) {
      info.mipmaps = lua_tonumber(L, -1);
    } else if (!lua_isnil(L, -1)) {
      info.mipmaps = lua_toboolean(L, -1) ? ~0u : 1;
    } else {
      info.mipmaps = (info.samples > 1 || info.imageCount == 0 || !mipmappable) ? 1 : ~0u;
    }
    if (info.imageCount > 0 && info.mipmaps > 1 && !mipmappable) {
      luaL_error(L, "This texture format does not support blitting, which is required for mipmap generation");
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "usage");
    switch (lua_type(L, -1)) {
      case LUA_TSTRING: info.usage = 1 << luax_checkenum(L, -1, TextureUsage, NULL); break;
      case LUA_TTABLE:
        info.usage = 0;
        int length = luax_len(L, -1);
        for (int i = 0; i < length; i++) {
          lua_rawgeti(L, -1, i + 1);
          info.usage |= 1 << luax_checkenum(L, -1, TextureUsage, NULL);
          lua_pop(L, 1);
        }
        break;
      case LUA_TNIL: break;
      default: luaL_error(L, "Expected Texture usage to be a string, table, or nil"); return 0;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  if (lua_type(L, 1) == LUA_TNUMBER && lua_type(L, 3) != LUA_TNUMBER && info.type == TEXTURE_CUBE) {
    info.layers = 6;
  }

  Texture* texture = lovrTextureCreate(&info);

  for (uint32_t i = 0; i < info.imageCount; i++) {
    lovrRelease(images[i], lovrImageDestroy);
  }

  if (images != stack) {
    lovrFree(images);
  }

  luax_assert(L, texture);
  luax_pushtype(L, Texture, texture);
  lovrRelease(texture, lovrTextureDestroy);
  return 1;
}

static int l_lovrGraphicsNewTextureView(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  const TextureInfo* base = lovrTextureGetInfo(texture);

  TextureViewInfo info = {
    .type = base->type,
    .layerCount = ~0u,
    .levelCount = ~0u
  };

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "type");
    info.type = lua_isnil(L, -1) ? base->type : luax_checkenum(L, -1, TextureType, NULL);
    lua_pop(L, 1);

    lua_getfield(L, 2, "layer");
    bool hasLayer = !lua_isnil(L, -1);
    info.layerIndex = hasLayer ? luax_checku32(L, -1) - 1 : 0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "layercount");
    info.layerCount = lua_isnil(L, -1) ? (hasLayer ? 1 : ~0u) : luax_checku32(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "mipmap");
    bool hasMipmap = !lua_isnil(L, -1);
    info.levelIndex = hasMipmap ? luax_checku32(L, -1) - 1 : 0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "mipmapcount");
    info.levelCount = lua_isnil(L, -1) ? (hasMipmap ? 1 : ~0u) : luax_checku32(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  Texture* view = lovrTextureCreateView(texture, &info);
  luax_assert(L, view);
  luax_pushtype(L, Texture, view);
  lovrRelease(view, lovrTextureDestroy);
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
    luaL_error(L, "Expected string or table for Sampler filter");
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
    luaL_error(L, "Expected string or table for Sampler wrap");
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "compare");
  info.compare = luax_checkcomparemode(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "anisotropy");
  info.anisotropy = luax_optfloat(L, -1, 0.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "mipmaprange");
  if (!lua_isnil(L, -1)) {
    luax_check(L, lua_istable(L, -1), "Sampler mipmap range must be nil or a table");
    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    info.range[0] = luax_checkfloat(L, -2);
    info.range[1] = luax_checkfloat(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);

  Sampler* sampler = lovrSamplerCreate(&info);
  luax_assert(L, sampler);
  luax_pushtype(L, Sampler, sampler);
  lovrRelease(sampler, lovrSamplerDestroy);
  return 1;
}

static ShaderSource luax_checkshadersource(lua_State* L, int index, ShaderStage stage, bool* shouldFree) {
  *shouldFree = false;

  if (lua_isstring(L, index)) {
    size_t length;
    const char* string = lua_tolstring(L, index, &length);

    if (memchr(string, '\n', MIN(256, length))) {
      return (ShaderSource) { stage, string, length };
    } else {
      for (int i = 0; lovrDefaultShader[i].length; i++) {
        if (lovrDefaultShader[i].length == length && !memcmp(lovrDefaultShader[i].string, string, length)) {
          return lovrGraphicsGetDefaultShaderSource(i, stage);
        }
      }

      size_t size;
      void* code = luax_readfile(string, &size);

      if (code) {
        *shouldFree = true;
        return (ShaderSource) { stage, code, size };
      } else {
        luaL_argerror(L, index, "single-line string was not filename or DefaultShader");
      }
    }
  } else if (lua_isuserdata(L, index)) {
    Blob* blob = luax_checktype(L, index, Blob);
    return (ShaderSource) { stage, blob->data, blob->size };
  } else {
    luax_typeerror(L, index, "string, Blob, or DefaultShader");
  }
  return (ShaderSource) { 0 };
}

static int l_lovrGraphicsCompileShader(lua_State* L) {
  ShaderSource inputs[2], outputs[2] = { 0 };
  bool shouldFree[2];
  uint32_t count;

  if (lua_gettop(L) == 1) {
    inputs[0] = luax_checkshadersource(L, 1, STAGE_COMPUTE, &shouldFree[0]);
    count = 1;
  } else {
    inputs[0] = luax_checkshadersource(L, 1, STAGE_VERTEX, &shouldFree[0]);
    inputs[1] = luax_checkshadersource(L, 2, STAGE_FRAGMENT, &shouldFree[1]);
    count = 2;
  }

  bool success = lovrGraphicsCompileShader(inputs, outputs, count, luax_readfile, false);

  for (uint32_t i = 0; i < count; i++) {
    if (shouldFree[i] && outputs[i].code != inputs[i].code) lovrFree((void*) inputs[i].code);
  }

  luax_assert(L, success);

  for (uint32_t i = 0; i < count; i++) {
    Blob* blob = lovrBlobCreate((void*) outputs[i].code, outputs[i].size, "Shader code");
    luax_pushtype(L, Blob, blob);
    lovrRelease(blob, lovrBlobDestroy);
  }

  return count;
}

static int l_lovrGraphicsNewShader(lua_State* L) {
  ShaderSource source[2], compiled[2];
  ShaderInfo info = { .stages = compiled };
  bool shouldFree[2] = { 0 };
  int index;

  if (lua_gettop(L) == 1 || lua_istable(L, 2)) {
    info.type = SHADER_COMPUTE;
    source[0] = luax_checkshadersource(L, 1, STAGE_COMPUTE, &shouldFree[0]);
    info.stageCount = 1;
    index = 2;
  } else {
    info.type = SHADER_GRAPHICS;
    source[0] = luax_checkshadersource(L, 1, STAGE_VERTEX, &shouldFree[0]);
    source[1] = luax_checkshadersource(L, 2, STAGE_FRAGMENT, &shouldFree[1]);
    info.stageCount = 2;
    index = 3;
  }

  arr_t(ShaderFlag) flags;
  arr_init(&flags);

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, ShaderType, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "flags");
    if (!lua_isnil(L, -1)) {
      luaL_checktype(L, -1, LUA_TTABLE);
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        ShaderFlag flag = { 0 };
        flag.value = lua_isboolean(L, -1) ? (double) lua_toboolean(L, -1) : lua_tonumber(L, -1);
        switch (lua_type(L, -2)) {
          case LUA_TSTRING: flag.name = lua_tostring(L, -2); break;
          case LUA_TNUMBER: flag.id = lua_tointeger(L, -2); break;
          default: luaL_error(L, "Unexpected ShaderFlag key type (%s)", lua_typename(L, lua_type(L, -2)));
        }
        arr_push(&flags, flag);
        lua_pop(L, 1);
      }

      info.flags = flags.data;
      info.flagCount = (uint32_t) flags.length;
      if (flags.length >= 1000) {
        for (uint32_t i = 0; i < info.stageCount; i++) {
          if (shouldFree[i]) lovrFree((void*) source[i].code);
        }
        arr_free(&flags);
        luaL_error(L, "Too many shader flags");
        return 0;
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "raw");
    info.raw = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  if (!lovrGraphicsCompileShader(source, compiled, info.stageCount, luax_readfile, info.raw)) {
    for (uint32_t i = 0; i < info.stageCount; i++) {
      if (shouldFree[i]) lovrFree((void*) source[i].code);
    }
    arr_free(&flags);
    luax_assert(L, false);
    return 0;
  }

  Shader* shader = lovrShaderCreate(&info);

  for (uint32_t i = 0; i < info.stageCount; i++) {
    if (shouldFree[i]) lovrFree((void*) source[i].code);
    if (source[i].code != compiled[i].code) lovrFree((void*) compiled[i].code);
  }
  arr_free(&flags);

  luax_assert(L, shader);
  luax_pushtype(L, Shader, shader);
  lovrRelease(shader, lovrShaderDestroy);
  return 1;
}

static Texture* luax_opttexture(lua_State* L, int index) {
  if (lua_isnil(L, index)) {
    return NULL;
  }

  Texture* texture = luax_totype(L, index, Texture);
  if (texture) return texture;

  Image* image = luax_checkimage(L, index);

  TextureInfo info = {
    .type = TEXTURE_2D,
    .format = lovrImageGetFormat(image),
    .width = lovrImageGetWidth(image, 0),
    .height = lovrImageGetHeight(image, 0),
    .layers = 1,
    .mipmaps = ~0u,
    .samples = 1,
    .usage = TEXTURE_SAMPLE,
    .srgb = lovrImageIsSRGB(image),
    .imageCount = 1,
    .images = &image
  };

  texture = lovrTextureCreate(&info);
  lovrRelease(image, lovrImageDestroy);
  luax_assert(L, texture);
  return texture;
}

static int l_lovrGraphicsNewMaterial(lua_State* L) {
  MaterialInfo info;
  memset(&info, 0, sizeof(info));

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "color");
  luax_optcolor(L, -1, info.data.color);
  lua_pop(L, 1);

  lua_getfield(L, 1, "glow");
  if (lua_isnil(L, -1)) {
    memset(info.data.glow, 0, sizeof(info.data.glow));
  } else {
    luax_optcolor(L, -1, info.data.glow);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "uvShift");
  if (lua_type(L, -1) == LUA_TNUMBER) {
    float shift = lua_tonumber(L, -1);
    info.data.uvShift[0] = shift;
    info.data.uvShift[1] = shift;
  } else if (lua_type(L, -1) == LUA_TTABLE) {
    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    info.data.uvShift[0] = luax_optfloat(L, -2, 0.f);
    info.data.uvShift[1] = luax_optfloat(L, -1, 0.f);
    lua_pop(L, 2);
  } else if (!lua_isnil(L, -1)) {
    float* v = luax_checkvector(L, -1, V_VEC2, "vec2, table, or nil");
    info.data.uvShift[0] = v[0];
    info.data.uvShift[1] = v[1];
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "uvScale");
  if (lua_isnil(L, -1)) {
    info.data.uvScale[0] = 1.f;
    info.data.uvScale[1] = 1.f;
  } else if (lua_isnumber(L, -1)) {
    float scale = lua_tonumber(L, -1);
    info.data.uvScale[0] = scale;
    info.data.uvScale[1] = scale;
  } else if (lua_type(L, -1) == LUA_TTABLE) {
    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    info.data.uvScale[0] = luax_optfloat(L, -2, 1.f);
    info.data.uvScale[1] = luax_optfloat(L, -1, 1.f);
    lua_pop(L, 2);
  } else {
    float* v = luax_checkvector(L, -1, V_VEC2, "vec2, table, or nil");
    info.data.uvScale[0] = v[0];
    info.data.uvScale[1] = v[1];
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "metalness");
  info.data.metalness = luax_optfloat(L, -1, 1.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "roughness");
  info.data.roughness = luax_optfloat(L, -1, 1.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "clearcoat");
  info.data.clearcoat = luax_optfloat(L, -1, 0.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "clearcoatRoughness");
  info.data.clearcoatRoughness = luax_optfloat(L, -1, 0.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "occlusionStrength");
  info.data.occlusionStrength = luax_optfloat(L, -1, 1.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "normalScale");
  info.data.normalScale = luax_optfloat(L, -1, 1.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "alphaCutoff");
  info.data.alphaCutoff = luax_optfloat(L, -1, 0.f);
  lua_pop(L, 1);

  lua_getfield(L, 1, "texture");
  info.texture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "glowTexture");
  info.glowTexture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "metalnessTexture");
  info.metalnessTexture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "roughnessTexture");
  info.roughnessTexture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "clearcoatTexture");
  info.clearcoatTexture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "occlusionTexture");
  info.occlusionTexture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "normalTexture");
  info.normalTexture = luax_opttexture(L, -1);
  lua_pop(L, 1);

  Material* material = lovrMaterialCreate(&info);
  luax_assert(L, material);
  luax_pushtype(L, Material, material);
  lovrRelease(material, lovrMaterialDestroy);
  return 1;
}

static int l_lovrGraphicsNewFont(lua_State* L) {
  FontInfo info = { 0 };

  info.rasterizer = luax_totype(L, 1, Rasterizer);
  info.spread = 4.;

  if (!info.rasterizer) {
    Blob* blob = NULL;
    float size;

    if (lua_type(L, 1) == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
      size = luax_optfloat(L, 1, 32.);
      info.spread = luaL_optnumber(L, 2, info.spread);
    } else {
      blob = luax_readblob(L, 1, "Font");
      size = luax_optfloat(L, 2, 32.);
      info.spread = luaL_optnumber(L, 3, info.spread);
    }

    info.rasterizer = lovrRasterizerCreate(blob, size, luax_readfile);
    lovrRelease(blob, lovrBlobDestroy);
    luax_assert(L, info.rasterizer);
  } else {
    info.spread = luaL_optnumber(L, 2, info.spread);
    lovrRetain(info.rasterizer);
  }

  Font* font = lovrFontCreate(&info);
  lovrRelease(info.rasterizer, lovrRasterizerDestroy);
  luax_assert(L, font);
  luax_pushtype(L, Font, font);
  lovrRelease(font, lovrFontDestroy);
  return 1;
}

static int l_lovrGraphicsNewMesh(lua_State* L) {
  MeshInfo info = { 0 };

  DataField stack[17];
  DataField* format = stack;
  bool customFormat = false;

  if (lua_istable(L, 1)) {
    lua_rawgeti(L, 1, 1);
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "type");
      lua_rawgeti(L, -2, 1);
      if (lua_type(L, -2) == LUA_TSTRING || lua_type(L, -1) == LUA_TSTRING) {
        info.vertexFormat = format;
        format[0] = (DataField) { .fields = format + 1, .fieldCount = luax_len(L, 1) };
        luax_check(L, format->fieldCount + 1 <= COUNTOF(stack), "Mesh has too many vertex attributes (max is %d)", COUNTOF(stack) - 1);
        for (uint32_t i = 0; i < format->fieldCount; i++) {
          lua_rawgeti(L, 1, i + 1);
          luax_check(L, lua_istable(L, -1), "Expected table of tables");
          DataField* attribute = &format->fields[i];
          memset(attribute, 0, sizeof(*attribute));

          lua_getfield(L, -1, "name");
          if (lua_isnil(L, -1)) lua_pop(L, 1), lua_rawgeti(L, -1, 1);
          luax_check(L, lua_type(L, -1) == LUA_TSTRING, "Mesh attribute must have 'name' key");
          attribute->name = lua_tostring(L, -1);
          lua_pop(L, 1);

          lua_getfield(L, -1, "type");
          if (lua_isnil(L, -1)) lua_pop(L, 1), lua_rawgeti(L, -1, 2);
          luax_check(L, lua_type(L, -1) == LUA_TSTRING, "Mesh attribute must have 'type' key");
          attribute->type = luax_checkdatatype(L, -1);
          lua_pop(L, 1);

          lua_getfield(L, -1, "offset");
          attribute->offset = luax_optu32(L, -1, 0);
          lua_pop(L, 2);
        }

        lua_getfield(L, 1, "stride");
        format->stride = luax_optu32(L, -1, 0);
        lua_pop(L, 1);

        customFormat = true;
      }
      lua_pop(L, 2);
    }
    lua_pop(L, 1);
  }

  DataField defaultFormat[] = {
    { .stride = 32 },
    { .name = "VertexPosition", .type = TYPE_F32x3, .offset = 0 },
    { .name = "VertexNormal", .type = TYPE_F32x3, .offset = 12 },
    { .name = "VertexUV", .type = TYPE_F32x2, .offset = 24 }
  };

  if (!customFormat) {
    format = defaultFormat;
    info.vertexFormat = defaultFormat;
    info.vertexFormat->fields = defaultFormat + 1;
    info.vertexFormat->fieldCount = COUNTOF(defaultFormat) - 1;
  }

  lovrGraphicsAlignFields(format, LAYOUT_PACKED);

  Blob* blob = NULL;
  bool hasData = false;
  int index = 1 + customFormat;
  switch (lua_type(L, index)) {
    case LUA_TNUMBER: format->length = luax_checku32(L, index); break;
    case LUA_TTABLE: format->length = luax_len(L, index); hasData = true; break;
    case LUA_TUSERDATA:
      if ((info.vertexBuffer = luax_totype(L, index, Buffer)) != NULL) break;
      if ((blob = luax_totype(L, index, Blob)) != NULL) {
        luax_check(L, blob->size % format->stride == 0, "Blob size must be a multiple of vertex size");
        luax_check(L, blob->size < UINT32_MAX, "Max Blob size is 4GB");
        format->length = (uint32_t) (blob->size / format->stride);
        hasData = true;
        break;
      }
    default: return luax_typeerror(L, index, "number, table, Blob, or Buffer");
  }

  if (info.vertexBuffer) {
    info.storage = MESH_GPU;
  } else {
    info.storage = luax_checkenum(L, index + 1, MeshStorage, "cpu");
  }

  void* vertices = NULL;
  Mesh* mesh = lovrMeshCreate(&info, hasData ? &vertices : NULL);
  luax_assert(L, mesh);

  if (blob) {
    memcpy(vertices, blob->data, blob->size);
  } else if (hasData) {
    luax_checkbufferdata(L, index, lovrMeshGetVertexFormat(mesh), vertices);
  }

  luax_pushtype(L, Mesh, mesh);
  lovrRelease(mesh, lovrMeshDestroy);
  return 1;
}

static int l_lovrGraphicsNewModel(lua_State* L) {
  ModelInfo info = { 0 };
  info.data = luax_totype(L, 1, ModelData);
  info.materials = true;
  info.mipmaps = true;

  if (!info.data) {
    Blob* blob = luax_readblob(L, 1, "Model");
    info.data = lovrModelDataCreate(blob, luax_readfile);
    lovrRelease(blob, lovrBlobDestroy);
    luax_assert(L, info.data);
  } else {
    lovrRetain(info.data);
  }

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "mipmaps");
    info.mipmaps = lua_isnil(L, -1) || lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "materials");
    info.materials = lua_isnil(L, -1) || lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  Model* model = lovrModelCreate(&info);
  lovrRelease(info.data, lovrModelDataDestroy);
  luax_assert(L, model);
  luax_pushtype(L, Model, model);
  lovrRelease(model, lovrModelDestroy);
  return 1;
}

int l_lovrPassSetCanvas(lua_State* L);

static int l_lovrGraphicsNewPass(lua_State* L) {
  const char* label = NULL;
  if (lua_istable(L, 1)) {
    lua_getfield(L, 1, "label");
    label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }
  Pass* pass = lovrPassCreate(label);
  luax_assert(L, pass);
  luax_pushtype(L, Pass, pass);
  lua_insert(L, 1);
  l_lovrPassSetCanvas(L);
  lua_settop(L, 1);
  lovrRelease(pass, lovrPassDestroy);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {
  { "initialize", l_lovrGraphicsInitialize },
  { "isInitialized", l_lovrGraphicsIsInitialized },
  { "isTimingEnabled", l_lovrGraphicsIsTimingEnabled },
  { "setTimingEnabled", l_lovrGraphicsSetTimingEnabled },
  { "submit", l_lovrGraphicsSubmit },
  { "present", l_lovrGraphicsPresent },
  { "wait", l_lovrGraphicsWait },
  { "getDevice", l_lovrGraphicsGetDevice },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "isFormatSupported", l_lovrGraphicsIsFormatSupported },
  { "getBackgroundColor", l_lovrGraphicsGetBackgroundColor },
  { "setBackgroundColor", l_lovrGraphicsSetBackgroundColor },
  { "getWindowPass", l_lovrGraphicsGetWindowPass },
  { "getDefaultFont", l_lovrGraphicsGetDefaultFont },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newTexture", l_lovrGraphicsNewTexture },
  { "newTextureView", l_lovrGraphicsNewTextureView },
  { "newSampler", l_lovrGraphicsNewSampler },
  { "compileShader", l_lovrGraphicsCompileShader },
  { "newShader", l_lovrGraphicsNewShader },
  { "newMaterial", l_lovrGraphicsNewMaterial },
  { "newFont", l_lovrGraphicsNewFont },
  { "newMesh", l_lovrGraphicsNewMesh },
  { "newModel", l_lovrGraphicsNewModel },
  { "newPass", l_lovrGraphicsNewPass },
  { NULL, NULL }
};

extern const luaL_Reg lovrBuffer[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrSampler[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrMaterial[];
extern const luaL_Reg lovrFont[];
extern const luaL_Reg lovrMesh[];
extern const luaL_Reg lovrModel[];
extern const luaL_Reg lovrReadback[];
extern const luaL_Reg lovrPass[];

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  luax_registertype(L, Buffer);
  luax_registertype(L, Texture);
  luax_registertype(L, Sampler);
  luax_registertype(L, Shader);
  luax_registertype(L, Material);
  luax_registertype(L, Font);
  luax_registertype(L, Mesh);
  luax_registertype(L, Model);
  luax_registertype(L, Readback);
  luax_registertype(L, Pass);
  return 1;
}
