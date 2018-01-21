#include "api.h"
#include "data/audioStream.h"

int l_lovrAudioStreamGetBitDepth(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->bitDepth);
  return 1;
}

int l_lovrAudioStreamGetChannelCount(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->channelCount);
  return 1;
}

int l_lovrAudioStreamGetDuration(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushnumber(L, (float) stream->samples / stream->sampleRate);
  return 1;
}

int l_lovrAudioStreamGetSampleRate(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->sampleRate);
  return 1;
}

int l_lovrAudioStreamSeek(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  float seconds = luaL_checknumber(L, 2);
  lovrAudioStreamSeek(stream, (int) (seconds * stream->sampleRate + .5));
  return 0;
}

const luaL_Reg lovrAudioStream[] = {
  { "getBitDepth", l_lovrAudioStreamGetBitDepth },
  { "getChannelCount", l_lovrAudioStreamGetChannelCount },
  { "getDuration", l_lovrAudioStreamGetDuration },
  { "getSampleRate", l_lovrAudioStreamGetSampleRate },
  { "seek", l_lovrAudioStreamSeek },
  { NULL, NULL }
};
