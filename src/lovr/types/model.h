#include "graphics/model.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

void luax_pushmodel(lua_State* L, Model* model);
Model* luax_checkmodel(lua_State* L, int index);
extern const luaL_Reg lovrModel[];

int l_lovrModelDraw(lua_State* L);
int l_lovrModelGetTexture(lua_State* L);
int l_lovrModelSetTexture(lua_State* L);
