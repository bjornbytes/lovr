#include "lovr/types/source.h"
#include "lovr/audio.h"

const luaL_Reg lovrSource[] = {
  { "getBitDepth", l_lovrSourceGetBitDepth },
  { "getChannels", l_lovrSourceGetChannels },
  { "getDuration", l_lovrSourceGetDuration },
  { "getSampleCount", l_lovrSourceGetSampleCount },
  { "getSampleRate", l_lovrSourceGetSampleRate },
  { "isPaused", l_lovrSourceIsPaused },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isStopped", l_lovrSourceIsStopped },
  { "pause", l_lovrSourcePause },
  { "play", l_lovrSourcePlay },
  { "resume", l_lovrSourceResume },
  { "rewind", l_lovrSourceRewind },
  { "stop", l_lovrSourceStop },
  { NULL, NULL }
};

int l_lovrSourceGetBitDepth(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetBitDepth(source));
  return 1;
}

int l_lovrSourceGetChannels(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetChannels(source));
  return 1;
}

int l_lovrSourceGetDuration(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetDuration(source));
  return 1;
}

int l_lovrSourceGetSampleCount(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetSampleCount(source));
  return 1;
}

int l_lovrSourceGetSampleRate(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetSampleRate(source));
  return 1;
}

int l_lovrSourceIsPaused(lua_State* L) {
  lua_pushnumber(L, lovrSourceIsPaused(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourceIsPlaying(lua_State* L) {
  lua_pushnumber(L, lovrSourceIsPlaying(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourceIsStopped(lua_State* L) {
  lua_pushnumber(L, lovrSourceIsStopped(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourcePause(lua_State* L) {
  lovrSourcePause(luax_checktype(L, 1, Source));
  return 0;
}

int l_lovrSourcePlay(lua_State* L) {
  lovrSourcePlay(luax_checktype(L, 1, Source));
  return 0;
}

int l_lovrSourceResume(lua_State* L) {
  lovrSourceResume(luax_checktype(L, 1, Source));
  return 0;
}

int l_lovrSourceRewind(lua_State* L) {
  lovrSourceRewind(luax_checktype(L, 1, Source));
  return 0;
}

int l_lovrSourceStop(lua_State* L) {
  lovrSourceStop(luax_checktype(L, 1, Source));
  return 0;
}
