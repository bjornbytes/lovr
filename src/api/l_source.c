#include "api.h"
#include "audio/audio.h"
#include "audio/source.h"
#include "core/maf.h"
#include "modules/data/soundData.h"
#include <stdbool.h>

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePlay(source);
  return 0;
}

static int l_lovrSourcePause(lua_State* L) {
  lovrSourcePause(luax_checktype(L, 1, Source));
  return 0;
}

static int l_lovrSourceStop(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePause(source);
  lovrDecoderSeek(lovrSourceGetDecoder(source), 0);
  return 0;
}

static int l_lovrSourceIsPlaying(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsPlaying(luax_checktype(L, 1, Source)));
  return 1;
}

static int l_lovrSourceIsLooping(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsLooping(luax_checktype(L, 1, Source)));
  return 1;
}

static int l_lovrSourceSetLooping(lua_State* L) {
  lovrSourceSetLooping(luax_checktype(L, 1, Source), lua_toboolean(L, 2));
  return 0;
}

static int l_lovrSourceGetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetVolume(source));
  return 1;
}

static int l_lovrSourceSetVolume(lua_State* L) {
  lovrSourceSetVolume(luax_checktype(L, 1, Source), luax_checkfloat(L, 2));
  return 0;
}

static int l_lovrSourceGetDuration(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luaL_checkoption(L, 2, "seconds", TimeUnits);
  Decoder* decoder = lovrSourceGetDecoder(source);

  if (unit == TIME_SECONDS) {
    lua_pushnumber(L, decoder->frames / (float) SAMPLE_RATE);
  } else {
    lua_pushinteger(L, decoder->frames);
  }

  return 1;
}

static int l_lovrSourceSeek(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luaL_checkoption(L, 3, "seconds", TimeUnits);
  Decoder* decoder = lovrSourceGetDecoder(source);

  if (unit == TIME_SECONDS) {
    lovrDecoderSeek(decoder, (int) (luax_checkfloat(L, 2) * SAMPLE_RATE + .5f));
  } else {
    lovrDecoderSeek(decoder, luaL_checkinteger(L, 2));
  }

  return 0;
}

static int l_lovrSourceTell(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luaL_checkoption(L, 2, "seconds", TimeUnits);
  Decoder* decoder = lovrSourceGetDecoder(source);
  uint32_t frame = lovrDecoderTell(decoder);

  if (unit == TIME_SECONDS) {
    lua_pushnumber(L, frame / (float) SAMPLE_RATE);
  } else {
    lua_pushinteger(L, frame);
  }

  return 1;
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
  { "getDuration", l_lovrSourceGetDuration },
  { "seek", l_lovrSourceSeek },
  { "tell", l_lovrSourceTell },
  { NULL, NULL }
};
