#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/audioStream.h"
#include "data/soundData.h"
#include "core/maf.h"
#include "core/ref.h"
#include <stdlib.h>

StringEntry SourceTypes[] = {
  [SOURCE_STATIC] = ENTRY("static"),
  [SOURCE_STREAM] = ENTRY("stream"),
  { 0 }
};

StringEntry TimeUnits[] = {
  [UNIT_SECONDS] = ENTRY("seconds"),
  [UNIT_SAMPLES] = ENTRY("samples"),
  { 0 }
};

static int l_lovrAudioUpdate(lua_State* L) {
  lovrAudioUpdate();
  return 0;
}

static int l_lovrAudioGetDopplerEffect(lua_State* L) {
  float factor, speedOfSound;
  lovrAudioGetDopplerEffect(&factor, &speedOfSound);
  lua_pushnumber(L, factor);
  lua_pushnumber(L, speedOfSound);
  return 2;
}

static int l_lovrAudioGetMicrophoneNames(lua_State* L) {
  const char* names[MAX_MICROPHONES];
  uint32_t count;
  lovrAudioGetMicrophoneNames(names, &count);

  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_settop(L, 0);
    lua_createtable(L, count, 0);
  }

  for (uint32_t i = 0; i < count; i++) {
    lua_pushstring(L, names[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

static int l_lovrAudioGetOrientation(lua_State* L) {
  float orientation[4], angle, ax, ay, az;
  lovrAudioGetOrientation(orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrAudioGetPose(lua_State* L) {
  float position[4], orientation[4], angle, ax, ay, az;
  lovrAudioGetPosition(position);
  lovrAudioGetOrientation(orientation);
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

static int l_lovrAudioGetPosition(lua_State* L) {
  float position[4];
  lovrAudioGetPosition(position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrAudioGetVelocity(lua_State* L) {
  float velocity[4];
  lovrAudioGetVelocity(velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrAudioGetVolume(lua_State* L) {
  lua_pushnumber(L, lovrAudioGetVolume());
  return 1;
}

static int l_lovrAudioIsSpatialized(lua_State* L) {
  lua_pushboolean(L, lovrAudioIsSpatialized());
  return 1;
}

static int l_lovrAudioNewMicrophone(lua_State* L) {
  const char* name = luaL_optstring(L, 1, NULL);
  int samples = luaL_optinteger(L, 2, 1024);
  int sampleRate = luaL_optinteger(L, 3, 8000);
  int bitDepth = luaL_optinteger(L, 4, 16);
  int channelCount = luaL_optinteger(L, 5, 1);
  Microphone* microphone = lovrMicrophoneCreate(name, samples, sampleRate, bitDepth, channelCount);
  luax_pushtype(L, Microphone, microphone);
  lovrRelease(Microphone, microphone);
  return 1;
}

static int l_lovrAudioNewSource(lua_State* L) {
  Source* source = NULL;
  SoundData* soundData = luax_totype(L, 1, SoundData);
  AudioStream* stream = luax_totype(L, 1, AudioStream);
  bool isStatic = soundData || luax_checkenum(L, 2, SourceTypes, NULL, "SourceType") == SOURCE_STATIC;

  if (isStatic) {
    if (soundData) {
      source = lovrSourceCreateStatic(soundData);
    } else {
      if (stream) {
        soundData = lovrSoundDataCreateFromAudioStream(stream);
      } else {
        Blob* blob = luax_readblob(L, 1, "Source");
        soundData = lovrSoundDataCreateFromBlob(blob);
        lovrRelease(Blob, blob);
      }

      lovrAssert(soundData, "Could not create static Source");
      source = lovrSourceCreateStatic(soundData);
      lovrRelease(SoundData, soundData);
    }
  } else {
    if (stream) {
      source = lovrSourceCreateStream(stream);
    } else {
      Blob* blob = luax_readblob(L, 1, "Source");
      stream = lovrAudioStreamCreate(blob, 4096);
      lovrAssert(stream, "Could not create stream Source");
      source = lovrSourceCreateStream(stream);
      lovrRelease(Blob, blob);
      lovrRelease(AudioStream, stream);
    }
  }

  luax_pushtype(L, Source, source);
  lovrRelease(Source, source);
  return 1;
}

static int l_lovrAudioPause(lua_State* L) {
  lovrAudioPause();
  return 0;
}

static int l_lovrAudioSetDopplerEffect(lua_State* L) {
  float factor = luax_optfloat(L, 1, 1.f);
  float speedOfSound = luax_optfloat(L, 2, 343.29f);
  lovrAudioSetDopplerEffect(factor, speedOfSound);
  return 0;
}

static int l_lovrAudioSetOrientation(lua_State* L) {
  float orientation[4];
  luax_readquat(L, 1, orientation, NULL);
  lovrAudioSetOrientation(orientation);
  return 0;
}

static int l_lovrAudioSetPose(lua_State* L) {
  float position[4], orientation[4];
  int index = 1;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrAudioSetPosition(position);
  lovrAudioSetOrientation(orientation);
  return 0;
}

static int l_lovrAudioSetPosition(lua_State* L) {
  float position[4];
  luax_readvec3(L, 1, position, NULL);
  lovrAudioSetPosition(position);
  return 0;
}

static int l_lovrAudioSetVelocity(lua_State* L) {
  float velocity[4];
  luax_readvec3(L, 1, velocity, NULL);
  lovrAudioSetVelocity(velocity);
  return 0;
}

static int l_lovrAudioSetVolume(lua_State* L) {
  float volume = luax_checkfloat(L, 1);
  lovrAudioSetVolume(volume);
  return 0;
}

static int l_lovrAudioStop(lua_State* L) {
  lovrAudioStop();
  return 0;
}

static const luaL_Reg lovrAudio[] = {
  { "update", l_lovrAudioUpdate },
  { "getDopplerEffect", l_lovrAudioGetDopplerEffect },
  { "getMicrophoneNames", l_lovrAudioGetMicrophoneNames },
  { "getOrientation", l_lovrAudioGetOrientation },
  { "getPose", l_lovrAudioGetPose },
  { "getPosition", l_lovrAudioGetPosition },
  { "getVelocity", l_lovrAudioGetVelocity },
  { "getVolume", l_lovrAudioGetVolume },
  { "isSpatialized", l_lovrAudioIsSpatialized },
  { "newMicrophone", l_lovrAudioNewMicrophone },
  { "newSource", l_lovrAudioNewSource },
  { "pause", l_lovrAudioPause },
  { "setDopplerEffect", l_lovrAudioSetDopplerEffect },
  { "setOrientation", l_lovrAudioSetOrientation },
  { "setPose", l_lovrAudioSetPose },
  { "setPosition", l_lovrAudioSetPosition },
  { "setVelocity", l_lovrAudioSetVelocity },
  { "setVolume", l_lovrAudioSetVolume },
  { "stop", l_lovrAudioStop },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrAudio);
  luax_registertype(L, Microphone);
  luax_registertype(L, Source);
  if (lovrAudioInit()) {
    luax_atexit(L, lovrAudioDestroy);
  }
  return 1;
}
