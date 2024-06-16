#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "math/math.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static const uint32_t typeComponents[] = {
  [TYPE_I8x4] = 4,
  [TYPE_U8x4] = 4,
  [TYPE_SN8x4] = 4,
  [TYPE_UN8x4] = 4,
  [TYPE_SN10x3] = 3,
  [TYPE_UN10x3] = 3,
  [TYPE_I16] = 1,
  [TYPE_I16x2] = 2,
  [TYPE_I16x4] = 4,
  [TYPE_U16] = 1,
  [TYPE_U16x2] = 2,
  [TYPE_U16x4] = 4,
  [TYPE_SN16x2] = 2,
  [TYPE_SN16x4] = 4,
  [TYPE_UN16x2] = 2,
  [TYPE_UN16x4] = 4,
  [TYPE_I32] = 1,
  [TYPE_I32x2] = 2,
  [TYPE_I32x3] = 3,
  [TYPE_I32x4] = 4,
  [TYPE_U32] = 1,
  [TYPE_U32x2] = 2,
  [TYPE_U32x3] = 3,
  [TYPE_U32x4] = 4,
  [TYPE_F16x2] = 2,
  [TYPE_F16x4] = 4,
  [TYPE_F32] = 1,
  [TYPE_F32x2] = 2,
  [TYPE_F32x3] = 3,
  [TYPE_F32x4] = 4,
  [TYPE_MAT2] = 4,
  [TYPE_MAT3] = 9,
  [TYPE_MAT4] = 16,
  [TYPE_INDEX16] = 1,
  [TYPE_INDEX32] = 1
};

typedef union {
  void* raw;
  int8_t* i8;
  uint8_t* u8;
  int16_t* i16;
  uint16_t* u16;
  int32_t* i32;
  uint32_t* u32;
  float* f32;
} DataPointer;

#ifndef LOVR_UNCHECKED
#define luax_fieldcheck(L, cond, index, field, arr, single) if (!(cond)) luax_fielderror(L, index, field, arr, single)
#else
#define luax_fieldcheck(L, cond, index, field, arr, single) ((void) 0)
#endif

// "single" says whether a vector can be given as numbers or whether it must be a single table/udata
static void luax_fielderror(lua_State* L, int index, const DataField* field, bool arr, bool single) {
  if (index < 0) index += lua_gettop(L) + 1;

  if (!field->parent) {
    lua_pushliteral(L, "buffer data");
  } else if (!field->name) {
    lua_pushliteral(L, "<anonymous>");
  } else {
    lua_pushstring(L, field->name);
    DataField* parent = field->parent;
    while (parent && parent->name) {
      if (parent->length > 0) {
        lua_pushfstring(L, "%s[]", parent->name);
      } else {
        lua_pushstring(L, parent->name);
      }
      lua_insert(L, -2);
      lua_pushliteral(L, ".");
      lua_insert(L, -2);
      lua_concat(L, 3);
      parent = parent->parent;
    }
    lua_pushliteral(L, "'");
    lua_insert(L, -2);
    lua_pushliteral(L, "'");
    lua_concat(L, 3);
  }

  const char* kind;
  const char* expected;
  if (arr && field->length > 0) {
    kind = "array";
    expected = "table";
  } else if (field->fieldCount > 0) {
    kind = "struct";
    expected = "table";
  } else if (field->type >= TYPE_MAT2 && field->type <= TYPE_MAT4) {
    kind = "matrix";
    expected = single ? "table or Mat4" : "number, table, or Mat4";
  } else if (typeComponents[field->type] > 1) {
    kind = "vector";
    expected = single ? "table" : "number or table";
  } else {
    kind = "scalar";
    expected = "number";
  }

  const char* name = lua_tostring(L, -1);
  const char* typename = luaL_typename(L, index);
  luaL_error(L, "Bad type for %s %s: %s expected, got %s", kind, name, expected, typename);
}

