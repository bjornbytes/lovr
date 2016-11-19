#include "graphics/skybox.h"
#include "graphics/graphics.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

void luax_pushskybox(lua_State* L, Skybox* skybox);
Skybox* luax_checkskybox(lua_State* L, int index);
extern const luaL_Reg lovrSkybox[];

int l_lovrSkyboxDraw(lua_State* L);
