#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "data/rasterizer.h"
#include "util.h"
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
  [SHADER_FONT] = ENTRY("font"),
  { 0 }
};

StringEntry lovrDrawStyle[] = {
  [STYLE_FILL] = ENTRY("fill"),
  [STYLE_LINE] = ENTRY("line"),
  { 0 }
};

StringEntry lovrFieldType[] = {
  [FIELD_I8x4] = ENTRY("i8x4"),
  [FIELD_U8x4] = ENTRY("u8x4"),
  [FIELD_SN8x4] = ENTRY("sn8x4"),
  [FIELD_UN8x4] = ENTRY("un8x4"),
  [FIELD_UN10x3] = ENTRY("un10x3"),
  [FIELD_I16] = ENTRY("i16"),
  [FIELD_I16x2] = ENTRY("i16x2"),
  [FIELD_I16x4] = ENTRY("i16x4"),
  [FIELD_U16] = ENTRY("u16"),
  [FIELD_U16x2] = ENTRY("u16x2"),
  [FIELD_U16x4] = ENTRY("u16x4"),
  [FIELD_SN16x2] = ENTRY("sn16x2"),
  [FIELD_SN16x4] = ENTRY("sn16x4"),
  [FIELD_UN16x2] = ENTRY("un16x2"),
  [FIELD_UN16x4] = ENTRY("un16x4"),
  [FIELD_I32] = ENTRY("i32"),
  [FIELD_I32x2] = ENTRY("i32x2"),
  [FIELD_I32x3] = ENTRY("i32x3"),
  [FIELD_I32x4] = ENTRY("i32x4"),
  [FIELD_U32] = ENTRY("u32"),
  [FIELD_U32x2] = ENTRY("u32x2"),
  [FIELD_U32x3] = ENTRY("u32x3"),
  [FIELD_U32x4] = ENTRY("u32x4"),
  [FIELD_F16x2] = ENTRY("f16x2"),
  [FIELD_F16x4] = ENTRY("f16x4"),
  [FIELD_F32] = ENTRY("f32"),
  [FIELD_F32x2] = ENTRY("f32x2"),
  [FIELD_F32x3] = ENTRY("f32x3"),
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
  { 0 }
};

