#include "api.h"
#include "audio/audio.h"
#include "data/soundData.h"

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePlay(source);
  return 0;
}

static int l_lovrSourcePause(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePause(source);
  return 0;
}

static int l_lovrSourceStop(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourceStop(source);
  return 0;
}

static int l_lovrSourceIsPlaying(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushboolean(L, lovrSourceIsPlaying(source));
  return 1;
}

static int l_lovrSourceIsLooping(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushboolean(L, lovrSourceIsLooping(source));
  return 1;
}

static int l_lovrSourceSetLooping(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourceSetLooping(source, lua_toboolean(L, 2));
  return 0;
}

static int l_lovrSourceGetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetVolume(source));
  return 1;
}

static int l_lovrSourceSetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float volume = luax_checkfloat(L, 2);
  lovrSourceSetVolume(source, volume);
  return 0;
}

static int l_lovrSourceIsSpatial(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushboolean(L, lovrSourceGetSpatial(source));
  return 1;
}

static int l_lovrSourceSetPose(lua_State *L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4];
  int index = 2;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrSourceSetPose(source, position, orientation);
  return 0;
}

static int l_lovrSourceGetDuration(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  SoundData* sound = lovrSourceGetSoundData(source);
  uint32_t frames = lovrSoundDataGetDuration(sound);
  if (units == UNIT_SECONDS) {
    lua_pushnumber(L, (double) frames / sound->sampleRate);
  } else {
    lua_pushinteger(L, frames);
  }

  return 1;
}

static int l_lovrSourceGetTime(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  uint32_t offset = lovrSourceGetTime(source);

  if (units == UNIT_SECONDS) {
    SoundData* sound = lovrSourceGetSoundData(source);
    lua_pushnumber(L, (double) offset / sound->sampleRate);
  } else {
    lua_pushinteger(L, offset);
  }

  return 1;
}

static int l_lovrSourceSetTime(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 3, TimeUnit, "seconds");

  if (units == UNIT_SECONDS) {
    double seconds = luaL_checknumber(L, 2);
    SoundData* sound = lovrSourceGetSoundData(source);
    lovrSourceSetTime(source, (uint32_t) (seconds * sound->sampleRate + .5));
  } else {
    lovrSourceSetTime(source, luaL_checkinteger(L, 2));
  }

  return 0;
}

const luaL_Reg lovrSource[] = {
  { "play", l_lovrSourcePlay },
  { "pause", l_lovrSourcePause },
  { "stop", l_lovrSourceStop },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isLooping", l_lovrSourceIsLooping },
  { "setLooping", l_lovrSourceSetLooping },
  { "getVolume", l_lovrSourceGetVolume },
  { "setVolume", l_lovrSourceSetVolume },
  { "isSpatial", l_lovrSourceIsSpatial },
  { "setPose", l_lovrSourceSetPose },
  { "getDuration", l_lovrSourceGetDuration },
  { "getTime", l_lovrSourceGetTime },
  { "setTime", l_lovrSourceSetTime },
  { NULL, NULL }
};