static void luax_checkfieldn(lua_State* L, int index, const DataField* field, void* data) {
  DataPointer p = { .raw = data };
  for (uint32_t i = 0; i < typeComponents[field->type]; i++) {
    double x = lua_tonumber(L, index + i);
    switch (field->type) {
      case TYPE_I8x4: p.i8[i] = (int8_t) x; break;
      case TYPE_U8x4: p.u8[i] = (uint8_t) x; break;
      case TYPE_SN8x4: p.i8[i] = (int8_t) CLAMP(x, -1.f, 1.f) * INT8_MAX; break;
      case TYPE_UN8x4: p.u8[i] = (uint8_t) CLAMP(x, 0.f, 1.f) * UINT8_MAX; break;
      case TYPE_SN10x3: p.u32[0] |= (((uint32_t) (int32_t) (CLAMP(x, -1.f, 1.f) * 511.f)) & 0x3ff) << (10 * i); break;
      case TYPE_UN10x3: p.u32[0] |= (((uint32_t) (CLAMP(x, 0.f, 1.f) * 1023.f)) & 0x3ff) << (10 * i); break;
      case TYPE_I16: p.i16[i] = (int16_t) x; break;
      case TYPE_I16x2: p.i16[i] = (int16_t) x; break;
      case TYPE_I16x4: p.i16[i] = (int16_t) x; break;
      case TYPE_U16: p.u16[i] = (uint16_t) x; break;
      case TYPE_U16x2: p.u16[i] = (uint16_t) x; break;
      case TYPE_U16x4: p.u16[i] = (uint16_t) x; break;
      case TYPE_SN16x2: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
      case TYPE_SN16x4: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
      case TYPE_UN16x2: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
      case TYPE_UN16x4: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
      case TYPE_I32: p.i32[i] = (int32_t) x; break;
      case TYPE_I32x2: p.i32[i] = (int32_t) x; break;
      case TYPE_I32x3: p.i32[i] = (int32_t) x; break;
      case TYPE_I32x4: p.i32[i] = (int32_t) x; break;
      case TYPE_U32: p.u32[i] = (uint32_t) x; break;
      case TYPE_U32x2: p.u32[i] = (uint32_t) x; break;
      case TYPE_U32x3: p.u32[i] = (uint32_t) x; break;
      case TYPE_U32x4: p.i32[i] = (uint32_t) x; break;
      case TYPE_F16x2: p.u16[i] = float32to16(x); break;
      case TYPE_F16x4: p.u16[i] = float32to16(x); break;
      case TYPE_F32: p.f32[i] = (float) x; break;
      case TYPE_F32x2: p.f32[i] = (float) x; break;
      case TYPE_F32x3: p.f32[i] = (float) x; break;
      case TYPE_F32x4: p.f32[i] = (float) x; break;
      case TYPE_MAT2: p.f32[i] = (float) x; break;
      case TYPE_MAT3: p.f32[4 * i / 3 + i % 3] = (float) x; break;
      case TYPE_MAT4: p.f32[i] = (float) x; break;
      case TYPE_INDEX16: p.u16[i] = (uint16_t) x - 1; break;
      case TYPE_INDEX32: p.u32[i] = (uint32_t) x - 1; break;
      default: lovrUnreachable();
    }
  }
}

static void luax_checkfieldv(lua_State* L, int index, const DataField* field, void* data, bool single) {
  DataPointer p = { .raw = data };
  Mat4* matrix = luax_totype(L, index, Mat4);
  luax_fieldcheck(L, matrix && field->type >= TYPE_MAT2 && field->type <= TYPE_MAT4, index, field, false, single);
  float* m = lovrMat4GetPointer(matrix);
  switch (field->type) {
    case TYPE_MAT2: for (int i = 0; i < 2; i++) memcpy(p.f32 + 2 * i, m + 4 * i, 2 * sizeof(float)); break;
    case TYPE_MAT3: for (int i = 0; i < 3; i++) memcpy(p.f32 + 4 * i, m + 4 * i, 3 * sizeof(float)); break;
    case TYPE_MAT4: memcpy(data, m, 16 * sizeof(float)); break;
    default: lovrUnreachable();
  }
}

