#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static const uint16_t vectorComponents[MAX_VECTOR_TYPES] = {
  [V_VEC2] = 2,
  [V_VEC3] = 3,
  [V_VEC4] = 4,
  [V_MAT4] = 16
};

static const uint16_t fieldComponents[] = {
  [FIELD_I8] = 1,
  [FIELD_U8] = 1,
  [FIELD_I16] = 1,
  [FIELD_U16] = 1,
  [FIELD_I32] = 1,
  [FIELD_U32] = 1,
  [FIELD_F32] = 1,
  [FIELD_F64] = 1,
  [FIELD_I8x2] = 2,
  [FIELD_U8x2] = 2,
  [FIELD_I8Nx2] = 2,
  [FIELD_U8Nx2] = 2,
  [FIELD_I16x2] = 2,
  [FIELD_U16x2] = 2,
  [FIELD_I16Nx2] = 2,
  [FIELD_U16Nx2] = 2,
  [FIELD_I32x2] = 2,
  [FIELD_U32x2] = 2,
  [FIELD_F32x2] = 2,
  [FIELD_I32x3] = 3,
  [FIELD_U32x3] = 3,
  [FIELD_F32x3] = 3,
  [FIELD_I8x4] = 4,
  [FIELD_U8x4] = 4,
  [FIELD_I8Nx4] = 4,
  [FIELD_U8Nx4] = 4,
  [FIELD_I16x4] = 4,
  [FIELD_U16x4] = 4,
  [FIELD_I16Nx4] = 4,
  [FIELD_U16Nx4] = 4,
  [FIELD_I32x4] = 4,
  [FIELD_U32x4] = 4,
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
  double* f64;
} FieldPointer;

static void luax_readbufferfield(lua_State* L, int index, FieldType type, int components, void* data) {
  FieldPointer p = { .raw = data };
  for (int i = 0; i < components; i++) {
    double x = lua_tonumber(L, index + i);
    switch (type) {
      case FIELD_I8: p.i8[i] = (int8_t) x; break;
      case FIELD_U8: p.u8[i] = (uint8_t) x; break;
      case FIELD_I16: p.i16[i] = (int16_t) x; break;
      case FIELD_U16: p.u16[i] = (uint16_t) x; break;
      case FIELD_I32: p.i32[i] = (int32_t) x; break;
      case FIELD_U32: p.u32[i] = (uint32_t) x; break;
      case FIELD_F32: p.f32[i] = (float) x; break;
      case FIELD_F64: p.f64[i] = (double) x; break;
      case FIELD_I8x2: p.i8[i] = (int8_t) x; break;
      case FIELD_U8x2: p.u8[i] = (uint8_t) x; break;
      case FIELD_I8Nx2: p.i8[i] = (int8_t) CLAMP(x, -1.f, 1.f) * INT8_MAX; break;
      case FIELD_U8Nx2: p.u8[i] = (uint8_t) CLAMP(x, 0.f, 1.f) * UINT8_MAX; break;
      case FIELD_I16x2: p.i16[i] = (int16_t) x; break;
      case FIELD_U16x2: p.u16[i] = (uint16_t) x; break;
      case FIELD_I16Nx2: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
      case FIELD_U16Nx2: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
      case FIELD_I32x2: p.i32[i] = (int32_t) x; break;
      case FIELD_U32x2: p.u32[i] = (uint32_t) x; break;
      case FIELD_F32x2: p.f32[i] = (float) x; break;
      case FIELD_I32x3: p.i32[i] = (int32_t) x; break;
      case FIELD_U32x3: p.u32[i] = (uint32_t) x; break;
      case FIELD_F32x3: p.f32[i] = (float) x; break;
      case FIELD_I8x4: p.i8[i] = (int8_t) x; break;
      case FIELD_U8x4: p.u8[i] = (uint8_t) x; break;
      case FIELD_I8Nx4: p.i8[i] = (int8_t) CLAMP(x, -1.f, 1.f) * INT8_MAX; break;
      case FIELD_U8Nx4: p.u8[i] = (uint8_t) CLAMP(x, 0.f, 1.f) * UINT8_MAX; break;
      case FIELD_I16x4: p.i16[i] = (int16_t) x; break;
      case FIELD_U16x4: p.u16[i] = (uint16_t) x; break;
      case FIELD_I16Nx4: p.i16[i] = (int16_t) CLAMP(x, -1.f, 1.f) * INT16_MAX; break;
      case FIELD_U16Nx4: p.u16[i] = (uint16_t) CLAMP(x, 0.f, 1.f) * UINT16_MAX; break;
      case FIELD_I32x4: p.i32[i] = (int32_t) x; break;
      case FIELD_U32x4: p.i32[i] = (uint32_t) x; break;
      case FIELD_F32x4: p.f32[i] = (float) x; break;
      case FIELD_MAT2: p.f32[i] = (float) x; break;
      case FIELD_MAT3: p.f32[i] = (float) x; break;
      case FIELD_MAT4: p.f32[i] = (float) x; break;
      default: lovrThrow("Unreachable");
    }
  }
}

