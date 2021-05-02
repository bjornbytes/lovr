#include "api.h"
#include "graphics/graphics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrShaderGetType(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  const ShaderInfo* info = lovrShaderGetInfo(shader);
  luax_pushenum(L, ShaderType, info->type);
  return 1;
}

static int l_lovrShaderCompute(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  if (lua_isuserdata(L, 2)) {
    Buffer* params = luax_checktype(L, 2, Buffer);
    uint32_t offset = luaL_optinteger(L, 3, 0);
    lovrShaderComputeIndirect(shader, params, offset);
  } else {
    uint32_t x = luaL_optinteger(L, 2, 1);
    uint32_t y = luaL_optinteger(L, 3, 1);
    uint32_t z = luaL_optinteger(L, 4, 1);
    lovrShaderCompute(shader, x, y, z);
  }
  return 0;
}

const luaL_Reg lovrShader[] = {
  { "getType", l_lovrShaderGetType },
  { "compute", l_lovrShaderCompute },
  { NULL, NULL }
};