static void luax_checkfieldt(lua_State* L, int index, const DataField* field, void* data) {
  if (index < 0) index += lua_gettop(L) + 1;
  int n = typeComponents[field->type];
  for (int i = 1; i <= n; i++) {
    lua_rawgeti(L, index, i);
  }
  luax_checkfieldn(L, -n, field, data);
  lua_pop(L, n);
}

static void luax_checkstruct(lua_State* L, int index, const DataField* structure, char* data) {
  luax_fieldcheck(L, lua_istable(L, index), index, structure, false, true);
  if (index < 0) index += lua_gettop(L) + 1;
  uint32_t length = luax_len(L, index);
  uint32_t f = 0;

  // Number keys
  for (uint32_t i = 1; i <= length && f < structure->fieldCount; f++) {
    lua_rawgeti(L, index, i);
    const DataField* field = &structure->fields[f];
    if (field->length == 0 && field->fieldCount == 0 && lua_type(L, -1) == LUA_TNUMBER) {
      int n = typeComponents[field->type];
      for (int c = 1; c < n; c++) lua_rawgeti(L, index, i + c);
      luax_checkfieldn(L, -n, field, data + field->offset);
      lua_pop(L, n);
      i += n;
    } else {
      luax_checkbufferdata(L, -1, field, data + field->offset, false);
      lua_pop(L, 1);
      i++;
    }
  }

  // String keys
  while (f < structure->fieldCount && structure->fields[f].name) {
    const DataField* field = &structure->fields[f++];
    lua_getfield(L, index, field->name);

    if (lua_isnil(L, -1)) {
      memset(data + field->offset, 0, MAX(field->length, 1) * field->stride);
    } else {
      luax_checkbufferdata(L, -1, field, data + field->offset, true);
    }

    lua_pop(L, 1);
  }
}

static void luax_checkarray(lua_State* L, int index, int start, uint32_t count, const DataField* array, char* data) {
  luax_fieldcheck(L, lua_istable(L, index), index, array, true, true);
  uint32_t length = luax_len(L, index);
  count = MIN(count, (length - start + 1));

  if (array->fieldCount > 0) {
    for (uint32_t i = 0; i < count; i++, data += array->stride) {
      lua_rawgeti(L, index, start + i);
      luax_checkstruct(L, -1, array, data);
      lua_pop(L, 1);
    }
  } else {
    lua_rawgeti(L, index, start);
    int type = lua_type(L, -1);
    lua_pop(L, 1);

    if (type == LUA_TNUMBER) {
      if (index < 0) index += lua_gettop(L) + 1;
      uint32_t n = typeComponents[array->type];
      count = MIN(count, (length - start + 1) / n);
      for (uint32_t i = 0; i < count; i += n, data += array->stride) {
        for (uint32_t j = 0; j < n; j++) {
          lua_rawgeti(L, index, start + i + j);
        }
        luax_checkfieldn(L, -n, array, data);
        lua_pop(L, n);
      }
    } else if (type == LUA_TUSERDATA) {
      for (uint32_t i = 0; i < count; i++, data += array->stride) {
        lua_rawgeti(L, index, start + i);
        luax_checkfieldv(L, -1, array, data, true);
        lua_pop(L, 1);
      }
    } else if (type == LUA_TTABLE) {
      for (uint32_t i = 0; i < count; i++, data += array->stride) {
        lua_rawgeti(L, index, start + i);
        luax_checkfieldt(L, -1, array, data);
        lua_pop(L, 1);
      }
    } else {
      lua_rawgeti(L, index, start);
      luax_fieldcheck(L, type == LUA_TNIL, -1, array, false, false);
      lua_pop(L, 1);
    }
  }
}

