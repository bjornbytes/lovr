#include "api.h"
#include "audio/audio.h"
#include "data/soundData.h"
#include "core/ref.h"
#include <stdlib.h>

static int l_lovrMicrophoneGetBitDepth(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lua_pushinteger(L, lovrMicrophoneGetBitDepth(microphone));
  return 1;
}

static int l_lovrMicrophoneGetChannelCount(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lua_pushinteger(L, lovrMicrophoneGetChannelCount(microphone));
  return 1;
}

static int l_lovrMicrophoneGetData(lua_State* L) {
  int index = 1;
  Microphone* microphone = luax_checktype(L, index++, Microphone);
  size_t samples = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : lovrMicrophoneGetSampleCount(microphone);

  if (samples == 0) {
    return 0;
  }

  SoundData* soundData = luax_totype(L, index++, SoundData);
  size_t offset = soundData ? luaL_optinteger(L, index, 0) : 0;

  if (soundData) {
    lovrRetain(soundData);
  }

  soundData = lovrMicrophoneGetData(microphone, samples, soundData, offset);
  luax_pushtype(L, SoundData, soundData);
  lovrRelease(SoundData, soundData);
  return 1;
}

static int l_lovrMicrophoneGetName(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lua_pushstring(L, lovrMicrophoneGetName(microphone));
  return 1;
}

static int l_lovrMicrophoneGetSampleCount(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lua_pushinteger(L, lovrMicrophoneGetSampleCount(microphone));
  return 1;
}

static int l_lovrMicrophoneGetSampleRate(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lua_pushinteger(L, lovrMicrophoneGetSampleRate(microphone));
  return 1;
}

static int l_lovrMicrophoneIsRecording(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lua_pushboolean(L, lovrMicrophoneIsRecording(microphone));
  return 1;
}

static int l_lovrMicrophoneStartRecording(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lovrMicrophoneStartRecording(microphone);
  return 0;
}

static int l_lovrMicrophoneStopRecording(lua_State* L) {
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  lovrMicrophoneStopRecording(microphone);
  return 0;
}

const luaL_Reg lovrMicrophone[] = {
  { "getBitDepth", l_lovrMicrophoneGetBitDepth },
  { "getChannelCount", l_lovrMicrophoneGetChannelCount },
  { "getData", l_lovrMicrophoneGetData },
  { "getName", l_lovrMicrophoneGetName },
  { "getSampleCount", l_lovrMicrophoneGetSampleCount },
  { "getSampleRate", l_lovrMicrophoneGetSampleRate },
  { "isRecording", l_lovrMicrophoneIsRecording },
  { "startRecording", l_lovrMicrophoneStartRecording },
  { "stopRecording", l_lovrMicrophoneStopRecording },
  { NULL, NULL }
};
