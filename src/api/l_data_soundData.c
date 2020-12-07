#include "api.h"
#include "data/soundData.h"
#include "data/blob.h"

static int l_lovrSoundDataGetBlob(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = soundData->blob;
  luax_pushtype(L, Blob, blob);
  return 1;
}


const luaL_Reg lovrSoundData[] = {
  { "getBlob", l_lovrSoundDataGetBlob },
  { NULL, NULL }
};
