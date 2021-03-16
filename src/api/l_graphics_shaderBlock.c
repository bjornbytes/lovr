#include "api.h"
#include "graphics/buffer.h"
#include "graphics/shader.h"
#include "data/blob.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

static int l_lovrShaderBlockGetType(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  luax_pushenum(L, BlockType, lovrShaderBlockGetType(block));
  return 1;
}

static int l_lovrShaderBlockGetSize(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  Buffer* buffer = lovrShaderBlockGetBuffer(block);
  lua_pushinteger(L, lovrBufferGetSize(buffer));
  return 1;
}

static int l_lovrShaderBlockGetOffset(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  const char* name = luaL_checkstring(L, 2);
  const Uniform* uniform = lovrShaderBlockGetUniform(block, name);
  lovrAssert(uniform, "Unknown uniform for ShaderBlock '%s'", name);
  lua_pushinteger(L, uniform->offset);
  return 1;
}

static int l_lovrShaderBlockSend(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  if (lua_type(L, 2) == LUA_TSTRING) {
    const char* name = luaL_checkstring(L, 2);
    const Uniform* uniform = lovrShaderBlockGetUniform(block, name);
    lovrAssert(uniform, "Unknown uniform for ShaderBlock '%s'", name);
    Buffer* buffer = lovrShaderBlockGetBuffer(block);
    uint8_t* data = lovrBufferMap(buffer, uniform->offset, false);
    luax_checkuniform(L, 3, uniform, data, name);
    lovrBufferFlush(buffer, uniform->offset, uniform->size);
    return 0;
  } else {
    Blob* blob = luax_checktype(L, 2, Blob);
    Buffer* buffer = lovrShaderBlockGetBuffer(block);
    void* data = lovrBufferMap(buffer, 0, false);
    int offset = luaL_optinteger(L, 3, 0);
    lovrAssert(offset >= 0, "Negative offset");
    lovrAssert((size_t) offset < blob->size, "Offset %d larger than blob (size %d)", offset, (int) blob->size);
    int size = luaL_optnumber(L, 4, blob->size - offset);
    lovrAssert((size_t) size <= blob->size - offset, "Size %d offset %d larger than blob (size %d)", offset, size, (int) blob->size);
    size_t bufferSize = lovrBufferGetSize(buffer);
    size_t copySize = MIN(bufferSize, (size_t) size);
    memcpy(data, ((uint8_t*) blob->data) + offset, copySize);
    lovrBufferFlush(buffer, offset, copySize);
    lua_pushinteger(L, copySize);
    return 1;
  }
}

static int l_lovrShaderBlockRead(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  const char* name = luaL_checkstring(L, 2);
  const Uniform* uniform = lovrShaderBlockGetUniform(block, name);
  lovrAssert(uniform, "Unknown uniform for ShaderBlock '%s'", name);
  Buffer* buffer = lovrShaderBlockGetBuffer(block);
  lovrAssert(lovrBufferIsReadable(buffer), "ShaderBlock:read requires the ShaderBlock to be created with the readable flag");
  union { float* floats; int* ints; } data = { .floats = lovrBufferMap(buffer, uniform->offset, false) };
  int components = uniform->components;

  if (uniform->type == UNIFORM_MATRIX) {
    components *= components;
  }

  lua_createtable(L, uniform->count, 0);
  for (int i = 0; i < uniform->count; i++) {
    if (components == 1) {
      switch (uniform->type) {
        case UNIFORM_FLOAT:
          lua_pushnumber(L, data.floats[i]);
          lua_rawseti(L, -2, i + 1);
          break;
        case UNIFORM_INT:
          lua_pushinteger(L, data.ints[i]);
          lua_rawseti(L, -2, i + 1);
          break;
        default: break;
      }
    } else {
      lua_createtable(L, components, 0);
      for (int j = 0; j < components; j++) {
        switch (uniform->type) {
          case UNIFORM_FLOAT:
          case UNIFORM_MATRIX:
            lua_pushnumber(L, data.floats[i * components + j]);
            lua_rawseti(L, -2, j + 1);
            break;
          case UNIFORM_INT:
            lua_pushinteger(L, data.ints[i * components + j]);
            lua_rawseti(L, -2, j + 1);
            break;
          default: break;
        }
      }
      lua_rawseti(L, -2, i + 1);
    }
  }
  return 1;
}

static int l_lovrShaderBlockGetShaderCode(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  const char* blockName = luaL_checkstring(L, 2);
  const char* namespace = luaL_optstring(L, 3, NULL);
  size_t length;
  char* code = lovrShaderBlockGetShaderCode(block, blockName, namespace, &length);
  lua_pushlstring(L, code, length);
  free(code);
  return 1;
}

const luaL_Reg lovrShaderBlock[] = {
  { "getType", l_lovrShaderBlockGetType },
  { "getSize", l_lovrShaderBlockGetSize },
  { "getOffset", l_lovrShaderBlockGetOffset },
  { "read", l_lovrShaderBlockRead },
  { "send", l_lovrShaderBlockSend },
  { "getShaderCode", l_lovrShaderBlockGetShaderCode },
  { NULL, NULL }
};
