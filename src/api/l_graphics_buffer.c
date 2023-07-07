#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static const uint32_t vectorComponents[MAX_VECTOR_TYPES] = {
  [V_VEC2] = 2,
  [V_VEC3] = 3,
  [V_VEC4] = 4,
  [V_QUAT] = 4,
  [V_MAT4] = 16
};

Buffer* luax_checkbuffer(lua_State* L, int index) {
  Buffer* buffer = luax_checktype(L, index, Buffer);
  lovrCheck(lovrBufferIsValid(buffer), "Buffers created with getBuffer can only be used for a single frame (unable to use this Buffer again because lovr.graphics.submit has been called since it was created)");
  return buffer;
}

static const uint32_t fieldComponents[] = {
  [TYPE_I8x4] = 4,
  [TYPE_U8x4] = 4,
  [TYPE_SN8x4] = 4,
  [TYPE_UN8x4] = 4,
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
} FieldPointer;

static void luax_tofield(lua_State* L, int index, DataType type, void* data) {
  FieldPointer p = { .raw = data };
  if (lua_isuserdata(L, index)) {
    VectorType vectorType;
    float* v = luax_tovector(L, index, &vectorType);
    lovrCheck(vectorComponents[vectorType] == fieldComponents[type], "Vector type is incompatible with field type (expected %d components, got %d)", fieldComponents[type], vectorComponents[vectorType]);
    switch (type) {
      case TYPE_I8x4: for (int i = 0; i < 4; i++) p.i8[i] = (int8_t) v[i]; break;
      case TYPE_U8x4: for (int i = 0; i < 4; i++) p.u8[i] = (uint8_t) v[i]; break;
      case TYPE_SN8x4: for (int i = 0; i < 4; i++) p.i8[i] = (int8_t) CLAMP(v[i], -1.f, 1.f) * INT8_MAX; break;
      case TYPE_UN8x4: for (int i = 0; i < 4; i++) p.u8[i] = (uint8_t) CLAMP(v[i], 0.f, 1.f) * UINT8_MAX; break;
      case TYPE_UN10x3: for (int i = 0; i < 3; i++) p.u32[0] |= (uint32_t) (CLAMP(v[i], 0.f, 1.f) * 1023.f) << (10 * (2 - i)); break;
      case TYPE_I16x2: for (int i = 0; i < 2; i++) p.i16[i] = (int16_t) v[i]; break;
      case TYPE_I16x4: for (int i = 0; i < 4; i++) p.i16[i] = (int16_t) v[i]; break;
      case TYPE_U16x2: for (int i = 0; i < 2; i++) p.u16[i] = (uint16_t) v[i]; break;
      case TYPE_U16x4: for (int i = 0; i < 4; i++) p.u16[i] = (uint16_t) v[i]; break;
      case TYPE_SN16x2: for (int i = 0; i < 2; i++) p.i16[i] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
      case TYPE_SN16x4: for (int i = 0; i < 4; i++) p.i16[i] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
      case TYPE_UN16x2: for (int i = 0; i < 2; i++) p.u16[i] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
      case TYPE_UN16x4: for (int i = 0; i < 4; i++) p.u16[i] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
      case TYPE_I32x2: for (int i = 0; i < 2; i++) p.i32[i] = (int32_t) v[i]; break;
      case TYPE_I32x3: for (int i = 0; i < 3; i++) p.i32[i] = (int32_t) v[i]; break;
      case TYPE_I32x4: for (int i = 0; i < 4; i++) p.i32[i] = (int32_t) v[i]; break;
      case TYPE_U32x2: for (int i = 0; i < 2; i++) p.u32[i] = (uint32_t) v[i]; break;
      case TYPE_U32x3: for (int i = 0; i < 3; i++) p.u32[i] = (uint32_t) v[i]; break;
      case TYPE_U32x4: for (int i = 0; i < 4; i++) p.u32[i] = (uint32_t) v[i]; break;
      case TYPE_F16x2: for (int i = 0; i < 2; i++) p.u16[i] = float32to16(v[i]); break;
      case TYPE_F16x4: for (int i = 0; i < 4; i++) p.u16[i] = float32to16(v[i]); break;
      case TYPE_F32x2: memcpy(data, v, 2 * sizeof(float)); break;
      case TYPE_F32x3: memcpy(data, v, 3 * sizeof(float)); break;
      case TYPE_F32x4: memcpy(data, v, 4 * sizeof(float)); break;
      case TYPE_MAT4: memcpy(data, v, 16 * sizeof(float)); break;
      default: lovrUnreachable();
    }
  } else {
    for (uint32_t i = 0; i < fieldComponents[type]; i++) {
      double x = lua_tonumber(L, index + i);
      switch (type) {
        case TYPE_I8x4: p.i8[i] = (int8_t) x; break;
        case TYPE_U8x4: p.u8[i] = (uint8_t) x; break;
        case TYPE_SN8x4: p.i8[i] = (int8_t) CLAMP(x, -1.f, 1.f) * INT8_MAX; break;
        case TYPE_UN8x4: p.u8[i] = (uint8_t) CLAMP(x, 0.f, 1.f) * UINT8_MAX; break;
        case TYPE_UN10x3: p.u32[0] |= (uint32_t) (CLAMP(x, 0.f, 1.f) * 1023.f) << (10 * (2 - i)); break;
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
        case TYPE_MAT3: p.f32[i] = (float) x; break;
        case TYPE_MAT4: p.f32[i] = (float) x; break;
        case TYPE_INDEX16: p.u16[i] = (uint16_t) x - 1; break;
        case TYPE_INDEX32: p.u32[i] = (uint32_t) x - 1; break;
        default: lovrUnreachable();
      }
    }
  }
}

