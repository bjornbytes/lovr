#include "api.h"
#include "data/soundData.h"
#include "data/blob.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>

StringEntry lovrSampleFormat[] = {
  [SAMPLE_F32] = ENTRY("f32"),
  [SAMPLE_I16] = ENTRY("i16"),
  { 0 }
};

static int l_lovrSoundDataGetFormat(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  luax_pushenum(L, SampleFormat, soundData->format);
  return 1;
}

static int l_lovrSoundDataGetSampleRate(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->sampleRate);
  return 1;
}

static int l_lovrSoundDataGetChannelCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, soundData->channels);
  return 1;
}

static int l_lovrSoundDataGetFrameCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  uint32_t frames = lovrSoundDataGetFrameCount(soundData);
  lua_pushinteger(L, frames);
  return 1;
}

static int l_lovrSoundDataIsCompressed(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  bool compressed = lovrSoundDataIsCompressed(soundData);
  lua_pushboolean(L, compressed);
  return 1;
}

static int l_lovrSoundDataIsStream(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  bool stream = lovrSoundDataIsStream(soundData);
  lua_pushboolean(L, stream);
  return 1;
}

static int l_lovrSoundDataGetBlob(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = soundData->blob;
  luax_pushtype(L, Blob, blob);
  return 1;
}

// soundData:read(dest, {size}, {offset}) -> framesRead
// soundData:read({size}) -> framesRead
static int l_lovrSoundDataRead(lua_State* L) {
  //struct SoundData* soundData, uint32_t offset, uint32_t count, void* data
  SoundData* source = luax_checktype(L, 1, SoundData);
  int index = 2;
  SoundData* dest = luax_totype(L, index, SoundData);
  if (dest) index++;
  size_t frameCount = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : lovrSoundDataGetFrameCount(source);
  size_t offset = dest ? luaL_optinteger(L, index, 0) : 0;
  bool shouldRelease = false;
  if (dest == NULL) {
    dest = lovrSoundDataCreateRaw(frameCount, source->channels, source->sampleRate, source->format, NULL);
    shouldRelease = true;
  } else {
    lovrAssert(dest->channels == source->channels, "Source (%d) and destination (%d) channel counts must match", source->channels, dest->channels);
    lovrAssert(dest->sampleRate == source->sampleRate, "Source (%d) and destination (%d) sample rates must match", source->sampleRate, dest->sampleRate);
    lovrAssert(dest->format == source->format, "Source (%s) and destination (%s) formats must match", lovrSampleFormat[source->format].string, lovrSampleFormat[dest->format].string);
    lovrAssert(offset + frameCount <= dest->frames, "Tried to write samples past the end of a SoundData buffer");
    lovrAssert(dest->blob, "Can't read into a stream destination");
  }

  size_t outFrames = source->read(source, offset, frameCount, dest->blob->data);
  dest->frames = outFrames;
  dest->blob->size = outFrames * lovrSoundDataGetStride(dest);
  luax_pushtype(L, SoundData, dest);
  if (shouldRelease) lovrRelease(SoundData, dest);

  return 1;
}

static int l_lovrSoundDataAppend(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = luax_totype(L, 2, Blob);
  SoundData* sound = luax_totype(L, 2, SoundData);
  size_t appendedSamples = 0;
  if (sound) {
    appendedSamples = lovrSoundDataStreamAppendSound(soundData, sound);
  } else if (blob) {
    appendedSamples = lovrSoundDataStreamAppendBlob(soundData, blob);
  } else {
    luaL_argerror(L, 2, "Invalid blob appended");
  }
  lua_pushinteger(L, appendedSamples);
  return 1;
}

static int l_lovrSoundDataGetSample(lua_State* L) {
  return 0;
}

static int l_lovrSoundDataSetSample(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  int index = luaL_checkinteger(L, 2);
  float value = luax_checkfloat(L, 3);
  lovrSoundDataSetSample(soundData, index, value);
  return 0;
}

const luaL_Reg lovrSoundData[] = {
  { "getFormat", l_lovrSoundDataGetFormat },
  { "getSampleRate", l_lovrSoundDataGetSampleRate },
  { "getChannelCount", l_lovrSoundDataGetChannelCount },
  { "getFrameCount", l_lovrSoundDataGetFrameCount },
  { "isCompressed", l_lovrSoundDataIsCompressed },
  { "isStream", l_lovrSoundDataIsStream },

  { "getBlob", l_lovrSoundDataGetBlob },
  { "read", l_lovrSoundDataRead },
  { "append", l_lovrSoundDataAppend },
  { "getSample", l_lovrSoundDataGetSample },
  { "setSample", l_lovrSoundDataSetSample },
  { NULL, NULL }
};
