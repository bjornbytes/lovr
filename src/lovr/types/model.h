#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../../graphics/model.h"

void luax_pushmodel(lua_State* L, Model* model);
Model* luax_checkmodel(lua_State* L, int index);
int luax_destroymodel(lua_State* L);
extern const luaL_Reg lovrModel[];

int l_lovrModelDraw(lua_State* L);
