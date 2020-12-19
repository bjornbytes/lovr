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

#define AUDIO_SPATIALIZER_MAX_SOURCES_HINT 16

static int l_lovrAudioReset(lua_State* L) {
  lovrAudioReset();
  return 0;
}

static int l_lovrAudioStart(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool  started = lovrAudioStart(type);
  lua_pushboolean(L, started);
  return 1;
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
  
  bool spatial = true;
  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "spatial");
    if (lua_isboolean(L, -1)) {
      spatial = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
  }
  

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

static int l_lovrAudioGetCaptureDuration(lua_State *L) {
  TimeUnit units = luax_checkenum(L, 1, TimeUnit, "seconds");
  size_t sampleCount = lovrAudioGetCaptureSampleCount();
  
  if (units == UNIT_SECONDS) {
    lua_pushnumber(L, (double) sampleCount / LOVR_AUDIO_SAMPLE_RATE);
  } else {
    lua_pushinteger(L, sampleCount);
  }
  return 1;
}

static int l_lovrAudioCapture(lua_State* L) {
  int index = 1;
  
  size_t samples = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : lovrAudioGetCaptureSampleCount();

  if (samples == 0) {
    return 0;
  }

  SoundData* soundData = luax_totype(L, index++, SoundData);
  size_t offset = soundData ? luaL_optinteger(L, index, 0) : 0;

  if (soundData) {
    lovrRetain(soundData);
  }

  soundData = lovrAudioCapture(samples, soundData, offset);
  luax_pushtype(L, SoundData, soundData);
  lovrRelease(SoundData, soundData);
  return 1;
}

static int l_lovrAudioSetCaptureDevice(lua_State *L) {
  //

  return 0;
}

static int l_lovrAudioGetCaptureDevice(lua_State *L) {
  //

  return 0;
}

static int l_lovrAudioListCaptureDevices(lua_State *L) {
  //
  
  return 0;
}

static int l_lovrAudioGetSpatializerName(lua_State *L) {
  lua_pushstring(L, lovrSourceGetSpatializerName());
  return 1;
}

static const luaL_Reg lovrAudio[] = {
  { "reset", l_lovrAudioReset },
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "newSource", l_lovrAudioNewSource },
  { "setListenerPose", l_lovrAudioSetListenerPose },
  { "capture", l_lovrAudioCapture },
  { "getCaptureDuration", l_lovrAudioGetCaptureDuration },
  { "setCaptureDevice", l_lovrAudioSetCaptureDevice },
  { "getCaptureDevice", l_lovrAudioGetCaptureDevice },
  { "listCaptureDevices", l_lovrAudioListCaptureDevices },
  { "getSpatializerName", l_lovrAudioGetSpatializerName },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrAudio);
  luax_registertype(L, Source);

  char *spatializer = NULL;
  int spatializerMaxSourcesHint = AUDIO_SPATIALIZER_MAX_SOURCES_HINT;
  luax_pushconf(L);
  lua_getfield(L, -1, "audio");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "spatializerMaxSourcesHint");
    if (lua_type(L, -1) == LUA_TNUMBER)
      spatializerMaxSourcesHint = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "spatializer");
    if (lua_type(L, -1) == LUA_TSTRING)
      spatializer = lua_tostring(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  AudioConfig config = {
    .spatializer = spatializer,
    .spatializerMaxSourcesHint=spatializerMaxSourcesHint
  };

  // First config is for output device, second is for input device
  AudioDeviceConfig deviceConfig[2] = {
    { .enable = true, .start = true },
    { .enable = false, .start = false }
  };
  if (lovrAudioInit(config, deviceConfig)) {
    luax_atexit(L, lovrAudioDestroy);
  }
  return 1;
}
