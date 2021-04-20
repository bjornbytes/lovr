#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static const uint16_t fieldComponents[] = {
  [FIELD_I8] = 1,
  [FIELD_U8] = 1,
  [FIELD_VEC2] = 2,
  [FIELD_VEC3] = 3,
  [FIELD_VEC4] = 4,
  [FIELD_MAT4] = 16
};

void luax_readbufferdata(lua_State* L, int index, Buffer* buffer, void* data) {
  const BufferInfo* info = lovrBufferGetInfo(buffer);
  const BufferFormat* format = &info->format;
  char* base = data;

  Blob* blob = luax_totype(L, index, Blob);

  if (blob) {
    uint32_t dstOffset = lua_tointeger(L, index + 2);
    uint32_t srcOffset = lua_tointeger(L, index + 3);
    uint32_t size = luaL_optinteger(L, index + 1, MIN(blob->size - srcOffset, info->size - dstOffset));
    lovrAssert(srcOffset + size <= blob->size, "Tried to read past the end of the Blob");
    lovrAssert(dstOffset + size <= info->size, "Tried to write past the end of the Buffer");
    memcpy(base + dstOffset, (char*) blob->data + srcOffset, size);
    return;
  }

  luaL_checktype(L, index, LUA_TTABLE);
  lovrAssert(format->count > 0, "Buffer must be created with a format to write to it using a table");

  if (format->count == 1) {
    uint16_t offset = format->offsets[0];
    FieldType type = format->types[0];
    VectorType vectorType;
    lua_rawgeti(L, index, 1);
    float* p = luax_tovector(L, -1, &vectorType);
    lua_pop(L, 1);
    if (p) {
      uint16_t components;
      switch (vectorType) {
        case V_VEC2: components = 2; break;
        case V_VEC3: components = 3; break;
        case V_VEC4: components = 4; break;
        case V_MAT4: components = 16; break;
        default: lovrThrow("Unsupported vector type for Buffer field");
      }
      lovrAssert(components == fieldComponents[type], "Vector component count does not match field component count");
      int length = luax_len(L, index);
      for (int i = 0; i < length; i++) {
        lua_rawgeti(L, index, i + 1);
        p = luax_checkvector(L, -1, vectorType, NULL);
        switch (type) {
          case FIELD_VEC2: memcpy(base + offset, p, 2 * sizeof(float)); break;
          case FIELD_VEC3: memcpy(base + offset, p, 3 * sizeof(float)); break;
          case FIELD_VEC4: memcpy(base + offset, p, 4 * sizeof(float)); break;
          case FIELD_MAT4: memcpy(base + offset, p, 16 * sizeof(float)); break;
          default: lovrThrow("Unreachable");
        }
        base += format->stride;
        lua_pop(L, 1);
      }
    } else {
      int length = luax_len(L, index);
      uint16_t components = fieldComponents[type];
      for (int i = 0; i < length; i += components) {
        for (uint16_t c = 0; c < components; c++) {
          lua_rawgeti(L, index, i + 1);
          switch (type) {
            case FIELD_I8: ((int8_t*) (base + offset))[c] = (int8_t) lua_tonumber(L, -1); break;
            case FIELD_U8: ((uint8_t*) (base + offset))[c] = (uint8_t) lua_tonumber(L, -1); break;
            case FIELD_VEC2: ((float*) (base + offset))[c] = (float) lua_tonumber(L, -1); break;
            case FIELD_VEC3: ((float*) (base + offset))[c] = (float) lua_tonumber(L, -1); break;
            case FIELD_VEC4: ((float*) (base + offset))[c] = (float) lua_tonumber(L, -1); break;
            case FIELD_MAT4: ((float*) (base + offset))[c] = (float) lua_tonumber(L, -1); break;
          }
          lua_pop(L, 1);
        }
        base += format->stride;
      }
    }
  } else {
    int length = luax_len(L, index);
    for (int i = 0; i < length; i++) {
      int j = 1;
      lua_rawgeti(L, index, i + 1);
      lovrAssert(lua_type(L, -1) == LUA_TTABLE, "Expected table of tables");
      for (uint16_t k = 0; k < format->count; k++) {
        uint16_t offset = format->offsets[k];
        FieldType type = format->types[k];
        lua_rawgeti(L, -1, j);
        VectorType vectorType;
        float* p = luax_tovector(L, -1, &vectorType);
        if (p) {
          uint16_t components;
          switch (vectorType) {
            case V_VEC2: components = 2; break;
            case V_VEC3: components = 3; break;
            case V_VEC4: components = 4; break;
            case V_MAT4: components = 16; break;
            default: lovrThrow("Unsupported vector type for Buffer field");
          }
          lovrAssert(components == fieldComponents[type], "Vector component count does not match field component count");
          switch (type) {
            case FIELD_MAT4: memcpy(base + offset, p, 16 * sizeof(float)); break;
            default: lovrThrow("Unreachable");
          }
          lua_pop(L, 1);
        } else {
          uint16_t components = fieldComponents[type];
          for (uint16_t c = 1; c < components; c++) {
            lua_rawgeti(L, -(c + 1), ++j);
          }

          for (uint16_t c = 0, idx = -components; c < components; c++, idx++) {
            switch (type) {
              case FIELD_I8: ((int8_t*) (base + offset))[c] = lua_tonumber(L, idx); break;
              case FIELD_U8: ((uint8_t*) (base + offset))[c] = lua_tonumber(L, idx); break;
              case FIELD_VEC2: ((float*) (base + offset))[c] = lua_tonumber(L, idx); break;
              case FIELD_VEC3: ((float*) (base + offset))[c] = lua_tonumber(L, idx); break;
              case FIELD_VEC4: ((float*) (base + offset))[c] = lua_tonumber(L, idx); break;
              case FIELD_MAT4: ((float*) (base + offset))[c] = lua_tonumber(L, idx); break;
            }
          }

          lua_pop(L, components);
        }
      }
      base += format->stride;
      lua_pop(L, 1);
    }
  }
}

static int l_lovrBufferGetSize(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  uint32_t size = lovrBufferGetInfo(buffer)->size;
  lua_pushinteger(L, size);
  return 1;
}

static int l_lovrBufferGetType(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  BufferType type = lovrBufferGetInfo(buffer)->type;
  luax_pushenum(L, BufferType, type);
  return 1;
}

const luaL_Reg lovrBuffer[] = {
  { "getSize", l_lovrBufferGetSize },
  { "getType", l_lovrBufferGetType },
  { NULL, NULL }
};