static void luax_checkstruct(lua_State* L, int index, const DataField* field, char* data) {
  index = index > 0 ? index : lua_gettop(L) + 1 + index;

  if (!lua_istable(L, index)) {
    if (field->childCount == 1) {
      luax_checkbufferdata(L, index, field->children, data + field->children->offset);
      return;
    } else {
      lovrThrow("Expected table for struct data");
    }
  }

  if (!field->children[0].name || luax_len(L, index) > 0) {
    for (uint32_t i = 0, j = 1; i < field->childCount; i++) {
      const DataField* child = &field->children[i];
      int n = 1;

      lua_rawgeti(L, index, j);
      if (child->length == 0 && child->childCount == 0 && lua_type(L, -1) == LUA_TNUMBER) {
        for (uint32_t c = fieldComponents[child->type]; c > 1; c--, n++) {
          lua_rawgeti(L, index, j + n);
        }
      }

      luax_checkbufferdata(L, -n, child, data + child->offset);
      lua_pop(L, n);
      j += n;
    }
  } else {
    for (uint32_t i = 0; i < field->childCount; i++) {
      const DataField* child = &field->children[i];
      lua_pushstring(L, child->name);
      lua_rawget(L, index);
      luax_checkbufferdata(L, -1, child, data + child->offset);
      lua_pop(L, 1);
    }
  }
}

static void luax_checkarray(lua_State* L, int index, uint32_t offset, uint32_t count, const DataField* field, char* data) {
  lovrCheck(lua_istable(L, index), "Expected table for array data");

  if (field->childCount > 0) {
    for (uint32_t i = 0; i < count; i++, data += field->stride) {
      lua_rawgeti(L, index, i + offset + 1);
      luax_checkstruct(L, -1, field, data);
      lua_pop(L, 1);
    }
  } else {
    int n = fieldComponents[field->type];

    lua_rawgeti(L, index, 1);
    int type = lua_type(L, -1);
    lua_pop(L, 1);

    if (type == LUA_TUSERDATA || type == LUA_TLIGHTUSERDATA) {
      for (uint32_t i = 0; i < count; i++, data += field->stride) {
        lua_rawgeti(L, index, i + offset + 1);
        int type = lua_type(L, -1);
        if (type == LUA_TUSERDATA || type == LUA_TLIGHTUSERDATA) {
          luax_tofield(L, -1, field->type, data);
        } else if (type == LUA_TNIL) {
          break;
        } else {
          lovrThrow("Expected vector object for array value (arrays must use the same type for all elements)");
        }
        lua_pop(L, 1);
      }
    } else if (type == LUA_TNUMBER) {
      index = index > 0 ? index : lua_gettop(L) + 1 + index;
      for (uint32_t i = 0; i < count; i++, data += field->stride) {
        for (int c = 1; c <= n; c++) {
          lua_rawgeti(L, index, i * n + offset + c);
        }
        luax_tofield(L, -n, field->type, data);
        lua_pop(L, n);
      }
    } else if (type == LUA_TTABLE) {
      for (uint32_t i = 0; i < count; i++, data += field->stride) {
        lua_rawgeti(L, index, i + offset + 1);
        lovrCheck(lua_istable(L, -1), "Expected nested table for array value (arrays must use the same type for all elements)");
        for (int c = 1, j = -1; c <= n; c++, j--) {
          lua_rawgeti(L, j, c);
        }
        luax_tofield(L, -n, field->type, data);
        lua_pop(L, n + 1);
      }
    } else {
      lovrThrow("Expected number, table, or vector for array contents");
    }
  }
}