void luax_checkbufferdata(lua_State* L, int index, const DataField* field, char* data, bool single) {
  int type = lua_type(L, index);

  if (field->length > 0) {
    luax_checkarray(L, index, 1, field->length, field, data);
  } else if (field->fieldCount > 0) {
    luax_checkstruct(L, index, field, data);
  } else if (typeComponents[field->type] == 1) {
    luax_fieldcheck(L, lua_type(L, index) == LUA_TNUMBER, index, field, false, true);
    luax_checkfieldn(L, index, field, data);
  } else if (type == LUA_TUSERDATA) {
    luax_checkfieldv(L, index, field, data, single);
  } else if (type == LUA_TTABLE) {
    luax_checkfieldt(L, index, field, data);
  } else {
    luax_fielderror(L, index, field, false, single);
  }
}

static int luax_pushfieldn(lua_State* L, DataType type, char* data) {
  DataPointer p = { .raw = data };
  switch (type) {
    case TYPE_I8x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.i8[i]); return 4;
    case TYPE_U8x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.u8[i]); return 4;
    case TYPE_SN8x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, MAX((float) p.i8[i] / 127, -1.f)); return 4;
    case TYPE_UN8x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, (float) p.u8[i] / 255); return 4;
    case TYPE_SN10x3: for (int i = 0; i < 3; i++) lua_pushnumber(L, (float) ((p.i32[0] >> (10 * i)) & 0x3ff) / 511.f); return 3;
    case TYPE_UN10x3: for (int i = 0; i < 3; i++) lua_pushnumber(L, (float) ((p.u32[0] >> (10 * i)) & 0x3ff) / 1023.f); return 3;
    case TYPE_I16x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.i16[i]); return 2;
    case TYPE_I16x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.i16[i]); return 4;
    case TYPE_U16x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.u16[i]); return 2;
    case TYPE_U16x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.u16[i]); return 4;
    case TYPE_SN16x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, MAX((float) p.i16[i] / 32767, -1.f)); return 2;
    case TYPE_SN16x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, MAX((float) p.i16[i] / 32767, -1.f)); return 4;
    case TYPE_UN16x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, (float) p.u16[i] / 65535); return 2;
    case TYPE_UN16x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, (float) p.u16[i] / 65535); return 4;
    case TYPE_I32: lua_pushinteger(L, p.i32[0]); return 1;
    case TYPE_I32x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.i32[i]); return 2;
    case TYPE_I32x3: for (int i = 0; i < 3; i++) lua_pushinteger(L, p.i32[i]); return 3;
    case TYPE_I32x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.i32[i]); return 4;
    case TYPE_U32: lua_pushinteger(L, p.u32[0]); return 1;
    case TYPE_U32x2: for (int i = 0; i < 2; i++) lua_pushinteger(L, p.u32[i]); return 2;
    case TYPE_U32x3: for (int i = 0; i < 3; i++) lua_pushinteger(L, p.u32[i]); return 3;
    case TYPE_U32x4: for (int i = 0; i < 4; i++) lua_pushinteger(L, p.u32[i]); return 4;
    case TYPE_F16x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, float16to32(p.u16[i])); return 2;
    case TYPE_F16x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, float16to32(p.u16[i])); return 4;
    case TYPE_F32: lua_pushnumber(L, p.f32[0]); return 1;
    case TYPE_F32x2: for (int i = 0; i < 2; i++) lua_pushnumber(L, p.f32[i]); return 2;
    case TYPE_F32x3: for (int i = 0; i < 3; i++) lua_pushnumber(L, p.f32[i]); return 3;
    case TYPE_F32x4: for (int i = 0; i < 4; i++) lua_pushnumber(L, p.f32[i]); return 4;
    case TYPE_MAT2: for (int i = 0; i < 4; i++) lua_pushnumber(L, p.f32[i]); return 4;
    case TYPE_MAT3: for (int i = 0; i < 9; i++) lua_pushnumber(L, p.f32[4 * i / 3 + i % 3]); return 9;
    case TYPE_MAT4: for (int i = 0; i < 16; i++) lua_pushnumber(L, p.f32[i]); return 16;
    case TYPE_INDEX16: lua_pushinteger(L, p.u16[0] + 1); return 1;
    case TYPE_INDEX32: lua_pushinteger(L, p.u32[0] + 1); return 1;
    default: lovrUnreachable(); return 0;
  }
}

