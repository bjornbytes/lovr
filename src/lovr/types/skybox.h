#include "graphics/skybox.h"
#include "graphics/graphics.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrSkybox[];
int l_lovrSkyboxDraw(lua_State* L);