void luax_checkbufferdata(lua_State* L, int index, const DataField* field, char* data) {
  if (field->length > 0) {
    luax_checkarray(L, index, 0, field->length, field, data);
  } else if (field->childCount > 0) {
    luax_checkstruct(L, index, field, data);
  } else if (lua_type(L, index) == LUA_TTABLE) {
    int n = fieldComponents[field->type];
    for (int c = 0; c < n; c++) {
      lua_rawgeti(L, index < 0 ? index - c : index, c + 1);
    }
    luax_tofield(L, -n, field->type, data);
    lua_pop(L, n);
  } else {
    luax_tofield(L, index, field->type, data);
  }
}

static int luax_pushcomponents(lua_State* L, const DataField* field, char* data) {
  FieldPointer p = { .raw = data };
  int n = (int) fieldComponents[field->type];
  switch (field->type) {
    case TYPE_I8x4: for (int i = 0; i < n; i++) lua_pushinteger(L, p.i8[i]); return n;
    case TYPE_U8x4: for (int i = 0; i < n; i++) lua_pushinteger(L, p.u8[i]); return n;
    case TYPE_SN8x4: for (int i = 0; i < n; i++) lua_pushnumber(L, MAX((float) p.i8[i] / 127, -1.f)); return n;
    case TYPE_UN8x4: for (int i = 0; i < n; i++) lua_pushnumber(L, (float) p.u8[i] / 255); return n;
    case TYPE_UN10x3: for (int i = 0; i < n; i++) lua_pushnumber(L, (float) ((p.u32[0] >> (10 * (2 - i))) & 0x3ff) / 1023.f); return n;
    case TYPE_I16x2: for (int i = 0; i < n; i++) lua_pushinteger(L, p.i16[i]); return n;
    case TYPE_I16x4: for (int i = 0; i < n; i++) lua_pushinteger(L, p.i16[i]); return n;
    case TYPE_U16x2: for (int i = 0; i < n; i++) lua_pushinteger(L, p.u16[i]); return n;
    case TYPE_U16x4: for (int i = 0; i < n; i++) lua_pushinteger(L, p.u16[i]); return n;
    case TYPE_SN16x2: for (int i = 0; i < n; i++) lua_pushnumber(L, MAX((float) p.i16[i] / 32767, -1.f)); return n;
    case TYPE_SN16x4: for (int i = 0; i < n; i++) lua_pushnumber(L, MAX((float) p.i16[i] / 32767, -1.f)); return n;
    case TYPE_UN16x2: for (int i = 0; i < n; i++) lua_pushnumber(L, (float) p.u16[i] / 65535); return n;
    case TYPE_UN16x4: for (int i = 0; i < n; i++) lua_pushnumber(L, (float) p.u16[i] / 65535); return n;
    case TYPE_I32: lua_pushinteger(L, p.i32[0]); return n;
    case TYPE_I32x2: for (int i = 0; i < n; i++) lua_pushinteger(L, p.i32[i]); return n;
    case TYPE_I32x3: for (int i = 0; i < n; i++) lua_pushinteger(L, p.i32[i]); return n;
    case TYPE_I32x4: for (int i = 0; i < n; i++) lua_pushinteger(L, p.i32[i]); return n;
    case TYPE_U32: lua_pushinteger(L, p.u32[0]); return n;
    case TYPE_U32x2: for (int i = 0; i < n; i++) lua_pushinteger(L, p.u32[i]); return n;
    case TYPE_U32x3: for (int i = 0; i < n; i++) lua_pushinteger(L, p.u32[i]); return n;
    case TYPE_U32x4: for (int i = 0; i < n; i++) lua_pushinteger(L, p.u32[i]); return n;
    case TYPE_F16x2: for (int i = 0; i < n; i++) lua_pushnumber(L, float16to32(p.u16[i])); return n;
    case TYPE_F16x4: for (int i = 0; i < n; i++) lua_pushnumber(L, float16to32(p.u16[i])); return n;
    case TYPE_F32: lua_pushnumber(L, p.f32[0]); return n;
    case TYPE_F32x2: for (int i = 0; i < n; i++) lua_pushnumber(L, p.f32[i]); return n;
    case TYPE_F32x3: for (int i = 0; i < n; i++) lua_pushnumber(L, p.f32[i]); return n;
    case TYPE_F32x4: for (int i = 0; i < n; i++) lua_pushnumber(L, p.f32[i]); return n;
    case TYPE_MAT2: for (int i = 0; i < n; i++) lua_pushnumber(L, p.f32[i]); return n;
    case TYPE_MAT3: for (int i = 0; i < n; i++) lua_pushnumber(L, p.f32[i]); return n;
    case TYPE_MAT4: for (int i = 0; i < n; i++) lua_pushnumber(L, p.f32[i]); return n;
    case TYPE_INDEX16: lua_pushinteger(L, p.u16[0] + 1); return n;
    case TYPE_INDEX32: lua_pushinteger(L, p.u32[0] + 1); return n;
    default: lovrUnreachable(); return 0;
  }
}

