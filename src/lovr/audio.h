#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrAudio[];
int l_lovrAudioInit(lua_State* L);
int l_lovrAudioUpdate(lua_State* L);
int l_lovrAudioNewSource(lua_State* L);
