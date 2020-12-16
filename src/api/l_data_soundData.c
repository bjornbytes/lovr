#include "api.h"
#include "data/soundData.h"
#include "data/blob.h"
#include "core/ref.h"
#include <stdlib.h>

StringEntry lovrTimeUnit[] = {
  [UNIT_SECONDS] = ENTRY("seconds"),
  [UNIT_SAMPLES] = ENTRY("samples"),
  { 0 }
};

static int l_lovrSoundDataGetBlob(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = soundData->blob;
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrSoundDataGetDuration(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  uint32_t frames = lovrSoundDataGetDuration(soundData);
  if (units == UNIT_SECONDS) {
    lua_pushnumber(L, (double) frames / soundData->sampleRate);
  } else {
    lua_pushinteger(L, frames);
  }

  return 1;
}

static const char *format2string(SampleFormat f) { return f == SAMPLE_I16 ? "i16" : "f32"; }

// soundData:read(dest, {size}, {offset}) -> framesRead
// soundData:read({size}) -> framesRead
static int l_lovrSoundDataRead(lua_State* L) {
  //struct SoundData* soundData, uint32_t offset, uint32_t count, void* data
  SoundData* source = luax_checktype(L, 1, SoundData);
  int index = 2;
  SoundData* dest = luax_totype(L, index, SoundData);
  size_t frameCount = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : lovrSoundDataGetDuration(source);
  size_t offset = dest ? luaL_optinteger(L, index, 0) : 0;
  bool shouldRelease = false;
  if (dest == NULL) {
    dest = lovrSoundDataCreateRaw(frameCount, source->channels, source->sampleRate, source->format, NULL);
    shouldRelease = true;
  } else {
    lovrAssert(dest->channels == source->channels, "Source (%d) and destination (%d) channel counts must match", source->channels, dest->channels);
    lovrAssert(dest->sampleRate == source->sampleRate, "Source (%d) and destination (%d) sample rates must match", source->sampleRate, dest->sampleRate);
    lovrAssert(dest->format == source->format, "Source (%s) and destination (%s) formats must match", format2string(source->format), format2string(dest->format));
    lovrAssert(offset + frameCount <= dest->frames, "Tried to write samples past the end of a SoundData buffer");
    lovrAssert(dest->blob, "Can't read into a stream destination");
  }

  size_t outFrames = source->read(source, offset, frameCount, dest->blob->data);
  dest->frames = outFrames;
  dest->blob->size = outFrames * SampleFormatBytesPerFrame(dest->channels, dest->format);
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

static int l_lovrSoundDataSetSample(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  int index = luaL_checkinteger(L, 2);
  float value = luax_checkfloat(L, 3);
  lovrSoundDataSetSample(soundData, index, value);
  return 0;
}

const luaL_Reg lovrSoundData[] = {
  { "getBlob", l_lovrSoundDataGetBlob },
  { "getDuration", l_lovrSoundDataGetDuration },
  { "read", l_lovrSoundDataRead },
  { "append", l_lovrSoundDataAppend },
  { "setSample", l_lovrSoundDataSetSample },
  { NULL, NULL }
};
