#include "api.h"
#include "audio/audio.h"
#include <stdbool.h>

int l_lovrSourceGetBitDepth(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetBitDepth(source));
  return 1;
}

int l_lovrSourceGetChannelCount(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetChannelCount(source));
  return 1;
}

int l_lovrSourceGetCone(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float innerAngle, outerAngle, outerGain;
  lovrSourceGetCone(source, &innerAngle, &outerAngle, &outerGain);
  lua_pushnumber(L, innerAngle);
  lua_pushnumber(L, outerAngle);
  lua_pushnumber(L, outerGain);
  return 3;
}

int l_lovrSourceGetDirection(lua_State* L) {
  float x, y, z;
  lovrSourceGetDirection(luax_checktype(L, 1, Source), &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
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

int l_lovrSourceGetFalloff(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float reference, max, rolloff;
  lovrSourceGetFalloff(source, &reference, &max, &rolloff);
  lua_pushnumber(L, reference);
  lua_pushnumber(L, max);
  lua_pushnumber(L, rolloff);
  return 3;
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

int l_lovrSourceGetVelocity(lua_State* L) {
  float x, y, z;
  lovrSourceGetVelocity(luax_checktype(L, 1, Source), &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrSourceGetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetVolume(source));
  return 1;
}

int l_lovrSourceGetVolumeLimits(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float min, max;
  lovrSourceGetVolumeLimits(source, &min, &max);
  lua_pushnumber(L, min);
  lua_pushnumber(L, max);
  return 2;
}

int l_lovrSourceIsLooping(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsLooping(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourceIsPaused(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsPaused(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourceIsPlaying(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsPlaying(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourceIsRelative(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsRelative(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourceIsStopped(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsStopped(luax_checktype(L, 1, Source)));
  return 1;
}

int l_lovrSourcePause(lua_State* L) {
  lovrSourcePause(luax_checktype(L, 1, Source));
  return 0;
}

int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePlay(source);
  lovrAudioAdd(source);
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

int l_lovrSourceSetCone(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float innerAngle = luaL_checknumber(L, 1);
  float outerAngle = luaL_checknumber(L, 2);
  float outerGain = luaL_checknumber(L, 3);
  lovrSourceSetCone(source, innerAngle, outerAngle, outerGain);
  return 0;
}

int l_lovrSourceSetFalloff(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float reference = luaL_checknumber(L, 2);
  float max = luaL_checknumber(L, 3);
  float rolloff = luaL_checknumber(L, 4);
  lovrSourceSetFalloff(source, reference, max, rolloff);
  return 0;
}

int l_lovrSourceSetLooping(lua_State* L) {
  lovrSourceSetLooping(luax_checktype(L, 1, Source), lua_toboolean(L, 2));
  return 0;
}

int l_lovrSourceSetDirection(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrSourceSetDirection(source, x, y, z);
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

int l_lovrSourceSetRelative(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool isRelative = lua_toboolean(L, 2);
  lovrSourceSetRelative(source, isRelative);
  return 0;
}

int l_lovrSourceSetVelocity(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrSourceSetVelocity(source, x, y, z);
  return 0;
}

int l_lovrSourceSetVolume(lua_State* L) {
  lovrSourceSetVolume(luax_checktype(L, 1, Source), luaL_checknumber(L, 2));
  return 0;
}

int l_lovrSourceSetVolumeLimits(lua_State* L) {
  lovrSourceSetVolumeLimits(luax_checktype(L, 1, Source), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
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

const luaL_Reg lovrSource[] = {
  { "getBitDepth", l_lovrSourceGetBitDepth },
  { "getChannelCount", l_lovrSourceGetChannelCount },
  { "getCone", l_lovrSourceGetCone },
  { "getDirection", l_lovrSourceGetDirection },
  { "getDuration", l_lovrSourceGetDuration },
  { "getFalloff", l_lovrSourceGetFalloff },
  { "getPitch", l_lovrSourceGetPitch },
  { "getPosition", l_lovrSourceGetPosition },
  { "getSampleRate", l_lovrSourceGetSampleRate },
  { "getVelocity", l_lovrSourceGetVelocity },
  { "getVolume", l_lovrSourceGetVolume },
  { "getVolumeLimits", l_lovrSourceGetVolumeLimits },
  { "isLooping", l_lovrSourceIsLooping },
  { "isPaused", l_lovrSourceIsPaused },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isRelative", l_lovrSourceIsRelative },
  { "isStopped", l_lovrSourceIsStopped },
  { "pause", l_lovrSourcePause },
  { "play", l_lovrSourcePlay },
  { "resume", l_lovrSourceResume },
  { "rewind", l_lovrSourceRewind },
  { "seek", l_lovrSourceSeek },
  { "setCone", l_lovrSourceSetCone },
  { "setDirection", l_lovrSourceSetDirection },
  { "setFalloff", l_lovrSourceSetFalloff },
  { "setLooping", l_lovrSourceSetLooping },
  { "setPitch", l_lovrSourceSetPitch },
  { "setPosition", l_lovrSourceSetPosition },
  { "setRelative", l_lovrSourceSetRelative },
  { "setVelocity", l_lovrSourceSetVelocity },
  { "setVolume", l_lovrSourceSetVolume },
  { "setVolumeLimits", l_lovrSourceSetVolumeLimits },
  { "stop", l_lovrSourceStop },
  { "tell", l_lovrSourceTell },
  { NULL, NULL }
};
