#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef const struct aiMesh Model;
void luax_pushmodel(lua_State* L, Model* model);
Model* luax_checkmodel(lua_State* L, int index);

int lovrModelDraw(lua_State* L);
int lovrModelGetVertexCount(lua_State* L);
int lovrModelGetVertexColors(lua_State* L);

extern const luaL_Reg lovrModel[];
