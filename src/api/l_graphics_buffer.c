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
  [FIELD_I8x4] = 4,
  [FIELD_U8x4] = 4,
  [FIELD_SN8x4] = 4,
  [FIELD_UN8x4] = 4,
  [FIELD_UN10x3] = 3,
  [FIELD_I16] = 1,
  [FIELD_I16x2] = 2,
  [FIELD_I16x4] = 4,
  [FIELD_U16] = 1,
  [FIELD_U16x2] = 2,
  [FIELD_U16x4] = 4,
  [FIELD_SN16x2] = 2,
  [FIELD_SN16x4] = 4,
  [FIELD_UN16x2] = 2,
  [FIELD_UN16x4] = 4,
  [FIELD_I32] = 1,
  [FIELD_I32x2] = 2,
  [FIELD_I32x3] = 3,
  [FIELD_I32x4] = 4,
  [FIELD_U32] = 1,
  [FIELD_U32x2] = 2,
  [FIELD_U32x3] = 3,
  [FIELD_U32x4] = 4,
  [FIELD_F16x2] = 2,
  [FIELD_F16x4] = 4,
  [FIELD_F32] = 1,
  [FIELD_F32x2] = 2,
  [FIELD_F32x3] = 3,
  [FIELD_F32x4] = 4,
  [FIELD_MAT2] = 4,
  [FIELD_MAT3] = 9,
  [FIELD_MAT4] = 16,
  [FIELD_INDEX16] = 1,
  [FIELD_INDEX32] = 1
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