static void luax_readbufferfieldv(float* v, FieldType type, int c, void* data) {
  FieldPointer p = { .raw = data };
  switch (type) {
    case FIELD_I8x2: for (int i = 0; i < 2; i++) p.i8[c] = (int8_t) v[i]; break;
    case FIELD_U8x2: for (int i = 0; i < 2; i++) p.u8[c] = (uint8_t) v[i]; break;
    case FIELD_I8Nx2: for (int i = 0; i < 2; i++) p.i8[c] = (int8_t) CLAMP(v[i], -1.f, 1.f) * INT8_MAX; break;
    case FIELD_U8Nx2: for (int i = 0; i < 2; i++) p.u8[c] = (uint8_t) CLAMP(v[i], 0.f, 1.f) * UINT8_MAX; break;
    case FIELD_I16x2: for (int i = 0; i < 2; i++) p.i16[c] = (int16_t) v[i]; break;
    case FIELD_U16x2: for (int i = 0; i < 2; i++) p.u16[c] = (uint16_t) v[i]; break;
    case FIELD_I16Nx2: for (int i = 0; i < 2; i++) p.i16[c] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
    case FIELD_U16Nx2: for (int i = 0; i < 2; i++) p.u16[c] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
    case FIELD_I32x2: for (int i = 0; i < 2; i++) p.i32[c] = (int32_t) v[i]; break;
    case FIELD_U32x2: for (int i = 0; i < 2; i++) p.u32[c] = (uint32_t) v[i]; break;
    case FIELD_I32x3: for (int i = 0; i < 3; i++) p.i32[c] = (int32_t) v[i]; break;
    case FIELD_U32x3: for (int i = 0; i < 3; i++) p.u32[c] = (uint32_t) v[i]; break;
    case FIELD_I8x4: for (int i = 0; i < 4; i++) p.i8[c] = (int8_t) v[i]; break;
    case FIELD_U8x4: for (int i = 0; i < 4; i++) p.u8[c] = (uint8_t) v[i]; break;
    case FIELD_I8Nx4: for (int i = 0; i < 4; i++) p.i8[c] = (int8_t) CLAMP(v[i], -1.f, 1.f) * INT8_MAX; break;
    case FIELD_U8Nx4: for (int i = 0; i < 4; i++) p.u8[c] = (uint8_t) CLAMP(v[i], 0.f, 1.f) * UINT8_MAX; break;
    case FIELD_I16x4: for (int i = 0; i < 4; i++) p.i16[c] = (int16_t) v[i]; break;
    case FIELD_U16x4: for (int i = 0; i < 4; i++) p.u16[c] = (uint16_t) v[i]; break;
    case FIELD_I16Nx4: for (int i = 0; i < 4; i++) p.i16[c] = (int16_t) CLAMP(v[i], -1.f, 1.f) * INT16_MAX; break;
    case FIELD_U16Nx4: for (int i = 0; i < 4; i++) p.u16[c] = (uint16_t) CLAMP(v[i], 0.f, 1.f) * UINT16_MAX; break;
    case FIELD_I32x4: for (int i = 0; i < 4; i++) p.i32[c] = (int32_t) v[i]; break;
    case FIELD_U32x4: for (int i = 0; i < 4; i++) p.u32[c] = (uint32_t) v[i]; break;
    case FIELD_F32x2: memcpy(data, v, 2 * sizeof(float)); break;
    case FIELD_F32x3: memcpy(data, v, 3 * sizeof(float)); break;
    case FIELD_F32x4: memcpy(data, v, 4 * sizeof(float)); break;
    case FIELD_MAT4: memcpy(data, v, 16 * sizeof(float)); break;
    default: lovrThrow("Unreachable");
  }
}