static int luax_pushstruct(lua_State* L, const DataField* field, char* data) {
  lua_createtable(L, 0, field->childCount);
  for (uint32_t i = 0; i < field->childCount; i++) {
    if (field->childCount > 0 || field->length > 0 || fieldComponents[field->type] == 1) {
      luax_pushbufferdata(L, &field->children[i], data + field->children[i].offset);
    } else {
      int n = fieldComponents[field->type];
      lua_createtable(L, n, 0);
      luax_pushbufferdata(L, &field->children[i], data + field->children[i].offset);
      for (int j = n + 1, k = n; k >= 1; k++, j--) {
        lua_rawseti(L, -j, k);
      }
    }
    lua_setfield(L, -2, field->children[i].name);
  }
  return 1;
}

int luax_pushbufferdata(lua_State* L, const DataField* field, char* data) {
  if (field->length > 0) {
    lua_createtable(L, field->length, 0);
    if (field->childCount > 0) {
      for (uint32_t i = 0; i < field->length; i++) {
        luax_pushstruct(L, &field->children[i], data);
        lua_rawseti(L, -2, i + 1);
        data += field->stride;
      }
    } else {
      for (uint32_t i = 0; i < field->length; i++) {
        int n = (int) fieldComponents[field->type];
        if (n > 1) {
          lua_createtable(L, n, 0);
          luax_pushcomponents(L, field, data);
          for (int j = n + 1, k = n; k >= 1; k--, j--) {
            lua_rawseti(L, -j, k);
          }
        } else {
          luax_pushcomponents(L, field, data);
        }
        lua_rawseti(L, -2, i + 1);
        data += field->stride;
      }
    }
    return 1;
  } else if (field->childCount > 0) {
    return luax_pushstruct(L, field, data);
  } else {
    return luax_pushcomponents(L, field, data);
  }
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->size);
  return 1;
}

static int l_lovrBufferGetLength(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t length = info->format ? info->format->length : 0;
  lua_pushinteger(L, length);
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t stride = info->format && info->format->length > 0 ? info->format->stride : 0;
  lua_pushinteger(L, stride);
  return 1;
}