StringEntry lovrShaderStage[] = {
  [STAGE_VERTEX] = ENTRY("vertex"),
  [STAGE_FRAGMENT] = ENTRY("fragment"),
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

StringEntry lovrTextureFeature[] = {
  [0] = ENTRY("sample"),
  [1] = ENTRY("filter"),
  [2] = ENTRY("render"),
  [3] = ENTRY("blend"),
  [4] = ENTRY("storage"),
  [5] = ENTRY("atomic"),
  [6] = ENTRY("blitsrc"),
  [7] = ENTRY("blitdst"),
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

StringEntry lovrVertexMode[] = {
  [VERTEX_POINTS] = ENTRY("points"),
  [VERTEX_LINES] = ENTRY("lines"),
  [VERTEX_TRIANGLES] = ENTRY("triangles"),
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
  { 0 }
};

static struct { uint32_t size, scalarAlign, baseAlign, components; } fieldInfo[] = {
  [FIELD_I8x4] = { 4, 1, 4, 4 },
  [FIELD_U8x4] = { 4, 1, 4, 4 },
  [FIELD_SN8x4] = { 4, 1, 4, 4 },
  [FIELD_UN8x4] = { 4, 1, 4, 4 },
  [FIELD_UN10x3] = { 4, 4, 4, 3 },
  [FIELD_I16] = { 2, 2, 2, 1 },
  [FIELD_I16x2] = { 4, 2, 4, 2 },
  [FIELD_I16x4] = { 8, 2, 8, 4 },
  [FIELD_U16] = { 2, 2, 2, 1 },
  [FIELD_U16x2] = { 4, 2, 4, 2 },
  [FIELD_U16x4] = { 8, 2, 8, 4 },
  [FIELD_SN16x2] = { 4, 2, 4, 2 },
  [FIELD_SN16x4] = { 8, 2, 8, 4 },
  [FIELD_UN16x2] = { 4, 2, 4, 2 },
  [FIELD_UN16x4] = { 8, 2, 8, 4 },
  [FIELD_I32] = { 4, 4, 4, 1 },
  [FIELD_I32x2] = { 8, 4, 8, 2 },
  [FIELD_I32x3] = { 12, 4, 16, 3 },
  [FIELD_I32x4] = { 16, 4, 16, 4 },
  [FIELD_U32] = { 4, 4, 4, 1 },
  [FIELD_U32x2] = { 8, 4, 8, 2 },
  [FIELD_U32x3] = { 12, 4, 16, 3 },
  [FIELD_U32x4] = { 16, 4, 16, 4 },
  [FIELD_F16x2] = { 4, 2, 4, 2 },
  [FIELD_F16x4] = { 8, 2, 8, 4 },
  [FIELD_F32] = { 4, 4, 4, 1 },
  [FIELD_F32x2] = { 8, 4, 8, 2 },
  [FIELD_F32x3] = { 12, 4, 16, 3 },
  [FIELD_F32x4] = { 16, 4, 16, 4 },
  [FIELD_MAT2] = { 16, 4, 8, 4 },
  [FIELD_MAT3] = { 64, 4, 16, 9 },
  [FIELD_MAT4] = { 64, 4, 16, 16 }
};

static uint32_t luax_checkfieldtype(lua_State* L, int index, uint32_t* nameHash) {
  size_t length;
  const char* string = luaL_checklstring(L, index, &length);

  char* colon = strchr(string, ':');

  if (colon) {
    *nameHash = (uint32_t) hash64(colon + 1, length - (colon + 1 - string));
    length = colon - string;
  } else {
    *nameHash = 0;
  }

  if (length < 3) {
    return luaL_error(L, "invalid FieldType '%s'", string), 0;
  }

  // Plurals are allowed and ignored
  if (string[length - 1] == 's') {
    length--;
  }

  // x1 is allowed and ignored
  if (string[length - 2] == 'x' && string[length - 1] == '1') {
    length -= 2;
  }

  // vec[234]
  if (length == 4 && string[0] == 'v' && string[1] == 'e' && string[2] == 'c' && string[3] >= '2' && string[3] <= '4') {
    return FIELD_F32x2 + string[3] - '2';
  }

  if (length == 3 && !memcmp(string, "int", length)) {
    return FIELD_I32;
  }

  if (length == 4 && !memcmp(string, "uint", length)) {
    return FIELD_U32;
  }

  if (length == 5 && !memcmp(string, "float", length)) {
    return FIELD_F32;
  }

  if (length == 5 && !memcmp(string, "color", length)) {
    return FIELD_UN8x4;
  }

  for (int i = 0; lovrFieldType[i].length; i++) {
    if (length == lovrFieldType[i].length && !memcmp(string, lovrFieldType[i].string, length)) {
      return i;
    }
  }

  return luaL_error(L, "invalid FieldType '%s'", string), 0;
}

static void luax_checkbufferformat(lua_State* L, int index, BufferInfo* info) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
      info->stride = 1;
      break;
    case LUA_TSTRING:
      info->fieldCount = 1;
      info->fields[0].type = luax_checkfieldtype(L, index, &info->fields[0].hash);
      info->stride = fieldInfo[info->fields[0].type].size;
      break;
    case LUA_TTABLE:
      lua_getfield(L, index, "layout");
      BufferLayout layout = luax_checkenum(L, -1, BufferLayout, "packed");
      lua_pop(L, 1);

      int length = info->fieldCount = luax_len(L, index);
      lovrAssert(info->fieldCount <= COUNTOF(info->fields), "Too many buffer fields (max is %d)", COUNTOF(info->fields));

      uint32_t offset = 0;
      uint32_t extent = 0;
      uint32_t location = 0;
      uint32_t maxAlign = 1;
      for (int i = 0; i < length; i++) {
        BufferField* field = &info->fields[i];

        lua_rawgeti(L, index, i + 1);
        if (lua_istable(L, -1)) {
          lua_getfield(L, -1, "type");
          lovrAssert(lua_type(L, -1) == LUA_TSTRING, "When given as a table, Buffer fields must have a 'type' key.");
          field->type = luax_checkfieldtype(L, -1, &field->hash);
          lua_pop(L, 1);

          uint32_t align = layout == LAYOUT_PACKED ? fieldInfo[field->type].scalarAlign : fieldInfo[field->type].baseAlign;
          maxAlign = MAX(maxAlign, align);

          lua_getfield(L, -1, "offset");
          if (lua_isnil(L, -1)) {
            field->offset = ALIGN(offset, align);
            offset = field->offset + fieldInfo[field->type].size;
          } else {
            field->offset = luaL_checkinteger(L, -1);
          }
          lua_pop(L, 1);

          lua_getfield(L, -1, "location");
          field->location = lua_isnil(L, -1) ? location++ : luaL_checkinteger(L, -1);
          lua_pop(L, 1);

          lua_getfield(L, -1, "name");
          if (lua_type(L, -1) == LUA_TSTRING) {
            size_t nameLength;
            const char* name = lua_tolstring(L, -1, &nameLength);
            field->hash = (uint32_t) hash64(name, nameLength);
          }
          lua_pop(L, 1);
        } else if (lua_isstring(L, -1)) {
          FieldType type = luax_checkfieldtype(L, -1, &field->hash);
          uint32_t align = layout == LAYOUT_PACKED ? fieldInfo[type].scalarAlign : fieldInfo[type].baseAlign;
          field->offset = ALIGN(offset, align);
          field->location = location++;
          field->type = type;
          offset = field->offset + fieldInfo[type].size;
          maxAlign = MAX(maxAlign, align);
        }
        lua_pop(L, 1);

        extent = MAX(extent, field->offset + fieldInfo[field->type].size);
      }

      lua_getfield(L, index, "stride");
      if (lua_isnil(L, -1)) {
        switch (layout) {
          case LAYOUT_PACKED: info->stride = offset; break;
          case LAYOUT_STD140: info->stride = ALIGN(offset, 16); break;
          case LAYOUT_STD430: info->stride = ALIGN(offset, maxAlign); break;
          default: break;
        }
      } else {
        info->stride = luax_checku32(L, -1);
        lovrCheck(info->stride > 0, "Buffer stride can not be zero");
        lovrCheck(info->stride <= extent, "Buffer stride can not be smaller than the size of a single item");
      }
      lua_pop(L, 1);
      break;
    default: luaL_argerror(L, index, "Expected nil, string, or table");
  }
}

