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

const luaL_Reg lovrShader[] = {
  { "getType", l_lovrShaderGetType },
  { NULL, NULL }
};
