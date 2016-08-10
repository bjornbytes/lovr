#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../graphics/model.h"

void luax_pushmodel(lua_State* L, Model* model);
Model* luax_checkmodel(lua_State* L, int index);
extern const luaL_Reg lovrModel[];
