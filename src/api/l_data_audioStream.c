#include "api.h"
#include "data/audioStream.h"
#include "data/soundData.h"
#include "core/ref.h"
#include <string.h>
#include <stdlib.h>

static int l_lovrAudioStreamDecode(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  size_t samples = lovrAudioStreamDecode(stream, NULL, 0);
  if (samples > 0) {
    SoundData* soundData = lovrSoundDataCreate(samples / stream->channelCount, stream->sampleRate, stream->bitDepth, stream->channelCount);
    memcpy(soundData->blob->data, stream->buffer, samples * (stream->bitDepth / 8));
    luax_pushtype(L, SoundData, soundData);
    lovrRelease(SoundData, soundData);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrAudioStreamGetBitDepth(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->bitDepth);
  return 1;
}

static int l_lovrAudioStreamGetChannelCount(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->channelCount);
  return 1;
}

static int l_lovrAudioStreamGetDuration(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushnumber(L, (float)lovrAudioStreamGetDurationInSeconds(stream));
  return 1;
}

static int l_lovrAudioStreamGetSampleRate(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, stream->sampleRate);
  return 1;
}

static int l_lovrAudioStreamAppend(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  Blob* blob = luax_totype(L, 2, Blob);
  SoundData* sound = luax_totype(L, 2, SoundData);
  lovrAssert(blob || sound, "Invalid blob appended");
  bool success = false;
  if (sound) {
    success = lovrAudioStreamAppendRawSound(stream, sound);
  } else if (blob) {
    success = lovrAudioStreamAppendRawBlob(stream, blob);
  }
  lua_pushboolean(L, success);
  return 1;
}

const luaL_Reg lovrAudioStream[] = {
  { "decode", l_lovrAudioStreamDecode },
  { "getBitDepth", l_lovrAudioStreamGetBitDepth },
  { "getChannelCount", l_lovrAudioStreamGetChannelCount },
  { "getDuration", l_lovrAudioStreamGetDuration },
  { "getSampleRate", l_lovrAudioStreamGetSampleRate },
  { "append", l_lovrAudioStreamAppend},
  { NULL, NULL }
};
