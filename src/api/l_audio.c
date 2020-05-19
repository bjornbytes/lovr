#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/soundData.h"
#include "core/ref.h"
#include <stdlib.h>

StringEntry lovrAudioType[] = {
  [AUDIO_PLAYBACK] = ENTRY("playback"),
  [AUDIO_CAPTURE] = ENTRY("capture"),
  { 0 }
};

StringEntry lovrTimeUnit[] = {
  [UNIT_SECONDS] = ENTRY("seconds"),
  [UNIT_SAMPLES] = ENTRY("samples"),
  { 0 }
};

static int l_lovrAudioReset(lua_State* L) {
  lovrAudioReset();
  return 0;
}

static int l_lovrAudioStart(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioTypes, "playback", "AudioType");
  lovrAudioStart(type);
  return 0;
}

static int l_lovrAudioStop(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioTypes, "playback", "AudioType");
  lovrAudioStop(type);
  return 0;
}

static int l_lovrAudioGetVolume(lua_State* L) {
  lua_pushnumber(L, lovrAudioGetVolume());
  return 1;
}

static int l_lovrAudioSetVolume(lua_State* L) {
  float volume = luax_checkfloat(L, 1);
  lovrAudioSetVolume(volume);
  return 0;
}

static int l_lovrAudioNewSource(lua_State* L) {
  Source* source = NULL;
  SoundData* soundData = luax_totype(L, 1, SoundData);

  if (soundData) {
    source = lovrSourceCreate(soundData);
  } else {
    Blob* blob = luax_readblob(L, 1, "Source");
    soundData = lovrSoundDataCreateFromFile(blob, false);
    lovrRelease(Blob, blob);

    source = lovrSourceCreate(soundData);
    lovrRelease(SoundData, soundData);
  }

  luax_pushtype(L, Source, source);
  lovrRelease(Source, source);
  return 1;
}

static const luaL_Reg lovrAudio[] = {
  { "reset", l_lovrAudioReset },
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "newSource", l_lovrAudioNewSource },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrAudio);
  luax_registertype(L, Source);
  AudioConfig config[2] = { { .enable = true, .start = true }, { .enable = true, .start = true } };
  if (lovrAudioInit(config)) {
    luax_atexit(L, lovrAudioDestroy);
  }
  return 1;
}