int luax_pushbufferdata(lua_State* L, const DataField* format, uint32_t count, char* data) {
  if (format->length > 0 && count > 0) {
    lua_createtable(L, count, 0);
    if (format->fieldCount > 0) {
      for (uint32_t i = 0; i < count; i++, data += format->stride) {
        luax_pushbufferdata(L, format, 0, data);
        lua_rawseti(L, -2, i + 1);
      }
    } else {
      int n = typeComponents[format->type];
      for (uint32_t i = 0; i < count; i++, data += format->stride) {
        luax_pushfieldn(L, format->type, data);
        for (int c = 0; c < n; c++) {
          lua_rawseti(L, -1 - n + c, i * n + c + 1);
        }
      }
    }
    return 1;
  } else if (format->fieldCount > 0) {
    lua_createtable(L, 0, format->fieldCount);
    for (uint32_t f = 0; f < format->fieldCount; f++) {
      const DataField* field = &format->fields[f];
      if (field->length > 0) {
        luax_pushbufferdata(L, field, field->length, data + field->offset);
      } else if (field->fieldCount > 0) {
        luax_pushbufferdata(L, field, 0, data + field->offset);
      } else {
        uint32_t n = typeComponents[field->type];
        if (n > 1) {
          lua_createtable(L, n, 0);
          luax_pushfieldn(L, field->type, data + field->offset);
          for (uint32_t c = 0; c < n; c++) {
            lua_rawseti(L, -1 - n + c, n - c);
          }
        } else {
          luax_pushfieldn(L, field->type, data + field->offset);
        }
      }
      if (field->name) {
        lua_setfield(L, -2, field->name);
      } else {
        lua_rawseti(L, -2, f + 1);
      }
    }
    return 1;
  } else {
    return luax_pushfieldn(L, format->type, data);
  }
}

int luax_gettablestride(lua_State* L, int type) {
  return typeComponents[type];
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->size);
  return 1;
}