static void luax_readcomponents(lua_State* L, int index, FieldType type, void* data) {
  FieldPointer p = { .raw = data };
  if (lua_isuserdata(L, index)) {
    VectorType vectorType;
    float* v = luax_tovector(L, index, &vectorType);
    lovrCheck(vectorComponents[vectorType] == fieldComponents[type], "Vector type is incompatible with field type (expected %d components, got %d)", fieldComponents[type], vectorComponents[vectorType]);
    switch (type) {
      case FIELD_I8x4: for (int i = 0; i < 4; i++) p.i8[i] = (int8_t) v[i]; break;
      case FIELD_U8x4: for (int i = 0; i < 4; i++) p.u8[i] = (uint8_t) v[i]; break;
      case FIELD_SN8x4: for (int i = 0; i < 4; i++) p.i8[i] = (int8_t) CLAMP(v[i], -1.f, 1.f) * INT8_MAX; break;
      case FIELD_UN8x4: for (int i = 0; i < 4; i++) p.u8[i] = (uint8_t) CLAMP(v[i], 0.f, 1.f) * UINT8_MAX; break;
      case FIELD_UN10x3: for (int i = 0; i < 3; i++) p.u32[0] |= (uint32_t) (CLAMP(v[i], 0.f, 1.f) * 1023.f) << (10 * (2 - i)); break;
      case FIELD_I16x2: for (int i = 0; i < 2; i++) p.i16[i] = (int16_t) v[i]; break;
      case FIELD_I16x4: for (int i = 0; i < 4; i++) p.i16[i] = (int16_t) v[i]; break;
      case FIELD_U16x2: for (int i = 0; i < 2; i++) p.u16[i] = (uint16_t) v[i]; break;
      case FIELD_U16x4: for (int i = 0; i < 4; i++) p.u16[i] = (uint16_t) v[i]; break;
      case FIELD_SN16x2: for (int i = 0; i < 2; i++) p.i16[i] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
      case FIELD_SN16x4: for (int i = 0; i < 4; i++) p.i16[i] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
      case FIELD_UN16x2: for (int i = 0; i < 2; i++) p.u16[i] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
      case FIELD_UN16x4: for (int i = 0; i < 4; i++) p.u16[i] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
      case FIELD_I32x2: for (int i = 0; i < 2; i++) p.i32[i] = (int32_t) v[i]; break;
      case FIELD_I32x3: for (int i = 0; i < 3; i++) p.i32[i] = (int32_t) v[i]; break;
      case FIELD_I32x4: for (int i = 0; i < 4; i++) p.i32[i] = (int32_t) v[i]; break;
      case FIELD_U32x2: for (int i = 0; i < 2; i++) p.u32[i] = (uint32_t) v[i]; break;
      case FIELD_U32x3: for (int i = 0; i < 3; i++) p.u32[i] = (uint32_t) v[i]; break;
      case FIELD_U32x4: for (int i = 0; i < 4; i++) p.u32[i] = (uint32_t) v[i]; break;
      case FIELD_F16x2: for (int i = 0; i < 2; i++) p.u16[i] = float32to16(v[i]); break;
      case FIELD_F16x4: for (int i = 0; i < 4; i++) p.u16[i] = float32to16(v[i]); break;
      case FIELD_F32x2: memcpy(data, v, 2 * sizeof(float)); break;
      case FIELD_F32x3: memcpy(data, v, 3 * sizeof(float)); break;
      case FIELD_F32x4: memcpy(data, v, 4 * sizeof(float)); break;
      case FIELD_MAT4: memcpy(data, v, 16 * sizeof(float)); break;
      default: lovrUnreachable();
    }
  } else {
    for (uint32_t i = 0; i < fieldComponents[type]; i++) {
      double x = lua_tonumber(L, index + i);
      switch (type) {
        case FIELD_I8x4: p.i8[i] = (int8_t) x; break;
        case FIELD_U8x4: p.u8[i] = (uint8_t) x; break;
        case FIELD_SN8x4: p.i8[i] = (int8_t) CLAMP(x, -1.f, 1.f) * INT8_MAX; break;
        case FIELD_UN8x4: p.u8[i] = (uint8_t) CLAMP(x, 0.f, 1.f) * UINT8_MAX; break;
        case FIELD_UN10x3: p.u32[0] |= (uint32_t) (CLAMP(x, 0.f, 1.f) * 1023.f) << (10 * (2 - i)); break;
        case FIELD_I16: p.i16[i] = (int16_t) x; break;
        case FIELD_I16x2: p.i16[i] = (int16_t) x; break;
        case FIELD_I16x4: p.i16[i] = (int16_t) x; break;
        case FIELD_U16: p.u16[i] = (uint16_t) x; break;
        case FIELD_U16x2: p.u16[i] = (uint16_t) x; break;
        case FIELD_U16x4: p.u16[i] = (uint16_t) x; break;
        case FIELD_SN16x2: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
        case FIELD_SN16x4: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
        case FIELD_UN16x2: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
        case FIELD_UN16x4: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
        case FIELD_I32: p.i32[i] = (int32_t) x; break;
        case FIELD_I32x2: p.i32[i] = (int32_t) x; break;
        case FIELD_I32x3: p.i32[i] = (int32_t) x; break;
        case FIELD_I32x4: p.i32[i] = (int32_t) x; break;
        case FIELD_U32: p.u32[i] = (uint32_t) x; break;
        case FIELD_U32x2: p.u32[i] = (uint32_t) x; break;
        case FIELD_U32x3: p.u32[i] = (uint32_t) x; break;
        case FIELD_U32x4: p.i32[i] = (uint32_t) x; break;
        case FIELD_F16x2: p.u16[i] = float32to16(x); break;
        case FIELD_F16x4: p.u16[i] = float32to16(x); break;
        case FIELD_F32: p.f32[i] = (float) x; break;
        case FIELD_F32x2: p.f32[i] = (float) x; break;
        case FIELD_F32x3: p.f32[i] = (float) x; break;
        case FIELD_F32x4: p.f32[i] = (float) x; break;
        case FIELD_MAT2: p.f32[i] = (float) x; break;
        case FIELD_MAT3: p.f32[i] = (float) x; break;
        case FIELD_MAT4: p.f32[i] = (float) x; break;
        case FIELD_INDEX16: p.u16[i] = (uint16_t) x - 1; break;
        case FIELD_INDEX32: p.u32[i] = (uint32_t) x - 1; break;
        default: lovrUnreachable();
      }
    }
  }
}

