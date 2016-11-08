#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../../graphics/texture.h"

void luax_pushtexture(lua_State* L, Texture* texture);
Texture* luax_checktexture(lua_State* L, int index);
int luax_destroytexture(lua_State* L);
extern const luaL_Reg lovrTexture[];

int l_lovrTextureGetDimensions(lua_State* L);
int l_lovrTextureGetHeight(lua_State* L);
int l_lovrTextureGetWidth(lua_State* L);
