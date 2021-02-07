#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/soundData.h"
#include "core/ref.h"
#include "core/util.h"
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
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  lovrAudioStart(type);
  return 0;
}

static int l_lovrAudioStop(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
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
  bool spatial = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;

  if (soundData) {
    source = lovrSourceCreate(soundData, spatial);
  } else {
    Blob* blob = luax_readblob(L, 1, "Source");
    soundData = lovrSoundDataCreateFromFile(blob, false);
    lovrRelease(Blob, blob);

    source = lovrSourceCreate(soundData, spatial);
    lovrRelease(SoundData, soundData);
  }

  luax_pushtype(L, Source, source);
  lovrRelease(Source, source);
  return 1;
}

static int l_lovrAudioSetListenerPose(lua_State *L) {
  float position[4], orientation[4];
  int index = 1;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrAudioSetListenerPose(position, orientation);
  return 0;
}

static const luaL_Reg lovrAudio[] = {
  { "reset", l_lovrAudioReset },
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "newSource", l_lovrAudioNewSource },
  { "setListenerPose", l_lovrAudioSetListenerPose },
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
