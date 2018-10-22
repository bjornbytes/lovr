#include "api.h"
#include "data/audioStream.h"
#include "data/soundData.h"

int l_lovrAudioStreamDecode(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  int samples = lovrAudioStreamDecode(stream, NULL, 0);
  if (samples > 0) {
    SoundData* soundData = lovrSoundDataCreate(samples / stream->channelCount, stream->sampleRate, stream->bitDepth, stream->channelCount);
    memcpy(soundData->blob.data, stream->buffer, samples * (stream->bitDepth / 8));
    luax_pushobject(L, soundData);
    lovrRelease(soundData);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrAudioStreamGetBitDepth(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->bitDepth);
  return 1;
}

int l_lovrAudioStreamGetChannelCount(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->channelCount);
  return 1;
}

int l_lovrAudioStreamGetDuration(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushnumber(L, (float) stream->samples / stream->sampleRate);
  return 1;
}

int l_lovrAudioStreamGetSampleRate(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->sampleRate);
  return 1;
}

const luaL_Reg lovrAudioStream[] = {
  { "decode", l_lovrAudioStreamDecode },
  { "getBitDepth", l_lovrAudioStreamGetBitDepth },
  { "getChannelCount", l_lovrAudioStreamGetChannelCount },
  { "getDuration", l_lovrAudioStreamGetDuration },
  { "getSampleRate", l_lovrAudioStreamGetSampleRate },
  { NULL, NULL }
};
