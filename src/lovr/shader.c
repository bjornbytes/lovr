#include "shader.h"

void luax_pushshader(lua_State* L, Shader* shader) {
  if (shader == NULL) {
    return lua_pushnil(L);
  }

  Shader** userdata = (Shader**) lua_newuserdata(L, sizeof(Shader*));
  luaL_getmetatable(L, "Shader");
  lua_setmetatable(L, -2);
  *userdata = shader;
}

Shader* luax_checkshader(lua_State* L, int index) {
  return *(Shader**) luaL_checkudata(L, index, "Shader");
}

int luax_destroyshader(lua_State* L) {
  Shader* shader = luax_checkshader(L, 1);
  lovrShaderDestroy(shader);
  return 0;
}

const luaL_Reg lovrShader[] = {
  { NULL, NULL }
};