static void luax_readstruct(lua_State* L, int index, const BufferField* field, char* data) {
  lovrCheck(lua_istable(L, index), "Expected table for struct data");
  index = index > 0 ? index : lua_gettop(L) + 1 + index;

  if (!field->children[0].name || luax_len(L, index) > 0) {
    for (uint32_t i = 0, j = 1; i < field->childCount; i++) {
      const BufferField* child = &field->children[i];
      uint32_t n = 1;

      lua_rawgeti(L, index, j);
      if (child->length == 0 && child->childCount == 0 && lua_type(L, -1) == LUA_TNUMBER) {
        for (uint32_t c = fieldComponents[child->type]; c > 1; c--, n++) {
          lua_rawgeti(L, index, j + n);
        }
      }

      luax_readbufferfield(L, -n, child, data + child->offset);
      lua_pop(L, n);
      j += n;
    }
  } else {
    for (uint32_t i = 0; i < field->childCount; i++) {
      const BufferField* child = &field->children[i];
      lua_pushstring(L, child->name);
      lua_rawget(L, index);
      luax_readbufferfield(L, -1, child, data + child->offset);
      lua_pop(L, 1);
    }
  }
}

static void luax_readarray(lua_State* L, int index, uint32_t offset, uint32_t count, const BufferField* field, char* data) {
  lovrCheck(lua_istable(L, index), "Expected table for array data");

  if (!count) {
    count = field->length;
  }

  lua_rawgeti(L, index, 1);
  int type = lua_type(L, -1);
  lua_pop(L, 1);

  if (field->childCount > 0) {
    for (uint32_t i = 0; i < count; i++, data += field->stride) {
      lua_rawgeti(L, index, i + offset + 1);
      luax_readstruct(L, -1, field, data);
      lua_pop(L, 1);
    }
  } else {
    int n = fieldComponents[field->type];

    if (type == LUA_TUSERDATA || type == LUA_TLIGHTUSERDATA) {
      for (uint32_t i = 0; i < count; i++, data += field->stride) {
        lua_rawgeti(L, index, i + offset + 1);
        lovrCheck(lua_isuserdata(L, -1), "Expected vector object for array value (arrays must use the same type for all elements)");
        luax_readcomponents(L, -1, field->type, data);
        lua_pop(L, 1);
      }
    } else if (type == LUA_TNUMBER) {
      for (uint32_t i = 0; i < count; i++, data += field->stride) {
        for (int c = 1; c <= n; c++) {
          lua_rawgeti(L, index, i * n + offset + c);
        }
        luax_readcomponents(L, -n, field->type, data);
        lua_pop(L, n);
      }
    } else if (type == LUA_TTABLE) {
      for (uint32_t i = 0; i < count; i++, data += field->stride) {
        lua_rawgeti(L, index, i + offset + 1);
        lovrCheck(lua_istable(L, -1), "Expected nested table for array value (arrays must use the same type for all elements)");
        for (int c = 1, j = -1; c <= n; c++, j--) {
          lua_rawgeti(L, j, c);
        }
        luax_readcomponents(L, -n, field->type, data);
        lua_pop(L, n + 1);
      }
    } else {
      lovrThrow("Expected number, table, or vector for array contents");
    }
  }
}

void luax_readbufferfield(lua_State* L, int index, const BufferField* field, char* data) {
  if (field->length > 0) {
    luax_readarray(L, index, 0, 0, field, data);
  } else if (field->childCount > 0) {
    luax_readstruct(L, index, field, data);
  } else if (lua_type(L, index) == LUA_TTABLE) {
    int n = fieldComponents[field->type];
    for (int c = 0; c < n; c++) {
      lua_rawgeti(L, index < 0 ? index - c : index, c + 1);
    }
    luax_readcomponents(L, -n, field->type, data);
    lua_pop(L, n);
  } else {
    luax_readcomponents(L, index, field->type, data);
  }
}