static Canvas luax_checkcanvas(lua_State* L, int index) {
  Canvas canvas = {
    .loads = { LOAD_CLEAR, LOAD_CLEAR, LOAD_CLEAR, LOAD_CLEAR },
    .depth.format = FORMAT_D32F,
    .depth.load = LOAD_CLEAR,
    .depth.clear = 1.f,
    .samples = 4
  };

  for (uint32_t i = 0; i < 4; i++) {
    lovrGraphicsGetBackground(canvas.clears[i]); // srgb conversion here does not spark joy
  }

  if (lua_type(L, index) == LUA_TSTRING && !strcmp(lua_tostring(L, index), "window")) {
    canvas.textures[0] = lovrGraphicsGetWindowTexture();
  } else if (lua_isuserdata(L, index)) {
    canvas.textures[0] = luax_checktype(L, index, Texture);
  } else if (!lua_istable(L, index)) {
    luax_typeerror(L, index, "Texture or table");
  } else {
    for (uint32_t i = 0; i < 4; i++) {
      lua_rawgeti(L, index, i + 1);
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
    } else if (lua_isnumber(L, -1) || lua_isuserdata(L, -1)) {
      luax_optcolor(L, -1, canvas.clears[1]);
    } else if (!lua_isnil(L, -1)) {
      LoadAction load = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
      canvas.loads[0] = canvas.loads[1] = canvas.loads[2] = canvas.loads[3] = load;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "samples");
    canvas.samples = lua_isnil(L, -1) ? canvas.samples : lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    canvas.mipmap = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  return canvas;
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

static int l_lovrGraphicsInit(lua_State* L) {
  bool debug = false;
  bool vsync = false;

  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "vsync");
    vsync = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (lovrGraphicsInit(debug, vsync)) {
    luax_atexit(L, lovrGraphicsDestroy);
  }

  return 0;
}

static int l_lovrGraphicsSubmit(lua_State* L) {
  bool table = lua_istable(L, 1);
  int length = table ? luax_len(L, 1) : lua_gettop(L);
  uint32_t count = 0;

  Pass* stack[8];
  Pass** passes = (size_t) length > COUNTOF(stack) ? malloc(length * sizeof(Pass*)) : stack;
  lovrAssert(passes, "Out of memory");

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

  lovrGraphicsSubmit(passes, count);
  if (passes != stack) free(passes);
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrGraphicsWait(lua_State* L) {
  lovrGraphicsWait();
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
  lua_pushboolean(L, features.indirectDrawFirstInstance), lua_setfield(L, -2, "indirectDrawFirstInstance");
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
  lua_pushinteger(L, limits.shaderConstantSize), lua_setfield(L, -2, "shaderConstantSize");
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
  bool supported = lovrGraphicsIsFormatSupported(format, features);
  lua_pushboolean(L, supported);
  return 1;
}

static int l_lovrGraphicsGetBackground(lua_State* L) {
  float color[4];
  lovrGraphicsGetBackground(color);
  lua_pushnumber(L, color[0]);
  lua_pushnumber(L, color[1]);
  lua_pushnumber(L, color[2]);
  lua_pushnumber(L, color[3]);
  return 4;
}

static int l_lovrGraphicsSetBackground(lua_State* L) {
  float color[4];
  luax_readcolor(L, 1, color);
  lovrGraphicsSetBackground(color);
  return 0;
}

static int l_lovrGraphicsGetDefaultFont(lua_State* L) {
  Font* font = lovrGraphicsGetDefaultFont();
  luax_pushtype(L, Font, font);
  return 1;
}

static int l_lovrGraphicsGetBuffer(lua_State* L) {
  BufferInfo info = { 0 };

  luax_checkbufferformat(L, 2, &info);

  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: info.length = lua_tointeger(L, 1); break;
    case LUA_TTABLE: info.length = luax_len(L, 1); break;
    default: {
      Blob* blob = luax_totype(L, 1, Blob);
      if (blob) {
        info.length = (graphics_size)blob->size / info.stride;
        break;
      } else {
        return luax_typeerror(L, 1, "number, table, or Blob");
      }
    }
  }

  void* pointer;
  bool hasData = !lua_isnumber(L, 1);
  Buffer* buffer = lovrGraphicsGetBuffer(&info, hasData ? &pointer : NULL);

  if (hasData) {
    lua_settop(L, 1);
    luax_readbufferdata(L, 1, buffer, pointer);
  }

  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewBuffer(lua_State* L) {
  BufferInfo info = { 0 };

  luax_checkbufferformat(L, 2, &info);

  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: info.length = lua_tointeger(L, 1); break;
    case LUA_TTABLE: info.length = luax_len(L, -1); break;
    default: {
      Blob* blob = luax_totype(L, 1, Blob);
      if (blob) {
        info.length = (graphics_size)(blob->size / info.stride);
        break;
      } else {
        return luax_typeerror(L, 1, "number, table, or Blob");
      }
    }
  }

  void* pointer;
  bool hasData = !lua_isnumber(L, 1);
  Buffer* buffer = lovrBufferCreate(&info, hasData ? &pointer : NULL);

  if (hasData) {
    lua_settop(L, 1);
    luax_readbufferdata(L, 1, buffer, pointer);
  }

  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewTexture(lua_State* L) {
  TextureInfo info = {
    .type = TEXTURE_2D,
    .format = FORMAT_RGBA8,
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
      info.depth = luax_checku32(L, index++);
      info.type = TEXTURE_ARRAY;
    }
    info.usage |= TEXTURE_RENDER;
  } else if (lua_istable(L, 1)) {
    info.imageCount = luax_len(L, index++);
    images = info.imageCount > COUNTOF(stack) ? malloc(info.imageCount * sizeof(Image*)) : stack;
    lovrAssert(images, "Out of memory");
    info.type = TEXTURE_ARRAY;
    info.depth = info.imageCount;

    if (info.imageCount == 0) {
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
        lovrAssert(!lua_isnil(L, -1), "No array texture layers given and cubemap face '%s' missing", faces[i]);
        images[i] = luax_checkimage(L, -1);
      }
    }

    lovrCheck(lovrImageGetLayerCount(images[0]) == 1, "When a list of images is provided, each must have a single layer");
  } else {
    info.imageCount = 1;
    images[0] = luax_checkimage(L, index++);
    info.depth = lovrImageGetLayerCount(images[0]);
    if (lovrImageIsCube(images[0])) {
      info.type = TEXTURE_CUBE;
    } else if (info.depth > 1) {
      info.type = TEXTURE_ARRAY;
    }
  }

  if (info.imageCount > 0) {
    info.images = images;
    Image* image = images[0];
    uint32_t levels = lovrImageGetLevelCount(image);
    info.format = lovrImageGetFormat(image);
    info.width = lovrImageGetWidth(image, 0);
    info.height = lovrImageGetHeight(image, 0);
    info.mipmaps = levels == 1 ? ~0u : levels;
    info.srgb = lovrImageIsSRGB(image);
    for (uint32_t i = 1; i < info.imageCount; i++) {
      lovrAssert(lovrImageGetWidth(images[0], 0) == lovrImageGetWidth(images[i], 0), "Image widths must match");
      lovrAssert(lovrImageGetHeight(images[0], 0) == lovrImageGetHeight(images[i], 0), "Image heights must match");
      lovrAssert(lovrImageGetFormat(images[0]) == lovrImageGetFormat(images[i]), "Image formats must match");
      lovrAssert(lovrImageGetLevelCount(images[0]) == lovrImageGetLevelCount(images[i]), "Image mipmap counts must match");
      lovrAssert(lovrImageGetLayerCount(images[i]) == 1, "When a list of images are provided, each must have a single layer");
    }
  }

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, TextureType, NULL);
    lua_pop(L, 1);

    if (info.imageCount == 0) {
      lua_getfield(L, index, "format");
      info.format = lua_isnil(L, -1) ? info.format : luax_checkenum(L, -1, TextureFormat, NULL);
      lua_pop(L, 1);

      lua_getfield(L, index, "samples");
      info.samples = lua_isnil(L, -1) ? info.samples : luax_checku32(L, -1);
      lua_pop(L, 1);
    }

    lua_getfield(L, index, "linear");
    info.srgb = lua_isnil(L, -1) ? info.srgb : !lua_toboolean(L, -1);
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
      case LUA_TNIL: break;
      default: return luaL_error(L, "Expected Texture usage to be a string, table, or nil");
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  if (info.imageCount == 0 && info.depth == 0) {
    info.depth = info.type == TEXTURE_CUBE ? 6 : 1;
  }

  Texture* texture = lovrTextureCreate(&info);

  for (uint32_t i = 0; i < info.imageCount; i++) {
    lovrRelease(images[i], lovrImageDestroy);
  }

  if (images != stack) {
    free(images);
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
    lovrThrow("Expected string or table for Sampler wrap");
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

static Blob* luax_checkshadercode(lua_State* L, int index, ShaderStage stage) {
  Blob* source;
  size_t length;
  const char* string = lua_tolstring(L, index, &length);
  if (string && memchr(string, '\n', MIN(256, length))) {
    void* data = malloc(length);
    lovrAssert(data, "Out of memory");
    memcpy(data, string, length);
    source = lovrBlobCreate(data, length, "Shader code");
  } else {
    source = luax_readblob(L, index, "Shader");
  }

  Blob* code = lovrGraphicsCompileShader(stage, source);
  lovrRelease(source, lovrBlobDestroy);
  return code;
}

static int l_lovrGraphicsCompileShader(lua_State* L) {
  ShaderStage stage = luax_checkenum(L, 1, ShaderStage, NULL);
  Blob* source = luax_checkshadercode(L, 2, stage);
  Blob* output = lovrGraphicsCompileShader(stage, source);
  luax_pushtype(L, Blob, output);
  lovrRelease(output, lovrBlobDestroy);
  lovrRelease(source, lovrBlobDestroy);
  return 1;
}

static int l_lovrGraphicsNewShader(lua_State* L) {
  ShaderInfo info = { 0 };
  int index;

  if (lua_gettop(L) == 1 || lua_istable(L, 2)) {
    info.type = SHADER_COMPUTE;
    info.stages[0] = luax_checkshadercode(L, 1, STAGE_COMPUTE);
    index = 2;
  } else {
    info.type = SHADER_GRAPHICS;
    info.stages[0] = luax_checkshadercode(L, 1, STAGE_VERTEX);
    info.stages[1] = luax_checkshadercode(L, 2, STAGE_FRAGMENT);
    index = 3;
  }

  arr_t(ShaderFlag) flags;
  arr_init(&flags, realloc);

  if (lua_istable(L, index)) {
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
          default: lovrThrow("Unexpected ShaderFlag key type (%s)", lua_typename(L, lua_type(L, -2)));
        }
        arr_push(&flags, flag);
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  info.flags = flags.data;
  info.flagCount = (graphics_size)(flags.length);
  Shader* shader = lovrShaderCreate(&info);
  lovrRelease(info.stages[0], lovrBlobDestroy);
  lovrRelease(info.stages[1], lovrBlobDestroy);
  arr_free(&flags);

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
    .depth = 1,
    .mipmaps = ~0u,
    .samples = 1,
    .usage = TEXTURE_SAMPLE,
    .srgb = lovrImageIsSRGB(image),
    .imageCount = 1,
    .images = &image
  };

  texture = lovrTextureCreate(&info);
  lovrRelease(image, lovrImageDestroy);
  return texture;
}

