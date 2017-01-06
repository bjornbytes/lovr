#include "audio/source.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrSource[];
int l_lovrSourceGetBitDepth(lua_State* L);
int l_lovrSourceGetChannels(lua_State* L);
int l_lovrSourceGetDuration(lua_State* L);
int l_lovrSourceGetSampleCount(lua_State* L);
int l_lovrSourceGetSampleRate(lua_State* L);
int l_lovrSourceIsLooping(lua_State* L);
int l_lovrSourceIsPaused(lua_State* L);
int l_lovrSourceIsPlaying(lua_State* L);
int l_lovrSourceIsStopped(lua_State* L);
int l_lovrSourcePause(lua_State* L);
int l_lovrSourcePlay(lua_State* L);
int l_lovrSourceResume(lua_State* L);
int l_lovrSourceRewind(lua_State* L);
int l_lovrSourceSetLooping(lua_State* L);
int l_lovrSourceStop(lua_State* L);
