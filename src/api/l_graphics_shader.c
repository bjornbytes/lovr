#include "api.h"
#include "graphics/graphics.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

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

  arr_t(ShaderFlag) flags;
  arr_init(&flags, realloc);

  while (lua_next(L, 2) != 0) {
    ShaderFlag flag = { 0 };
    flag.value = lua_isboolean(L, -1) ? (double) lua_toboolean(L, -1) : lua_tonumber(L, -1);
    switch (lua_type(L, -2)) {
      case LUA_TSTRING: flag.name = lua_tostring(L, -2); break;
      case LUA_TNUMBER: flag.id = lua_tointeger(L, -2); break;
      default: lovrThrow("Unexpected ShaderFlag key type (%s)", lua_typename(L, lua_type(L, -2)));
    }
    lovrCheck(flags.length < ~((uint32_t)0), "Too many flags");
    arr_push(&flags, flag);
    lua_pop(L, 1);
  }

  Shader* clone = lovrShaderClone(shader, flags.data, (uint32_t)flags.length);
  arr_free(&flags);

  luax_pushtype(L, Shader, clone);
  lovrRelease(clone, lovrShaderDestroy);
  return 1;
}

const luaL_Reg lovrShader[] = {
  { "getType", l_lovrShaderGetType },
  { "clone", l_lovrShaderClone },
  { NULL, NULL }
};
