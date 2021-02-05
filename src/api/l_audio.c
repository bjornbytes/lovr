#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/soundData.h"
#include "core/maf.h"
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
  [UNIT_FRAMES] = ENTRY("frames"),
  { 0 }
};

static void onDevice(AudioDevice* device, void* userdata) {
  lua_State* L = userdata;
  lua_createtable(L, 0, 3);
  void* id = lua_newuserdata(L, device->idSize);
  memcpy(id, device->id, device->idSize);
  lua_setfield(L, -2, "id");
  lua_pushstring(L, device->name);
  lua_setfield(L, -2, "name");
  lua_pushboolean(L, device->isDefault);
  lua_setfield(L, -2, "default");
  lua_rawseti(L, -2, luax_len(L, -2) + 1);
}

static int l_lovrAudioGetDevices(lua_State *L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  lua_newtable(L);
  lovrAudioEnumerateDevices(type, onDevice, L);
  return 1;
}

static int l_lovrAudioSetDevice(lua_State *L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  void* id = lua_touserdata(L, 2);
  size_t size = luax_len(L, 2);
  uint32_t sampleRate = lua_tointeger(L, 3);
  SampleFormat format = luax_checkenum(L, 4, SampleFormat, "f32");
  bool success = lovrAudioSetDevice(type, id, size, sampleRate, format);
  lua_pushboolean(L, success);
  return 1;
}

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
  int index = 1;
  float position[4], orientation[4];
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
  { "getDevices", l_lovrAudioGetDevices },
  { "setDevice", l_lovrAudioSetDevice },
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "isStarted", l_lovrAudioIsStarted },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "getPose", l_lovrAudioGetPose },
  { "setPose", l_lovrAudioSetPose },
  { "getCaptureStream", l_lovrAudioGetCaptureStream },
  { "getSpatializer", l_lovrAudioGetSpatializer },
  { "newSource", l_lovrAudioNewSource },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrAudio);
  luax_registertype(L, Source);

  const char *spatializer = NULL;
  luax_pushconf(L);
  lua_getfield(L, -1, "audio");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "spatializer");
    spatializer = lua_tostring(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 2);

  if (lovrAudioInit(spatializer)) {
    lovrAudioSetDevice(AUDIO_PLAYBACK, NULL, 0, PLAYBACK_SAMPLE_RATE, SAMPLE_F32);
    lovrAudioStart(AUDIO_PLAYBACK);
    luax_atexit(L, lovrAudioDestroy);
  }

  return 1;
}
