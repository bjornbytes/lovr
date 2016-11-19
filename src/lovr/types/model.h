#include "graphics/model.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrModel[];
int l_lovrModelDraw(lua_State* L);
int l_lovrModelGetTexture(lua_State* L);
int l_lovrModelSetTexture(lua_State* L);
