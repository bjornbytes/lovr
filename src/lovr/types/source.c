#include "lovr/types/source.h"
#include "lovr/audio.h"

const luaL_Reg lovrSource[] = {
  { "getBitDepth", l_lovrSourceGetBitDepth },
  { "getChannels", l_lovrSourceGetChannels },
  { "getDuration", l_lovrSourceGetDuration },
  { "getPitch", l_lovrSourceGetPitch },
  { "getPosition", l_lovrSourceGetPosition },
  { "getSampleRate", l_lovrSourceGetSampleRate },
  { "getVolume", l_lovrSourceGetVolume },
  { "isLooping", l_lovrSourceIsLooping },
  { "isPaused", l_lovrSourceIsPaused },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isStopped", l_lovrSourceIsStopped },
  { "pause", l_lovrSourcePause },
  { "play", l_lovrSourcePlay },
  { "resume", l_lovrSourceResume },
  { "rewind", l_lovrSourceRewind },
  { "seek", l_lovrSourceSeek },
  { "setLooping", l_lovrSourceSetLooping },
  { "setPitch", l_lovrSourceSetPitch },
  { "setPosition", l_lovrSourceSetPosition },
  { "setVolume", l_lovrSourceSetVolume },
  { "stop", l_lovrSourceStop },
  { "tell", l_lovrSourceTell },
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
  TimeUnit* unit = luax_optenum(L, 2, "seconds", &TimeUnits, "unit");
  int duration = lovrSourceGetDuration(source);

  if (*unit == UNIT_SECONDS) {
    lua_pushnumber(L, (float) duration / lovrSourceGetSampleRate(source));
  } else {
    lua_pushinteger(L, duration);
  }

  return 1;
}

int l_lovrSourceGetPitch(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetPitch(source));
  return 1;
}

int l_lovrSourceGetPosition(lua_State* L) {
  float x, y, z;
  lovrSourceGetPosition(luax_checktype(L, 1, Source), &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrSourceGetSampleRate(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetSampleRate(source));
  return 1;
}

int l_lovrSourceGetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetVolume(source));
  return 1;
}

int l_lovrSourceIsLooping(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsLooping(luax_checktype(L, 1, Source)));
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

int l_lovrSourceSeek(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit* unit = luax_optenum(L, 3, "seconds", &TimeUnits, "unit");

  if (*unit == UNIT_SECONDS) {
    float seconds = luaL_checknumber(L, 2);
    int sampleRate = lovrSourceGetSampleRate(source);
    lovrSourceSeek(source, (int) (seconds * sampleRate + .5f));
  } else {
    lovrSourceSeek(source, luaL_checkinteger(L, 2));
  }

  return 0;
}

int l_lovrSourceSetLooping(lua_State* L) {
  lovrSourceSetLooping(luax_checktype(L, 1, Source), lua_toboolean(L, 2));
  return 0;
}

int l_lovrSourceSetPitch(lua_State* L) {
  lovrSourceSetPitch(luax_checktype(L, 1, Source), luaL_checknumber(L, 2));
  return 0;
}

int l_lovrSourceSetPosition(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrSourceSetPosition(source, x, y, z);
  return 0;
}

int l_lovrSourceSetVolume(lua_State* L) {
  lovrSourceSetVolume(luax_checktype(L, 1, Source), luaL_checknumber(L, 2));
  return 0;
}

int l_lovrSourceStop(lua_State* L) {
  lovrSourceStop(luax_checktype(L, 1, Source));
  return 0;
}

int l_lovrSourceTell(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit* unit = luax_optenum(L, 2, "seconds", &TimeUnits, "unit");
  int offset = lovrSourceTell(source);

  if (*unit == UNIT_SECONDS) {
    lua_pushnumber(L, (float) offset / lovrSourceGetSampleRate(source));
  } else {
    lua_pushinteger(L, offset);
  }

  return 1;
}