void luax_readbufferdata(lua_State* L, int index, Buffer* buffer, void* data) {
  const BufferInfo* info = lovrBufferGetInfo(buffer);

  Blob* blob = luax_totype(L, index, Blob);

  if (blob) {
    uint32_t dstOffset = lua_tointeger(L, index + 1);
    uint32_t srcOffset = lua_tointeger(L, index + 2);
    uint32_t size = luaL_optinteger(L, index + 3, MIN(blob->size - srcOffset, info->size - dstOffset));
    lovrAssert(srcOffset + size <= blob->size, "Tried to read past the end of the Blob");
    lovrAssert(dstOffset + size <= info->size, "Tried to write past the end of the Buffer");
    memcpy((char*) data + dstOffset, (char*) blob->data + srcOffset, size);
    return;
  }

  luaL_checktype(L, index, LUA_TTABLE);
  const BufferFormat* format = &info->format;
  lovrAssert(format->count > 0, "Buffer must be created with a format to write to it using a table");
  uint32_t dstOffset = luaL_optinteger(L, index + 1, 1) - 1;
  uint32_t srcOffset = luaL_optinteger(L, index + 2, 1) - 1;
  char* base = (char*) data + dstOffset * format->stride;
  uint32_t capacity = info->size / format->stride;
  uint32_t length = luax_len(L, index);

  lua_rawgeti(L, index, 1);
  bool nested = lua_istable(L, -1);
  lua_pop(L, 1);

  uint32_t limit = nested ? MIN(length - srcOffset, capacity - dstOffset) : capacity - dstOffset;
  uint32_t count = luaL_optinteger(L, index + 3, limit);

  if (nested) {
    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, index, i + srcOffset + 1);
      lovrAssert(lua_type(L, -1) == LUA_TTABLE, "Expected table of tables");
      int j = 1;
      for (uint16_t f = 0; f < format->count; f++) {
        uint16_t offset = format->offsets[f];
        FieldType type = format->types[f];
        VectorType vectorType;
        lua_rawgeti(L, -1, j);
        float* vector = luax_tovector(L, -1, &vectorType);
        if (vector) {
          uint16_t components = vectorComponents[vectorType];
          lovrAssert(components == fieldComponents[type], "Vector type is incompatible with field type");
          luax_readbufferfieldv(vector, type, components, base + offset);
          lua_pop(L, 1);
          j++;
        } else {
          uint16_t components = fieldComponents[type];
          for (uint16_t c = 1; c < components; c++) {
            lua_rawgeti(L, -c - 1, j + c);
          }
          luax_readbufferfield(L, -components, type, components, base + offset);
          lua_pop(L, components);
          j += components;
        }
      }
      base += format->stride;
      lua_pop(L, 1);
    }
  } else {
    for (uint32_t i = 0, j = srcOffset + 1; i < count && j <= length; i++) {
      for (uint16_t f = 0; f < format->count; f++) {
        uint16_t offset = format->offsets[f];
        FieldType type = format->types[f];
        VectorType vectorType;
        lua_rawgeti(L, index, j);
        float* vector = luax_tovector(L, -1, &vectorType);
        if (vector) {
          uint16_t components = vectorComponents[vectorType];
          lovrAssert(components == fieldComponents[type], "Vector type is incompatible with field type");
          luax_readbufferfieldv(vector, type, components, base + offset);
          lua_pop(L, 1);
          j++;
        } else {
          uint16_t components = fieldComponents[type];
          for (uint16_t c = 1; c < components; c++) {
            lua_rawgeti(L, index, j + c);
          }
          luax_readbufferfield(L, -components, type, components, base + offset);
          lua_pop(L, components);
          j += components;
        }
      }
      base += format->stride;
    }
  }
}

static int l_lovrBufferGetPointer(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  void* pointer = lovrBufferMap(buffer);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t size = lovrBufferGetInfo(buffer)->size;
  lua_pushinteger(L, size);
  return 1;
}

static int l_lovrBufferGetFormat(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferFormat* format = &lovrBufferGetInfo(buffer)->format;
  if (format->count == 0) {
    lua_pushnil(L);
    return 1;
  }
  lua_createtable(L, format->count, 0);
  for (uint32_t i = 0; i < format->count; i++) {
    luax_pushenum(L, FieldType, format->types[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrBufferGetStride(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferFormat* format = &lovrBufferGetInfo(buffer)->format;
  if (format->count > 0) {
    lua_pushinteger(L, format->stride);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrBufferGetType(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  BufferType type = lovrBufferGetInfo(buffer)->type;
  luax_pushenum(L, BufferType, type);
  return 1;
}

static int l_lovrBufferGetUsage(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  int count = 0;
  for (int i = 0; lovrBufferUsage[i].length; i++) {
    if (info->usage & (1 << i)) {
      lua_pushlstring(L, lovrBufferUsage[i].string, lovrBufferUsage[i].length);
      count++;
    }
  }
  return count;
}

static int l_lovrBufferWrite(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  void* data = lovrBufferMap(buffer);
  luax_readbufferdata(L, 2, buffer, data);
  return 0;
}

static int l_lovrBufferClear(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t offset = luaL_optinteger(L, 2, 0);
  uint32_t size = luaL_optinteger(L, 2, info->size - offset);
  lovrBufferClear(buffer, offset, size);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "getPointer", l_lovrBufferGetPointer },
  { "getSize", l_lovrBufferGetSize },
  { "getStride", l_lovrBufferGetStride },
  { "getType", l_lovrBufferGetType },
  { "getUsage", l_lovrBufferGetUsage },
  { "getFormat", l_lovrBufferGetFormat },
  { "write", l_lovrBufferWrite },
  { "clear", l_lovrBufferClear },
  { NULL, NULL }
};
