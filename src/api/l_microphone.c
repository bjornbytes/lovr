#include "api.h"
#include "audio/microphone.h"
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
  Microphone* microphone = luax_checktype(L, 1, Microphone);
  SoundData* soundData = lovrMicrophoneGetData(microphone);
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
