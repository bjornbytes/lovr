#include "api.h"
#include "data/blob.h"
#include "data/sound.h"
#include "util.h"

static int l_lovrAudioStreamGetFormat(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  luax_pushenum(L, SampleFormat, lovrSoundGetFormat(lovrAudioStreamGetSound(stream)));
  return 1;
}

static int l_lovrAudioStreamGetChannelLayout(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  switch (lovrSoundGetChannelCount(lovrAudioStreamGetSound(stream))) {
    case 1: luax_pushenum(L, ChannelLayout, CHANNEL_MONO); break;
    case 2: luax_pushenum(L, ChannelLayout, CHANNEL_STEREO); break;
    case 4: case 9: case 16: luax_pushenum(L, ChannelLayout, CHANNEL_AMBISONIC); break;
    default: lua_pushnil(L);
  }
  return 1;
}

static int l_lovrAudioStreamGetChannelCount(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, lovrSoundGetChannelCount(lovrAudioStreamGetSound(stream)));
  return 1;
}

static int l_lovrAudioStreamGetSampleRate(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  lua_pushinteger(L, lovrSoundGetSampleRate(lovrAudioStreamGetSound(stream)));
  return 1;
}

static int l_lovrAudioStreamGetCapacity(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  Sound* sound = lovrAudioStreamGetSound(stream);
  lua_pushinteger(L, lovrSoundGetBlob(sound)->size / lovrSoundGetStride(sound));
  return 1;
}

static int l_lovrAudioStreamGetReadCapacity(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  uint32_t capacity = lovrAudioStreamGetReadCapacity(stream);
  lua_pushinteger(L, capacity);
  return 1;
}

static int l_lovrAudioStreamGetWriteCapacity(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  uint32_t capacity = lovrAudioStreamGetWriteCapacity(stream);
  lua_pushinteger(L, capacity);
  return 1;
}

static int l_lovrAudioStreamRead(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);
  SampleFormat format = lovrSoundGetFormat(lovrAudioStreamGetSound(stream));
  uint32_t channels = lovrSoundGetChannelCount(lovrAudioStreamGetSound(stream));
  size_t stride = lovrSoundGetStride(lovrAudioStreamGetSound(stream));
  uint32_t count = luax_optu32(L, 2, lovrAudioStreamGetReadCapacity(stream));
  Blob* blob = luax_totype(L, 3, Blob);
  Sound* sound = luax_totype(L, 3, Sound);
  uint32_t offset = luax_optu32(L, 4, 0);

  if (blob) {
    count = (uint32_t) MIN(count, (blob->size - offset) / stride);
  } else if (sound) {
    count = MIN(count, lovrSoundGetFrameCount(sound) - offset);
  }

  if (blob) {
    count = lovrAudioStreamRead(stream, count, (char*) blob->data + offset);
    lua_pushinteger(L, count);
  } else if (sound) {
    count = lovrAudioStreamRead(stream, count, (char*) lovrSoundGetBlob(sound)->data + offset * stride);
    lua_pushinteger(L, count);
  } else {
    lua_createtable(L, count * channels, 0);
    char buffer[4096];
    int index = 1;

    while (count > 0) {
      uint32_t chunk = MIN(count, (uint32_t) (sizeof(buffer) / stride));
      chunk = lovrAudioStreamRead(stream, chunk, buffer);

      if (format == SAMPLE_I16) {
        int16_t* i16 = (int16_t*) buffer;
        for (uint32_t i = 0; i < chunk; i++) {
          for (uint32_t c = 0; c < channels; c++) {
            lua_pushinteger(L, *i16++);
            lua_rawseti(L, -2, index++);
          }
        }
      } else {
        float* f32 = (float*) buffer;
        for (uint32_t i = 0; i < chunk; i++) {
          for (uint32_t c = 0; c < channels; c++) {
            lua_pushnumber(L, *f32++);
            lua_rawseti(L, -2, index++);
          }
        }
      }

      count -= chunk;
    }
  }

  return 1;
}

static int l_lovrAudioStreamWrite(lua_State* L) {
  AudioStream* stream = luax_checktype(L, 1, AudioStream);

  uint32_t capacity = lovrAudioStreamGetWriteCapacity(stream);
  uint32_t channels = lovrSoundGetChannelCount(lovrAudioStreamGetSound(stream));
  SampleFormat format = lovrSoundGetFormat(lovrAudioStreamGetSound(stream));
  size_t stride = lovrSoundGetStride(lovrAudioStreamGetSound(stream));

  if (lua_istable(L, 2)) {
    uint32_t count = MIN(luax_len(L, 2) / channels, capacity);
    uint32_t framesWritten = 0;
    char buffer[4096];
    int index = 1;

    while (framesWritten < count) {
      uint32_t chunk = MIN(count - framesWritten, (uint32_t) (sizeof(buffer) / stride));

      if (format == SAMPLE_I16) {
        int16_t* i16 = (int16_t*) buffer;
        for (uint32_t i = 0; i < chunk; i++) {
          for (uint32_t c = 0; c < channels; c++) {
            lua_rawgeti(L, 2, index++);
            *i16++ = lua_tointeger(L, -1);
            lua_pop(L, 1);
          }
        }
      } else {
        float* f32 = (float*) buffer;
        for (uint32_t i = 0; i < chunk; i++) {
          for (uint32_t c = 0; c < channels; c++) {
            lua_rawgeti(L, 2, index++);
            *f32++ = luax_tofloat(L, -1);
            lua_pop(L, 1);
          }
        }
      }

      framesWritten += lovrAudioStreamWrite(stream, chunk, buffer);
    }

    lua_pushinteger(L, framesWritten);
    return 1;
  }

  Blob* blob = luax_totype(L, 2, Blob);

  if (blob) {
    uint32_t count = (uint32_t) MIN(blob->size / stride, capacity);
    count = lovrAudioStreamWrite(stream, count, blob->data);
    lua_pushinteger(L, count);
    return 1;
  }

  Sound* sound = luax_totype(L, 2, Sound);

  if (sound) {
    luax_check(L, lovrSoundGetChannelCount(sound) == channels, "Sound channel count must match stream channel count");
    luax_check(L, lovrSoundGetFormat(sound) == format, "Sound format must match stream format");
    uint32_t count = MIN(lovrSoundGetFrameCount(sound), capacity);
    count = lovrAudioStreamWrite(stream, count, lovrSoundGetBlob(sound)->data);
    lua_pushinteger(L, count);
    return 1;
  }

  return luax_typeerror(L, 2, "table, Blob, or Sound");
}

const luaL_Reg lovrAudioStream[] = {
  { "getFormat", l_lovrAudioStreamGetFormat },
  { "getChannelLayout", l_lovrAudioStreamGetChannelLayout },
  { "getChannelCount", l_lovrAudioStreamGetChannelCount },
  { "getSampleRate", l_lovrAudioStreamGetSampleRate },
  { "getCapacity", l_lovrAudioStreamGetCapacity },
  { "getReadCapacity", l_lovrAudioStreamGetReadCapacity },
  { "getWriteCapacity", l_lovrAudioStreamGetWriteCapacity },
  { "read", l_lovrAudioStreamRead },
  { "write", l_lovrAudioStreamWrite },
  { NULL, NULL }
};
