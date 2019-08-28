#include "api.h"
#include "data/soundData.h"

static int l_lovrSoundDataGetChannelCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->channels);
  return 1;
}

static int l_lovrSoundDataGetDuration(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
 lua_pushnumber(L, soundData->frames / (float) SAMPLE_RATE);
 return 1;
}
 
static int l_lovrSoundDataGetFrameCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->frames);
  return 1;
}

const luaL_Reg lovrSoundData[] = {
  { "getChannelCount", l_lovrSoundDataGetChannelCount },
  { "getDuration", l_lovrSoundDataGetDuration },
  { "getFrameCount", l_lovrSoundDataGetFrameCount },
  { NULL, NULL }
};
