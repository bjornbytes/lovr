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

static int l_lovrShaderClone(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  luaL_checktype(L, 2, LUA_TTABLE);
  lua_pushnil(L);
  uint32_t count = 0;
  ShaderFlag flags[32];
  while (lua_next(L, 2) != 0) {
    lovrCheck(count < COUNTOF(flags), "Too many shader flags (max is %d), please report this encounter", COUNTOF(flags));
    double value = lua_isboolean(L, -1) ? (double) lua_toboolean(L, -1) : lua_tonumber(L, -1);
    switch (lua_type(L, -2)) {
      case LUA_TSTRING:
        flags[count++] = (ShaderFlag) { .name = lua_tostring(L, -2), .value = value };
        break;
      case LUA_TNUMBER:
        flags[count++] = (ShaderFlag) { .id = lua_tointeger(L, -2), .value = value };
        break;
      default: lovrThrow("Unexpected ShaderFlag key type (%s)", lua_typename(L, lua_type(L, -2)));
    }
    lua_pop(L, 1);
  }
  Shader* clone = lovrShaderClone(shader, flags, count);
  luax_pushtype(L, Shader, clone);
  lovrRelease(clone, lovrShaderDestroy);
  return 1;
}

const luaL_Reg lovrShader[] = {
  { "clone", l_lovrShaderClone },
  { "getType", l_lovrShaderGetType },
  { NULL, NULL }
};
