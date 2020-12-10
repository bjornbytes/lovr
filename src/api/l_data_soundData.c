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
  lovrAssert(blob || sound, "Invalid blob appended");
  size_t appendedSamples = false;
  if (sound) {
    appendedSamples = lovrSoundDataStreamAppendSound(soundData, sound);
  } else if (blob) {
    appendedSamples = lovrSoundDataStreamAppendBlob(soundData, blob);
  }
  lua_pushinteger(L, appendedSamples);
  return 1;
}

const luaL_Reg lovrSoundData[] = {
  { "getBlob", l_lovrSoundDataGetBlob },
  { "append", l_lovrSoundDataAppend },
  { NULL, NULL }
};
