#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../graphics/shader.h"

void luax_pushshader(lua_State* L, Shader* shader);
Shader* luax_checkshader(lua_State* L, int index);
int luax_destroyshader(lua_State* L);
extern const luaL_Reg lovrShader[];
