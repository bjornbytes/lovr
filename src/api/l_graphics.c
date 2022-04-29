#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

StringEntry lovrBufferLayout[] = {
  [LAYOUT_PACKED] = ENTRY("packed"),
  [LAYOUT_STD140] = ENTRY("std140"),
  [LAYOUT_STD430] = ENTRY("std430"),
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

static uint32_t luax_checkfieldtype(lua_State* L, int index) {
  size_t length;
  const char* string = luaL_checklstring(L, index, &length);

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

  if (length == 6 && !memcmp(string, "normal", length)) {
    return FIELD_UN10x3;
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
      info->fields[0].type = luax_checkfieldtype(L, index);
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
          field->type = luax_checkenum(L, -1, FieldType, NULL);
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
          field->location = lua_isnil(L, -1) ? location++ : luaL_checkinteger(L, -1); // TODO names
          lua_pop(L, 1);
        } else if (lua_isstring(L, -1)) {
          FieldType type = luax_checkfieldtype(L, -1);
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

static int l_lovrGraphicsInit(lua_State* L) {
  bool debug = false;

  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (lovrGraphicsInit(debug)) {
    luax_atexit(L, lovrGraphicsDestroy);
  }

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

static int l_lovrGraphicsGetBuffer(lua_State* L) {
  BufferInfo info = { 0 };

  luax_checkbufferformat(L, 2, &info);

  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: info.length = lua_tointeger(L, 1); break;
    case LUA_TTABLE: info.length = luax_len(L, -1); break;
    default: {
      Blob* blob = luax_totype(L, 1, Blob);
      if (blob) {
        info.length = blob->size / info.stride;
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
        info.length = blob->size / info.stride;
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

static const luaL_Reg lovrGraphics[] = {
  { "init", l_lovrGraphicsInit },
  { "getDevice", l_lovrGraphicsGetDevice },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "getBuffer", l_lovrGraphicsGetBuffer },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { NULL, NULL }
};

extern const luaL_Reg lovrBuffer[];

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  luax_registertype(L, Buffer);
  return 1;
}
