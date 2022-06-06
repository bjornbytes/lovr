#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t vectorComponents[MAX_VECTOR_TYPES] = {
  [V_VEC2] = 2,
  [V_VEC3] = 3,
  [V_VEC4] = 4,
  [V_QUAT] = 4,
  [V_MAT4] = 16
};

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
  [FIELD_MAT4] = 16
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

void luax_readbufferfield(lua_State* L, int index, int type, void* data) {
  FieldPointer p = { .raw = data };
  if (lua_isuserdata(L, index)) {
    VectorType vectorType;
    float* v = luax_tovector(L, index, &vectorType);
    lovrCheck(vectorComponents[vectorType] == fieldComponents[type], "Vector type is incompatible with field type");
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
        default: lovrUnreachable();
      }
    }
  }
}

void luax_readbufferdata(lua_State* L, int index, Buffer* buffer, char* data) {
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t stride = info->stride;

  Blob* blob = luax_totype(L, index, Blob);
  uint32_t srcIndex = luax_optu32(L, index + 1, 1) - 1;
  uint32_t dstIndex = luax_optu32(L, index + 2, 1) - 1;

  if (blob) {
    uint32_t limit = MIN(blob->size / stride - srcIndex, info->length - dstIndex);
    uint32_t count = luax_optu32(L, index + 3, limit);
    lovrCheck(srcIndex + count <= blob->size / stride, "Tried to read too many elements from the Blob");
    lovrCheck(dstIndex + count <= info->length, "Tried to write Buffer elements [%d,%d] but Buffer can only hold %d things", dstIndex + 1, dstIndex + count - 1, info->length);
    data = data ? data : lovrBufferMap(buffer, dstIndex * stride, count * stride);
    char* src = (char*) blob->data + srcIndex * stride;
    memcpy(data, src, count * stride);
    return;
  }

  luaL_checktype(L, index, LUA_TTABLE);
  lua_rawgeti(L, index, 1);
  bool nested = lua_istable(L, -1);
  lua_pop(L, 1);

  uint32_t length = luax_len(L, index);
  uint32_t limit = nested ? MIN(length - srcIndex, info->length - dstIndex) : info->length - dstIndex;
  uint32_t count = luaL_optinteger(L, index + 3, limit);
  lovrCheck(dstIndex + count <= info->length, "Tried to write Buffer elements [%d,%d] but Buffer can only hold %d things", dstIndex + 1, dstIndex + count - 1, info->length);

  data = data ? data : lovrBufferMap(buffer, dstIndex * stride, count * stride);

  if (nested) {
    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, index, i + srcIndex + 1);
      lovrCheck(lua_type(L, -1) == LUA_TTABLE, "Expected table of tables");
      int j = 1;
      for (uint32_t f = 0; f < info->fieldCount; f++) {
        int n = 1;
        lua_rawgeti(L, -1, j);
        const BufferField* field = &info->fields[f];
        if (!lua_isuserdata(L, -1)) {
          n = fieldComponents[field->type];
          for (int c = 1; c < n; c++) {
            lua_rawgeti(L, -c - 1, j + c);
          }
        }
        luax_readbufferfield(L, -n, field->type, data + field->offset);
        lua_pop(L, n);
        j += n;
      }
      data += info->stride;
      lua_pop(L, 1);
    }
  } else {
    for (uint32_t i = 0, j = srcIndex + 1; i < count && j <= length; i++) {
      for (uint32_t f = 0; f < info->fieldCount; f++) {
        int n = 1;
        lua_rawgeti(L, index, j);
        const BufferField* field = &info->fields[f];
        if (!lua_isuserdata(L, -1)) {
          n = fieldComponents[field->type];
          for (int c = 1; c < n; c++) {
            lua_rawgeti(L, index, (int) j + c);
          }
        }
        luax_readbufferfield(L, -n, field->type, data + field->offset);
        lua_pop(L, n);
        j += n;
      }
      data += info->stride;
    }
  }
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t size = info->length * MAX(info->stride, 1);
  lua_pushinteger(L, size);
  return 1;
}

static int l_lovrBufferGetLength(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->length);
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_pushinteger(L, info->stride);
  return 1;
}

static int l_lovrBufferGetFormat(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  lua_createtable(L, info->fieldCount, 0);
  for (uint32_t i = 0; i < info->fieldCount; i++) {
    const BufferField* field = &info->fields[i];
    lua_createtable(L, 0, 3);
    luax_pushenum(L, FieldType, field->type);
    lua_setfield(L, -2, "type");
    lua_pushinteger(L, field->offset);
    lua_setfield(L, -2, "offset");
    lua_pushinteger(L, field->location);
    lua_setfield(L, -2, "location");
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrBufferGetPointer(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  if (!lovrBufferIsTemporary(buffer)) {
    lua_pushnil(L);
    return 1;
  }
  void* pointer = lovrBufferMap(buffer, 0, ~0u);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferIsTemporary(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  bool temporary = lovrBufferIsTemporary(buffer);
  lua_pushboolean(L, temporary);
  return 1;
}

static int l_lovrBufferSetData(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  luax_readbufferdata(L, 2, buffer, NULL);
  return 0;
}

static int l_lovrBufferClear(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t index = luaL_optinteger(L, 2, 1);
  uint32_t count = luaL_optinteger(L, 3, info->length - index + 1);
  lovrBufferClear(buffer, (index - 1) * info->stride, count * info->stride);
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