static void luax_pushbufferformat(lua_State* L, const DataField* format, uint32_t count, bool root) {
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    const DataField* field = &format[i];
    lua_newtable(L);
    if (field->name) {
      lua_pushstring(L, field->name);
      lua_setfield(L, -2, "name");
    }
    if (field->location != ~0u) {
      lua_pushinteger(L, field->location);
      lua_setfield(L, -2, "location");
    }
    if (field->childCount > 0) {
      luax_pushbufferformat(L, field->children, field->childCount, false);
    } else {
      luax_pushenum(L, DataType, field->type);
    }
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, field->offset);
    lua_setfield(L, -2, "offset");
    if (field->length > 0 && !root) {
      lua_pushinteger(L, field->length);
      lua_setfield(L, -2, "length");
      lua_pushinteger(L, field->stride);
      lua_setfield(L, -2, "stride");
    }
    lua_rawseti(L, -2, i + 1);
  }
}

static int l_lovrBufferGetFormat(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  if (info->fieldCount == 0) {
    lua_pushnil(L);
  } else if (info->format->childCount > 0) {
    luax_pushbufferformat(L, info->format->children, info->format->childCount, true);
  } else {
    luax_pushbufferformat(L, info->format, 1, true);
  }
  return 1;
}

static int l_lovrBufferGetPointer(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  void* pointer = lovrBufferSetData(buffer, 0, ~0u);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferIsTemporary(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  bool temporary = lovrBufferIsTemporary(buffer);
  lua_pushboolean(L, temporary);
  return 1;
}

static int l_lovrBufferNewReadback(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  Readback* readback = lovrReadbackCreateBuffer(buffer, offset, extent);
  luax_pushtype(L, Readback, readback);
  lovrRelease(readback, lovrReadbackDestroy);
  return 1;
}

static int l_lovrBufferGetData(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  void* data = lovrBufferGetData(buffer, 0, info->size);
  return luax_pushbufferdata(L, info->format, data);
}

static int l_lovrBufferSetData(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);

  if (lua_istable(L, 2)) {
    lovrCheck(info->format, "Buffer must be created with format information to copy a table to it");

    if (info->format->length == 0) {
      void* data = lovrBufferSetData(buffer, 0, info->size);
      luax_checkbufferdata(L, 2, info->format, data);
    } else {
      lua_rawgeti(L, 2, 1);
      bool nested = lua_istable(L, -1);
      lua_pop(L, 1);

      const DataField* array = info->format;
      uint32_t tableLength = luax_len(L, 2);
      uint32_t dstIndex = luax_optu32(L, 3, 1) - 1;
      uint32_t srcIndex = luax_optu32(L, 4, 1) - 1;
      uint32_t limit = nested ? MIN(array->length - dstIndex, tableLength - srcIndex) : array->length - dstIndex;
      uint32_t count = luax_optu32(L, 5, limit);

      lovrCheck(dstIndex + count <= array->length, "Buffer copy range exceeds the length of the target Buffer");
      void* data = lovrBufferSetData(buffer, dstIndex * array->stride, count * array->stride);
      luax_checkarray(L, 2, srcIndex, count, array, data);
    }

    return 0;
  }

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
    const BufferInfo* dstInfo = lovrBufferGetInfo(dst);
    const BufferInfo* srcInfo = lovrBufferGetInfo(src);
    uint32_t limit = MIN(dstInfo->size - dstOffset, srcInfo->size - srcOffset);
    uint32_t extent = luax_optu32(L, 5, limit);
    lovrBufferCopy(src, dst, srcOffset, dstOffset, extent);
    return 0;
  }

  return luax_typeerror(L, 2, "table, Blob, or Buffer");
}

static int l_lovrBufferClear(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, ~0u);
  lovrBufferClear(buffer, offset, extent);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "getSize", l_lovrBufferGetSize },
  { "getLength", l_lovrBufferGetLength },
  { "getStride", l_lovrBufferGetStride },
  { "getFormat", l_lovrBufferGetFormat },
  { "getPointer", l_lovrBufferGetPointer },
  { "isTemporary", l_lovrBufferIsTemporary },
  { "newReadback", l_lovrBufferNewReadback },
  { "getData", l_lovrBufferGetData },
  { "setData", l_lovrBufferSetData },
  { "clear", l_lovrBufferClear },
  { NULL, NULL }
};
