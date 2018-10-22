#include "api.h"
#include "api/graphics.h"
#include "graphics/shader.h"
#include "math/transform.h"

int l_lovrShaderBlockIsWritable(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  lua_pushboolean(L, lovrShaderBlockGetType(block) == BLOCK_STORAGE);
  return 1;
}

int l_lovrShaderBlockGetSize(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  lua_pushinteger(L, lovrShaderBlockGetSize(block));
  return 1;
}

int l_lovrShaderBlockGetOffset(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  const char* field = luaL_checkstring(L, 2);
  const Uniform* uniform = lovrShaderBlockGetUniform(block, field);
  lua_pushinteger(L, uniform->offset);
  return 1;
}

int l_lovrShaderBlockSend(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  if (lua_type(L, 2) == LUA_TSTRING) {
    const char* name = luaL_checkstring(L, 2);
    const Uniform* uniform = lovrShaderBlockGetUniform(block, name);
    lovrAssert(uniform, "Unknown uniform for ShaderBlock '%s'", name);
    uint8_t* data = ((uint8_t*) lovrShaderBlockMap(block)) + uniform->offset;
    luax_checkuniform(L, 3, uniform, data, name);
    return 0;
  } else {
    Blob* blob = luax_checktype(L, 1, Blob);
    void* data = lovrShaderBlockMap(block);
    size_t blockSize = lovrShaderBlockGetSize(block);
    size_t copySize = MIN(blockSize, blob->size);
    memcpy(data, blob->data, copySize);
    lua_pushinteger(L, copySize);
    return 1;
  }
}

int l_lovrShaderBlockGetShaderCode(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  const char* blockName = luaL_checkstring(L, 2);
  size_t length;
  char* code = lovrShaderBlockGetShaderCode(block, blockName, &length);
  lua_pushlstring(L, code, length);
  return 1;
}

const luaL_Reg lovrShaderBlock[] = {
  { "isWritable", l_lovrShaderBlockIsWritable },
  { "getSize", l_lovrShaderBlockGetSize },
  { "getOffset", l_lovrShaderBlockGetOffset },
  { "send", l_lovrShaderBlockSend },
  { "getShaderCode", l_lovrShaderBlockGetShaderCode },
  { NULL, NULL }
};
