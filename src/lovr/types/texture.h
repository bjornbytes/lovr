#include "graphics/texture.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrTexture[];
int l_lovrTextureGetDimensions(lua_State* L);
int l_lovrTextureGetHeight(lua_State* L);
int l_lovrTextureGetFilter(lua_State* L);
int l_lovrTextureGetWidth(lua_State* L);
int l_lovrTextureGetWrap(lua_State* L);
int l_lovrTextureSetFilter(lua_State* L);
int l_lovrTextureSetWrap(lua_State* L);