static int l_lovrGraphicsNewMaterial(lua_State* L) {
  MaterialInfo info;
  memset(&info, 0, sizeof(info));

  Texture* texture = luax_totype(L, 1, Texture);

  if (texture) {
    info.texture = texture;
    info.data.color[0] = 1.f;
    info.data.color[1] = 1.f;
    info.data.color[2] = 1.f;
    info.data.color[3] = 1.f;
    info.data.uvScale[0] = 1.f;
    info.data.uvScale[1] = 1.f;
  } else {
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
    if (lua_type(L, -1) == LUA_TTABLE) {
      lua_rawgeti(L, -1, 1);
      lua_rawgeti(L, -1, 2);
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
      lua_rawgeti(L, -1, 2);
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

    lua_getfield(L, 1, "glowStrength");
    info.data.glowStrength = luax_optfloat(L, -1, 1.f);
    lua_pop(L, 1);

    lua_getfield(L, 1, "normalScale");
    info.data.normalScale = luax_optfloat(L, -1, 1.f);
    lua_pop(L, 1);

    lua_getfield(L, 1, "alphaCutoff");
    info.data.alphaCutoff = luax_optfloat(L, -1, 0.f);
    lua_pop(L, 1);

    lua_getfield(L, 1, "pointSize");
    info.data.pointSize = luax_optfloat(L, -1, 1.f);
    lua_pop(L, 1);

    lua_getfield(L, 1, "texture");
    info.texture = luax_opttexture(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "glowTexture");
    info.glowTexture = luax_opttexture(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "occlusionTexture");
    info.occlusionTexture = luax_opttexture(L, -1);
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

    lua_getfield(L, 1, "normalTexture");
    info.normalTexture = luax_opttexture(L, -1);
    lua_pop(L, 1);
  }

  Material* material = lovrMaterialCreate(&info);
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
      size = luax_optfloat(L, 1, 32.f);
      info.spread = luaL_optnumber(L, 2, info.spread);
    } else {
      blob = luax_readblob(L, 1, "Font");
      size = luax_optfloat(L, 2, 32.f);
      info.spread = luaL_optnumber(L, 3, info.spread);
    }

    info.rasterizer = lovrRasterizerCreate(blob, size);
    lovrRelease(blob, lovrBlobDestroy);
  } else {
    info.spread = luaL_optnumber(L, 2, info.spread);
  }

  Font* font = lovrFontCreate(&info);
  luax_pushtype(L, Font, font);
  lovrRelease(info.rasterizer, lovrRasterizerDestroy);
  lovrRelease(font, lovrFontDestroy);
  return 1;
}

