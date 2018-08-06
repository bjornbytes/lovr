#include "api.h"
#include "graphics/shader.h"
#include "math/transform.h"

int l_lovrShaderBlockGetSize(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  lua_pushinteger(L, lovrShaderBlockGetSize(block));
  return 1;
}

int l_lovrShaderBlockIsWritable(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  lua_pushboolean(L, lovrShaderBlockGetType(block) == BLOCK_STORAGE);
  return 1;
}

int l_lovrShaderBlockSend(lua_State* L) {
  ShaderBlock* block = luax_checktype(L, 1, ShaderBlock);
  const char* name = luaL_checkstring(L, 2);
  const Uniform* uniform = lovrShaderBlockGetUniform(block, name);
  lovrAssert(uniform, "Unknown uniform for ShaderBlock '%s'", name);
  uint8_t* data = ((uint8_t*) lovrShaderBlockMap(block)) + uniform->offset;
  luax_checkuniform(L, 3, uniform, data, name);
  return 0;
}

const luaL_Reg lovrShaderBlock[] = {
  { "getSize", l_lovrShaderBlockGetSize },
  { "isWritable", l_lovrShaderBlockIsWritable },
  { "send", l_lovrShaderBlockSend },
  { NULL, NULL }
};