void luax_readbufferdata(lua_State* L, int index, Buffer* buffer, char* data) {
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  Blob* blob = luax_totype(L, index, Blob);

  if (blob) {
    uint32_t srcOffset = luax_optu32(L, index + 1, 0);
    uint32_t dstOffset = luax_optu32(L, index + 2, 0);
    lovrCheck(srcOffset < blob->size, "Source offset is bigger than the size of the Blob");
    lovrCheck(dstOffset < info->size, "Destination offset is bigger than the size of the Buffer");
    uint32_t limit = MIN(blob->size - srcOffset, info->size - dstOffset);
    uint32_t extent = luax_optu32(L, index + 3, limit);
    lovrCheck(extent <= blob->size - srcOffset, "Buffer copy range exceeds the size of the source Blob");
    lovrCheck(extent <= info->size - dstOffset, "Buffer copy range exceeds the size of the target Buffer");
    data = data ? data : lovrBufferMap(buffer, dstOffset, extent);
    memcpy(data, (char*) blob->data + srcOffset, extent);
    return;
  }

  luaL_checktype(L, index, LUA_TTABLE);
  lovrCheck(info->fields, "Buffer must be created with format information to copy a table to it");

  if (info->fields[0].length > 0) {
    data = data ? data : lovrBufferMap(buffer, 0, info->size);
    luax_readbufferfield(L, index, info->fields, data);
  } else {
    lua_rawgeti(L, index, 1);
    bool nested = lua_istable(L, -1);
    lua_pop(L, 1);

    BufferField* array = &info->fields[0];
    uint32_t tableLength = luax_len(L, index);
    uint32_t srcIndex = luax_optu32(L, index + 1, 1) - 1;
    uint32_t dstIndex = luax_optu32(L, index + 2, 1) - 1;
    uint32_t limit = nested ? MIN(tableLength - srcIndex, array->length - dstIndex) : array->length - dstIndex;
    uint32_t count = luax_optu32(L, index + 3, limit);

    lovrCheck(dstIndex + count <= array->length, "Buffer copy range exceeds the length of the target Buffer");
    data = data ? data : lovrBufferMap(buffer, dstIndex * array->stride, count * array->stride);
    luax_readarray(L, index, srcIndex, count, array, data);
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
  uint32_t length = info->fields ? info->fields[0].length : 0;
  lua_pushinteger(L, length);
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t stride = info->fields && info->fields[0].length > 0 ? info->fields[0].stride : 0;
  lua_pushinteger(L, stride);
  return 1;
}

static void luax_pushbufferformat(lua_State* L, const BufferField* fields, uint32_t count, bool root) {
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    const BufferField* field = &fields[i];
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
      luax_pushenum(L, FieldType, field->type);
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
  } else if (info->fields[0].childCount > 0) {
    luax_pushbufferformat(L, info->fields[0].children, info->fields[0].childCount, true);
  } else {
    luax_pushbufferformat(L, info->fields, 1, true);
  }
  return 1;
}

static int l_lovrBufferGetPointer(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  if (!lovrBufferIsTemporary(buffer)) {
    lua_pushnil(L);
    return 1;
  }
  void* pointer = lovrBufferMap(buffer, 0, ~0u);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferIsTemporary(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  bool temporary = lovrBufferIsTemporary(buffer);
  lua_pushboolean(L, temporary);
  return 1;
}

static int l_lovrBufferSetData(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  luax_readbufferdata(L, 2, buffer, NULL);
  return 0;
}

static int l_lovrBufferClear(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t offset = luax_optu32(L, 2, 0);
  uint32_t extent = luax_optu32(L, 3, info->size - offset);
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
  { "setData", l_lovrBufferSetData },
  { "clear", l_lovrBufferClear },
  { NULL, NULL }
};
