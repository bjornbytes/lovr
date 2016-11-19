#include "graphics/shader.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

void luax_pushshader(lua_State* L, Shader* shader);
Shader* luax_checkshader(lua_State* L, int index);
int luax_destroyshader(lua_State* L);
extern const luaL_Reg lovrShader[];

int l_lovrShaderSend(lua_State* L);
