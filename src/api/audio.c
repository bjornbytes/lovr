#include "api.h"
#include "api/data.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/soundData.h"
#include <stdlib.h>

const char* TimeUnits[] = {
  [TIME_FRAMES] = "frames",
  [TIME_SECONDS] = "seconds",
  NULL
};

static int l_lovrAudioNewSource(lua_State* L) {
  SoundData* soundData = luax_totype(L, 1, SoundData);
  Decoder* decoder = soundData ? lovrDecoderCreateRaw(soundData) : NULL;

  if (!decoder) {
    Blob* blob = luax_readblob(L, 1, "Sound");
    bool stream = lua_toboolean(L, 2);

    if (stream) {
      decoder = lovrDecoderCreateOgg(blob);
    } else {
      SoundData* soundData = lovrSoundDataCreateFromBlob(blob);
      decoder = lovrDecoderCreateRaw(soundData);
      lovrRelease(SoundData, soundData);
    }
  }

  Source* source = lovrSourceCreate(decoder);
  luax_pushobject(L, source);
  lovrRelease(Source, source);
  lovrRelease(Decoder, decoder);
  return 1;
}

static const luaL_Reg lovrAudio[] = {
  { "newSource", l_lovrAudioNewSource },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrAudio);
  luax_registertype(L, Source);
  if (lovrAudioInit()) {
    luax_atexit(L, lovrAudioDestroy);
  }
  return 1;
}
