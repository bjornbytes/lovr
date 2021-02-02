#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/soundData.h"
#include "core/maf.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>

#define AUDIO_SPATIALIZER_MAX_SOURCES_HINT 16

StringEntry lovrAudioType[] = {
  [AUDIO_PLAYBACK] = ENTRY("playback"),
  [AUDIO_CAPTURE] = ENTRY("capture"),
  { 0 }
};

static int l_lovrAudioStart(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool started = lovrAudioStart(type);
  lua_pushboolean(L, started);
  return 1;
}

static int l_lovrAudioStop(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  lovrAudioStop(type);
  return 0;
}

static int l_lovrAudioIsStarted(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool started = lovrAudioIsStarted(type);
  lua_pushboolean(L, started);
  return 1;
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

static int l_lovrAudioGetPose(lua_State *L) {
  float position[4], orientation[4], angle, ax, ay, az;
  lovrAudioGetPose(position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrAudioSetPose(lua_State *L) {
  float position[4], orientation[4];
  int index = 1;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrAudioSetPose(position, orientation);
  return 0;
}

static int l_lovrAudioGetCaptureStream(lua_State* L) {
  SoundData* soundData = lovrAudioGetCaptureStream();
  luax_pushtype(L, SoundData, soundData);
  return 1;
}

static int l_lovrAudioGetDevices(lua_State *L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  AudioDeviceArr *devices = lovrAudioGetDevices(type);

  lua_newtable(L);
  int top = lua_gettop(L);
  for (size_t i = 0; i < devices->length; i++) {
    AudioDevice *device = &devices->data[i];
    lua_newtable(L);
    luax_pushenum(L, AudioType, device->type);
    lua_setfield(L, -2, "type");
    lua_pushstring(L, device->name);
    lua_setfield(L, -2, "name");
    lua_pushboolean(L, device->isDefault);
    lua_setfield(L, -2, "isDefault");
    lua_rawseti(L, top, i + 1);
  }

  lovrAudioFreeDevices(devices);
  return 1;
}

static int l_lovrAudioUseDevice(lua_State *L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  const char *name = lua_tostring(L, 2);
  lovrAudioUseDevice(type, name);
  return 0;
}

static int l_lovrAudioSetCaptureFormat(lua_State *L) {
  SampleFormat format = luax_checkenum(L, 1, SampleFormat, "invalid");
  int sampleRate = lua_tointeger(L, 2);
  lovrAudioSetCaptureFormat(format, sampleRate);
  return 0;
}

static int l_lovrAudioGetSpatializer(lua_State *L) {
  lua_pushstring(L, lovrAudioGetSpatializer());
  return 1;
}

static int l_lovrAudioNewSource(lua_State* L) {
  Source* source = NULL;
  SoundData* soundData = luax_totype(L, 1, SoundData);

  bool spatial = true;
  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "spatial");
    spatial = lua_isnil(L, -1) || lua_toboolean(L, -1);
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

static const luaL_Reg lovrAudio[] = {
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "isStarted", l_lovrAudioIsStarted },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "getPose", l_lovrAudioGetPose },
  { "setPose", l_lovrAudioSetPose },
  { "getCaptureStream", l_lovrAudioGetCaptureStream },
  { "getDevices", l_lovrAudioGetDevices },
  { "useDevice", l_lovrAudioUseDevice },
  { "setCaptureFormat", l_lovrAudioSetCaptureFormat },
  { "getSpatializer", l_lovrAudioGetSpatializer },
  { "newSource", l_lovrAudioNewSource },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrAudio);
  luax_registertype(L, Source);

  const char *spatializer = NULL;
  int spatializerMaxSourcesHint = AUDIO_SPATIALIZER_MAX_SOURCES_HINT;
  luax_pushconf(L);
  lua_getfield(L, -1, "audio");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "spatializerMaxSourcesHint");
    if (lua_type(L, -1) == LUA_TNUMBER) {
      spatializerMaxSourcesHint = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "spatializer");
    if (lua_type(L, -1) == LUA_TSTRING) {
      spatializer = lua_tostring(L, -1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  SpatializerConfig config = {
    .spatializer = spatializer,
    .spatializerMaxSourcesHint=spatializerMaxSourcesHint
  };

  if (lovrAudioInit(config)) {
    lovrAudioStart(AUDIO_PLAYBACK);
    luax_atexit(L, lovrAudioDestroy);
  }

  return 1;
}
