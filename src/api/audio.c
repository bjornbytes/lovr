#include "api.h"
#include "audio/audio.h"
#include "audio/source.h"
#include "data/audioStream.h"

const char* SourceTypes[] = {
  [SOURCE_STATIC] = "static",
  [SOURCE_STREAM] = "stream",
  NULL
};

const char* TimeUnits[] = {
  [UNIT_SECONDS] = "seconds",
  [UNIT_SAMPLES] = "samples",
  NULL
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
  uint8_t count;
  lovrAudioGetMicrophoneNames(names, &count);

  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_settop(L, 0);
    lua_createtable(L, count, 0);
  }

  for (int i = 0; i < count; i++) {
    lua_pushstring(L, names[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

static int l_lovrAudioGetOrientation(lua_State* L) {
  float angle, ax, ay, az;
  lovrAudioGetOrientation(&angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrAudioGetPosition(lua_State* L) {
  float x, y, z;
  lovrAudioGetPosition(&x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

static int l_lovrAudioGetVelocity(lua_State* L) {
  float x, y, z;
  lovrAudioGetVelocity(&x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
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
  luax_pushobject(L, microphone);
  lovrRelease(microphone);
  return 1;
}

static int l_lovrAudioNewSource(lua_State* L) {
  Source* source = NULL;
  SoundData* soundData = luax_totype(L, 1, SoundData);
  AudioStream* stream = luax_totype(L, 1, AudioStream);
  bool isStatic = soundData || luaL_checkoption(L, 2, NULL, SourceTypes) == SOURCE_STATIC;

  if (isStatic) {
    if (soundData) {
      source = lovrSourceCreateStatic(soundData);
    } else {
      if (stream) {
        soundData = lovrSoundDataCreateFromAudioStream(stream);
      } else {
        Blob* blob = luax_readblob(L, 1, "Source");
        soundData = lovrSoundDataCreateFromBlob(blob);
        lovrRelease(blob);
      }

      lovrAssert(soundData, "Could not create static Source");
      source = lovrSourceCreateStatic(soundData);
      lovrRelease(soundData);
    }
  } else {
    if (stream) {
      source = lovrSourceCreateStream(stream);
    } else {
      Blob* blob = luax_readblob(L, 1, "Source");
      stream = lovrAudioStreamCreate(blob, 4096);
      lovrAssert(stream, "Could not create stream Source");
      source = lovrSourceCreateStream(stream);
      lovrRelease(blob);
      lovrRelease(stream);
    }
  }

  luax_pushobject(L, source);
  lovrRelease(source);
  return 1;
}

static int l_lovrAudioPause(lua_State* L) {
  lovrAudioPause();
  return 0;
}

static int l_lovrAudioResume(lua_State* L) {
  lovrAudioResume();
  return 0;
}

static int l_lovrAudioRewind(lua_State* L) {
  lovrAudioRewind();
  return 0;
}

static int l_lovrAudioSetDopplerEffect(lua_State* L) {
  float factor = luaL_optnumber(L, 1, 1.);
  float speedOfSound = luaL_optnumber(L, 2, 343.29);
  lovrAudioSetDopplerEffect(factor, speedOfSound);
  return 0;
}

static int l_lovrAudioSetOrientation(lua_State* L) {
  float angle = luaL_checknumber(L, 1);
  float ax = luaL_checknumber(L, 2);
  float ay = luaL_checknumber(L, 3);
  float az = luaL_checknumber(L, 4);
  lovrAudioSetOrientation(angle, ax, ay, az);
  return 0;
}

static int l_lovrAudioSetPosition(lua_State* L) {
  float x = luaL_checknumber(L, 1);
  float y = luaL_checknumber(L, 2);
  float z = luaL_checknumber(L, 3);
  lovrAudioSetPosition(x, y, z);
  return 0;
}

static int l_lovrAudioSetVelocity(lua_State* L) {
  float x = luaL_checknumber(L, 1);
  float y = luaL_checknumber(L, 2);
  float z = luaL_checknumber(L, 3);
  lovrAudioSetVelocity(x, y, z);
  return 0;
}

static int l_lovrAudioSetVolume(lua_State* L) {
  float volume = luaL_checknumber(L, 1);
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
  { "getPosition", l_lovrAudioGetPosition },
  { "getVelocity", l_lovrAudioGetVelocity },
  { "getVolume", l_lovrAudioGetVolume },
  { "isSpatialized", l_lovrAudioIsSpatialized },
  { "newMicrophone", l_lovrAudioNewMicrophone },
  { "newSource", l_lovrAudioNewSource },
  { "pause", l_lovrAudioPause },
  { "resume", l_lovrAudioResume },
  { "rewind", l_lovrAudioRewind },
  { "setDopplerEffect", l_lovrAudioSetDopplerEffect },
  { "setOrientation", l_lovrAudioSetOrientation },
  { "setPosition", l_lovrAudioSetPosition },
  { "setVelocity", l_lovrAudioSetVelocity },
  { "setVolume", l_lovrAudioSetVolume },
  { "stop", l_lovrAudioStop },
  { NULL, NULL }
};

int luaopen_lovr_audio(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrAudio);
  luax_atexit(L, lovrAudioDestroy);
  luax_registertype(L, "Microphone", lovrMicrophone);
  luax_registertype(L, "Source", lovrSource);
  lovrAudioInit();
  return 1;
}