static int l_lovrGraphicsGetPass(lua_State* L) {
  PassInfo info;
  info.type = luax_checkenum(L, 1, PassType, NULL);
  if (info.type == PASS_RENDER) info.canvas = luax_checkcanvas(L, 2);
  info.label = lua_tostring(L, info.type == PASS_RENDER ? 3 : 2);

  Pass* pass = lovrGraphicsGetPass(&info);
  luax_pushtype(L, Pass, pass);
  lovrRelease(pass, lovrPassDestroy);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {
  { "init", l_lovrGraphicsInit },
  { "submit", l_lovrGraphicsSubmit },
  { "wait", l_lovrGraphicsWait },
  { "getDevice", l_lovrGraphicsGetDevice },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "isFormatSupported", l_lovrGraphicsIsFormatSupported },
  { "getBackground", l_lovrGraphicsGetBackground },
  { "setBackground", l_lovrGraphicsSetBackground },
  { "getDefaultFont", l_lovrGraphicsGetDefaultFont },
  { "getBuffer", l_lovrGraphicsGetBuffer },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newTexture", l_lovrGraphicsNewTexture },
  { "newSampler", l_lovrGraphicsNewSampler },
  { "compileShader", l_lovrGraphicsCompileShader },
  { "newShader", l_lovrGraphicsNewShader },
  { "newMaterial", l_lovrGraphicsNewMaterial },
  { "newFont", l_lovrGraphicsNewFont },
  { "getPass", l_lovrGraphicsGetPass },
  { NULL, NULL }
};

extern const luaL_Reg lovrBuffer[];
extern const luaL_Reg lovrTexture[];
extern const luaL_Reg lovrSampler[];
extern const luaL_Reg lovrShader[];
extern const luaL_Reg lovrMaterial[];
extern const luaL_Reg lovrFont[];
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
  luax_registertype(L, Pass);
  return 1;
}
