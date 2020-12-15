#include "api.h"
#include "data/soundData.h"
#include "data/blob.h"

static int l_lovrSoundDataGetBlob(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = soundData->blob;
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrSoundDataAppend(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = luax_totype(L, 2, Blob);
  SoundData* sound = luax_totype(L, 2, SoundData);
  size_t appendedSamples = 0;
  if (sound) {
    appendedSamples = lovrSoundDataStreamAppendSound(soundData, sound);
  } else if (blob) {
    appendedSamples = lovrSoundDataStreamAppendBlob(soundData, blob);
  } else {
    luaL_argerror(L, 2, "Invalid blob appended");
  }
  lua_pushinteger(L, appendedSamples);
  return 1;
}

static int l_lovrSoundDataSetSample(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  int index = luaL_checkinteger(L, 2);
  float value = luax_checkfloat(L, 3);
  lovrSoundDataSetSample(soundData, index, value);
  return 0;
}

const luaL_Reg lovrSoundData[] = {
  { "getBlob", l_lovrSoundDataGetBlob },
  { "append", l_lovrSoundDataAppend },
  { "setSample", l_lovrSoundDataSetSample },
  { NULL, NULL }
};