static int l_lovrBufferGetLength(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  if (format) {
    lua_pushinteger(L, format->length);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  if (format) {
    lua_pushinteger(L, format->stride);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

void luax_pushbufferformat(lua_State* L, const DataField* fields, uint32_t count) {
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    const DataField* field = &fields[i];
    lua_newtable(L);
    lua_pushstring(L, field->name);
    lua_setfield(L, -2, "name");
    if (field->fieldCount > 0) {
      luax_pushbufferformat(L, field->fields, field->fieldCount);
    } else {
      luax_pushenum(L, DataType, field->type);
    }
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, field->offset);
    lua_setfield(L, -2, "offset");
    if (field->length > 0) {
      if (field->length == ~0u) {
        lua_pushinteger(L, -1);
      } else {
        lua_pushinteger(L, field->length);
      }
      lua_setfield(L, -2, "length");
      lua_pushinteger(L, field->stride);
      lua_setfield(L, -2, "stride");
    }
    lua_rawseti(L, -2, i + 1);
  }
}

static int l_lovrBufferGetFormat(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  if (format) {
    if (format->fieldCount > 0) {
      luax_pushbufferformat(L, format->fields, format->fieldCount);
    } else {
      luax_pushbufferformat(L, format, 1);
    }
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrBufferNewReadback(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  Readback* readback = lovrReadbackCreateBuffer(buffer, offset, extent);
  luax_pushtype(L, Readback, readback);
  lovrRelease(readback, lovrReadbackDestroy);
  return 1;
}

static int l_lovrBufferGetData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const DataField* format = lovrBufferGetInfo(buffer)->format;
  lovrCheck(format, "Buffer:getData requires the Buffer to have a format");
  if (format->length > 0) {
    uint32_t index = luax_optu32(L, 2, 1) - 1;
    lovrCheck(index < format->length, "Buffer:getData index exceeds the Buffer's length");
    uint32_t count = luax_optu32(L, 3, format->length - index);
    void* data = lovrBufferGetData(buffer, index * format->stride, count * format->stride);
    return luax_pushbufferdata(L, format, count, data);
  } else {
    void* data = lovrBufferGetData(buffer, 0, format->stride);
    return luax_pushbufferdata(L, format, 0, data);
  }
}

static int l_lovrBufferSetData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  const DataField* format = info->format;

  Blob* blob = luax_totype(L, 2, Blob);

  if (blob) {
    uint32_t dstOffset = luax_optu32(L, 3, 0);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    lovrCheck(dstOffset < info->size, "Buffer offset is bigger than the size of the Buffer");
    lovrCheck(srcOffset < blob->size, "Blob offset is bigger than the size of the Blob");
    uint32_t limit = (uint32_t) MIN(info->size - dstOffset, blob->size - srcOffset);
    uint32_t extent = luax_optu32(L, 5, limit);
    lovrCheck(extent <= info->size - dstOffset, "Buffer copy range exceeds the size of the target Buffer");
    lovrCheck(extent <= blob->size - srcOffset, "Buffer copy range exceeds the size of the source Blob");
    void* data = lovrBufferSetData(buffer, dstOffset, extent);
    memcpy(data, (char*) blob->data + srcOffset, extent);
    return 0;
  }

  Buffer* src = luax_totype(L, 2, Buffer);

  if (src) {
    Buffer* dst = buffer;
    uint32_t dstOffset = luax_optu32(L, 3, 0);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    const BufferInfo* dstInfo = info;
    const BufferInfo* srcInfo = lovrBufferGetInfo(src);
    uint32_t limit = MIN(dstInfo->size - dstOffset, srcInfo->size - srcOffset);
    uint32_t extent = luax_optu32(L, 5, limit);
    lovrBufferCopy(src, dst, srcOffset, dstOffset, extent);
    return 0;
  }

  if (format) {
    if (format->length > 0) {
      luax_fieldcheck(L, lua_istable(L, 2), 2, format, -1, true);
      uint32_t length = luax_len(L, 2);
      uint32_t dstIndex = luax_optu32(L, 3, 1) - 1;
      uint32_t srcIndex = luax_optu32(L, 4, 1) - 1;

      lua_rawgeti(L, 2, srcIndex + 1);
      uint32_t tstride = format->fieldCount == 0 && lua_type(L, -1) == LUA_TNUMBER ? typeComponents[format->type] : 1;
      lua_pop(L, 1);

      uint32_t limit = MIN(format->length - dstIndex, (length - srcIndex) / tstride);
      uint32_t count = luax_optu32(L, 5, limit);

      char* data = lovrBufferSetData(buffer, dstIndex * format->stride, count * format->stride);
      luax_checkarray(L, 2, srcIndex + 1, count, format, data);
    } else {
      luaL_checkany(L, 2);
      luax_checkbufferdata(L, 2, format, lovrBufferSetData(buffer, 0, format->stride), true);
    }

    return 0;
  }

  return luax_typeerror(L, 2, "Blob or Buffer");
}

static int l_lovrBufferMapData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  void* pointer = lovrBufferSetData(buffer, offset, extent);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferClear(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  uint32_t value = (uint32_t) luaL_optinteger(L, 4, 0);
  lovrBufferClear(buffer, offset, extent, value);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "getSize", l_lovrBufferGetSize },
  { "getLength", l_lovrBufferGetLength },
  { "getStride", l_lovrBufferGetStride },
  { "getFormat", l_lovrBufferGetFormat },
  { "newReadback", l_lovrBufferNewReadback },
  { "getData", l_lovrBufferGetData },
  { "setData", l_lovrBufferSetData },
  { "mapData", l_lovrBufferMapData },
  { "clear", l_lovrBufferClear },
  { NULL, NULL }
};
