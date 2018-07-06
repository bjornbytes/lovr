#include "api.h"
#include "data/soundData.h"

int l_lovrSoundDataGetBitDepth(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->bitDepth);
  return 1;
}

int l_lovrSoundDataGetChannelCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->channelCount);
  return 1;
}

int l_lovrSoundDataGetDuration(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushnumber(L, soundData->samples / (float) soundData->sampleRate);
  return 1;
}

int l_lovrSoundDataGetSampleCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->samples);
  return 1;
}

int l_lovrSoundDataGetSampleRate(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->sampleRate);
  return 1;
}

const luaL_Reg lovrSoundData[] = {
  { "getBitDepth", l_lovrSoundDataGetBitDepth },
  { "getChannelCount", l_lovrSoundDataGetChannelCount },
  { "getDuration", l_lovrSoundDataGetDuration },
  { "getSampleCount", l_lovrSoundDataGetSampleCount },
  { "getSampleRate", l_lovrSoundDataGetSampleRate },
  { NULL, NULL }
};
