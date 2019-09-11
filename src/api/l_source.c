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
  lovrDecoderSeek(lovrSourceGetDecoder(source), 0); // This is safe because lovrSourcePause() is guaranteed to lock
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

static int l_lovrSourceGetPosition(lua_State* L) {
  float position[4];
  lovrSourceGetPosition(luax_checktype(L, 1, Source), position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrSourceRewind(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  Decoder* decoder = lovrSourceGetDecoder(source);
  lovrAudioLock();
  lovrDecoderSeek(decoder, 0);
  lovrAudioUnlock();
  return 0;
}

static int l_lovrSourceSeek(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luaL_checkoption(L, 3, "seconds", TimeUnits);
  Decoder* decoder = lovrSourceGetDecoder(source);

  lovrAudioLock();
  if (unit == TIME_SECONDS) {
    lovrDecoderSeek(decoder, (int) (luax_checkfloat(L, 2) * SAMPLE_RATE + .5f));
  } else {
    lovrDecoderSeek(decoder, luaL_checkinteger(L, 2));
  }
  lovrAudioUnlock();

  return 0;
}

static int l_lovrSourceSetPosition(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4];
  luax_readvec3(L, 2, position, NULL);
  lovrSourceSetPosition(source, position);
  return 0;
}

static int l_lovrSourceTell(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luaL_checkoption(L, 2, "seconds", TimeUnits);
  Decoder* decoder = lovrSourceGetDecoder(source);

  lovrAudioLock();
  uint32_t frame = lovrDecoderTell(decoder);
  lovrAudioUnlock();

  if (unit == TIME_SECONDS) {
    lua_pushnumber(L, frame / (float) SAMPLE_RATE);
  } else {
    lua_pushinteger(L, frame);
  }

  return 1;
}

const luaL_Reg lovrSource[] = {
  { "getPosition", l_lovrSourceGetPosition },
  { "play", l_lovrSourcePlay },
  { "pause", l_lovrSourcePause },
  { "stop", l_lovrSourceStop },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isLooping", l_lovrSourceIsLooping },
  { "setLooping", l_lovrSourceSetLooping },
  { "getVolume", l_lovrSourceGetVolume },
  { "setVolume", l_lovrSourceSetVolume },
  { "getDuration", l_lovrSourceGetDuration },
  { "rewind", l_lovrSourceRewind },
  { "seek", l_lovrSourceSeek },
  { "setPosition", l_lovrSourceSetPosition },
  { "tell", l_lovrSourceTell },
  { NULL, NULL }
};
