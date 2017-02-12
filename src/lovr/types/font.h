#include "graphics/font.h"
#include "luax.h"

extern const luaL_Reg lovrFont[];
int l_lovrFontGetWidth(lua_State* L);
int l_lovrFontGetHeight(lua_State* L);
int l_lovrFontGetAscent(lua_State* L);
int l_lovrFontGetDescent(lua_State* L);
int l_lovrFontGetBaseline(lua_State* L);
int l_lovrFontGetLineHeight(lua_State* L);
int l_lovrFontSetLineHeight(lua_State* L);
