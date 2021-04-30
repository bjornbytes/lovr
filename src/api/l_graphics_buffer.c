#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
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

static int luax_pushbufferfield(lua_State* L, void* data, FieldType type) {
  FieldPointer p = { .raw = data };
  int components = fieldComponents[type];
  for (int c = 0; c < components; c++) {
    switch (type) {
      case FIELD_I8: lua_pushnumber(L, p.i8[c]); break;
      case FIELD_U8: lua_pushnumber(L, p.u8[c]); break;
      case FIELD_I16: lua_pushnumber(L, p.i16[c]); break;
      case FIELD_U16: lua_pushnumber(L, p.u16[c]); break;
      case FIELD_I32: lua_pushnumber(L, p.i32[c]); break;
      case FIELD_U32: lua_pushnumber(L, p.u32[c]); break;
      case FIELD_F32: lua_pushnumber(L, p.f32[c]); break;
      case FIELD_F64: lua_pushnumber(L, p.f64[c]); break;
      case FIELD_I8x2: lua_pushnumber(L, p.i8[c]); break;
      case FIELD_U8x2: lua_pushnumber(L, p.u8[c]); break;
      case FIELD_I8Nx2: lua_pushnumber(L, p.i8[c] / (double) INT8_MAX); break;
      case FIELD_U8Nx2: lua_pushnumber(L, p.u8[c] / (double) UINT8_MAX); break;
      case FIELD_I16x2: lua_pushnumber(L, p.i16[c]); break;
      case FIELD_U16x2: lua_pushnumber(L, p.u16[c]); break;
      case FIELD_I16Nx2: lua_pushnumber(L, p.i16[c] / (double) INT16_MAX); break;
      case FIELD_U16Nx2: lua_pushnumber(L, p.u16[c] / (double) UINT16_MAX); break;
      case FIELD_I32x2: lua_pushnumber(L, p.i32[c]); break;
      case FIELD_U32x2: lua_pushnumber(L, p.u32[c]); break;
      case FIELD_F32x2: lua_pushnumber(L, p.f32[c]); break;
      case FIELD_I32x3: lua_pushnumber(L, p.i32[c]); break;
      case FIELD_U32x3: lua_pushnumber(L, p.u32[c]); break;
      case FIELD_F32x3: lua_pushnumber(L, p.f32[c]); break;
      case FIELD_I8x4: lua_pushnumber(L, p.i8[c]); break;
      case FIELD_U8x4: lua_pushnumber(L, p.u8[c]); break;
      case FIELD_I8Nx4: lua_pushnumber(L, p.i8[c] / (double) INT8_MAX); break;
      case FIELD_U8Nx4: lua_pushnumber(L, p.u8[c] / (double) UINT8_MAX); break;
      case FIELD_I16x4: lua_pushnumber(L, p.i16[c]); break;
      case FIELD_U16x4: lua_pushnumber(L, p.u16[c]); break;
      case FIELD_I16Nx4: lua_pushnumber(L, p.i16[c] / (double) INT16_MAX); break;
      case FIELD_U16Nx4: lua_pushnumber(L, p.u16[c] / (double) UINT16_MAX); break;
      case FIELD_I32x4: lua_pushnumber(L, p.i32[c]); break;
      case FIELD_U32x4: lua_pushnumber(L, p.u32[c]); break;
      case FIELD_F32x4: lua_pushnumber(L, p.f32[c]); break;
      case FIELD_MAT2: lua_pushnumber(L, p.f32[c]); break;
      case FIELD_MAT3: lua_pushnumber(L, p.f32[c]); break;
      case FIELD_MAT4: lua_pushnumber(L, p.f32[c]); break;
      default: lovrThrow("Unreachable");
    }
  }
  return components;
}

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
  uint32_t stride = info->stride;

  Blob* blob = luax_totype(L, index, Blob);
  uint32_t dstIndex = luaL_optinteger(L, index + 1, 1) - 1;
  uint32_t srcIndex = luaL_optinteger(L, index + 2, 1) - 1;

  if (blob) {
    uint32_t limit = MIN(blob->size / stride - srcIndex, info->length - dstIndex);
    uint32_t count = luaL_optinteger(L, index + 3, limit);
    lovrAssert(srcIndex + count <= blob->size / stride, "Tried to read too many elements from the Blob");
    lovrAssert(dstIndex + count <= info->length, "Tried to write Buffer elements [%d,%d] but Buffer can only hold %d things", dstIndex + 1, dstIndex + count - 1, info->length);
    char* dst = (char*) data + dstIndex * stride;
    char* src = (char*) blob->data + srcIndex * stride;
    memcpy(dst, src, count * stride);
    return;
  }

  luaL_checktype(L, index, LUA_TTABLE);
  lua_rawgeti(L, index, 1);
  bool nested = lua_istable(L, -1);
  lua_pop(L, 1);

  uint32_t length = luax_len(L, index);
  uint32_t limit = nested ? MIN(length - srcIndex, info->length - dstIndex) : info->length - dstIndex;
  uint32_t count = luaL_optinteger(L, index + 3, limit);
  lovrAssert(dstIndex + count <= info->length, "Tried to write Buffer elements [%d,%d] but Buffer can only hold %d things", dstIndex + 1, dstIndex + count - 1, info->length);

  char* base = (char*) data + dstIndex * stride;

  if (nested) {
    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, index, i + srcIndex + 1);
      lovrAssert(lua_type(L, -1) == LUA_TTABLE, "Expected table of tables");
      int j = 1;
      for (uint16_t f = 0; f < info->fieldCount; f++) {
        uint16_t offset = info->offsets[f];
        FieldType type = info->types[f];
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
      base += info->stride;
      lua_pop(L, 1);
    }
  } else {
    for (uint32_t i = 0, j = srcIndex + 1; i < count && j <= length; i++) {
      for (uint16_t f = 0; f < info->fieldCount; f++) {
        uint16_t offset = info->offsets[f];
        FieldType type = info->types[f];
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
      base += info->stride;
    }
  }
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  uint32_t size = info->length * info->stride;
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
    lua_createtable(L, 2, 0);
    luax_pushenum(L, FieldType, info->types[i]);
    lua_rawseti(L, -2, 1);
    lua_pushinteger(L, info->offsets[i]);
    lua_rawseti(L, -2, 2);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrBufferGetPointer(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  void* pointer = lovrBufferMap(buffer);
  lua_pushlightuserdata(L, pointer);
  return 1;
}

static int l_lovrBufferHasFlags(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  luaL_checkany(L, 2);
  int top = lua_gettop(L);
  for (int i = 2; i <= top; i++) {
    int bit = luax_checkenum(L, i, BufferFlag, NULL);
    if (~info->flags & (1 << bit)) {
      lua_pushboolean(L, false);
    }
  }
  lua_pushboolean(L, true);
  return 1;
}

static int l_lovrBufferWrite(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  void* data = lovrBufferMap(buffer);
  luax_readbufferdata(L, 2, buffer, data);
  return 0;
}

static int l_lovrBufferAppend(lua_State* L) {
  //Buffer* buffer = luax_checktype(L, 1, Buffer);
  //const BufferInfo* info = lovrBufferGetInfo(buffer);
  //void* data = lovrBufferMap(buffer);
  //uint32_t offset = lovrBufferAppend(buffer, info->stride);
  // something, need to write 1 thing, want to reuse buffer:write somehow, fuck
  return 0;
}

static int l_lovrBufferRewind(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  lovrBufferRewind(buffer);
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

static int l_lovrBufferCopy(lua_State* L) {
  Buffer* src = luax_checktype(L, 1, Buffer);
  Buffer* dst = luax_checktype(L, 2, Buffer);
  const BufferInfo* srcInfo = lovrBufferGetInfo(src);
  const BufferInfo* dstInfo = lovrBufferGetInfo(dst);
  uint32_t srcSize = srcInfo->length * srcInfo->stride;
  uint32_t dstSize = dstInfo->length * dstInfo->stride;
  uint32_t srcOffset = luaL_optinteger(L, 3, 0);
  uint32_t dstOffset = luaL_optinteger(L, 4, 0);
  uint32_t size = luaL_optinteger(L, 5, MIN(srcSize - srcOffset, dstSize - dstOffset));
  lovrBufferCopy(src, dst, srcOffset, dstOffset, size);
  return 0;
}

typedef struct {
  lua_State* L;
  int ref;
  Buffer* buffer;
  uint32_t index;
  uint32_t count;
  Blob* blob;
  uint32_t offset;
} BufferReader;

static void luax_onreadback(void* data, uint64_t size, void* userdata) {
  BufferReader* reader = userdata;

  lua_State* L = reader->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, reader->ref);

  Buffer* buffer = reader->buffer;
  const BufferInfo* info = lovrBufferGetInfo(buffer);

  if (reader->blob) {
    memcpy((char*) reader->blob->data + reader->offset, data, size);
    luax_pushtype(L, Blob, reader->blob);
  } else {
    uint32_t totalComponents = 0;
    for (uint32_t i = 0; i < info->fieldCount; i++) {
      totalComponents += fieldComponents[info->types[i]];
    }

    int index = 1;
    char* base = data;
    lua_createtable(L, reader->count * totalComponents, 0);
    int tableIndex = lua_gettop(L);
    for (uint32_t i = 0; i < reader->count; i++) {
      for (uint32_t f = 0; f < info->fieldCount; f++) {
        int components = luax_pushbufferfield(L, base + info->offsets[f], info->types[f]);
        for (int c = 1; c <= components; c++) {
          lua_rawseti(L, tableIndex, index + components - c);
        }
        index += components;
      }
      base += info->stride;
    }
  }

  luax_pushtype(L, Buffer, buffer);
  lua_call(L, 2, 0);

  lovrRelease(reader->blob, lovrBlobDestroy);
  lovrRelease(reader->buffer, lovrBufferDestroy);
  luaL_unref(L, LUA_REGISTRYINDEX, reader->ref);
  free(userdata);
}

static int l_lovrBufferRead(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  BufferReader* reader = malloc(sizeof(BufferReader));
  lovrAssert(reader, "Out of memory");
  reader->L = L;
  lua_pushvalue(L, 2);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  reader->ref = luaL_ref(L, LUA_REGISTRYINDEX);
  reader->buffer = buffer;
  reader->index = luaL_optinteger(L, 3, 1);
  reader->count = luaL_optinteger(L, 4, info->length - reader->index + 1);
  reader->blob = luax_totype(L, 5, Blob);
  reader->offset = luaL_optinteger(L, 6, 0);
  lovrRetain(buffer);
  lovrRetain(reader->blob);
  lovrBufferRead(buffer, (reader->index - 1) * info->stride, reader->count * info->stride, luax_onreadback, reader);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "getSize", l_lovrBufferGetSize },
  { "getLength", l_lovrBufferGetLength },
  { "getStride", l_lovrBufferGetStride },
  { "getFormat", l_lovrBufferGetFormat },
  { "getPointer", l_lovrBufferGetPointer },
  { "hasFlags", l_lovrBufferHasFlags },
  { "write", l_lovrBufferWrite },
  { "append", l_lovrBufferAppend },
  { "rewind", l_lovrBufferRewind },
  { "clear", l_lovrBufferClear },
  { "copy", l_lovrBufferCopy },
  { "read", l_lovrBufferRead },
  { NULL, NULL }
};
