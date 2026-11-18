#include "api.h"
#include "data/sound.h"
#include "data/blob.h"
#include "util.h"

StringEntry lovrSampleFormat[] = {
  [SAMPLE_F32] = ENTRY("f32"),
  [SAMPLE_I16] = ENTRY("i16"),
  { 0 }
};

StringEntry lovrChannelLayout[] = {
  [CHANNEL_MONO] = ENTRY("mono"),
  [CHANNEL_STEREO] = ENTRY("stereo"),
  [CHANNEL_AMBISONIC] = ENTRY("ambisonic"),
  { 0 }
};

static int l_lovrSoundGetBlob(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  Blob* blob = lovrSoundGetBlob(sound);
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrSoundGetFormat(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  luax_pushenum(L, SampleFormat, lovrSoundGetFormat(sound));
  return 1;
}

static int l_lovrSoundGetChannelLayout(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  switch (lovrSoundGetChannelCount(sound)) {
    case 1: luax_pushenum(L, ChannelLayout, CHANNEL_MONO); break;
    case 2: luax_pushenum(L, ChannelLayout, CHANNEL_STEREO); break;
    case 4: case 9: case 16: luax_pushenum(L, ChannelLayout, CHANNEL_AMBISONIC); break;
    default: lua_pushnil(L);
  }
  return 1;
}

static int l_lovrSoundGetChannelCount(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  lua_pushinteger(L, lovrSoundGetChannelCount(sound));
  return 1;
}

static int l_lovrSoundGetSampleRate(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  lua_pushinteger(L, lovrSoundGetSampleRate(sound));
  return 1;
}

static int l_lovrSoundGetByteStride(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  size_t stride = lovrSoundGetStride(sound);
  lua_pushinteger(L, stride);
  return 1;
}

static int l_lovrSoundGetFrameCount(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frames = lovrSoundGetFrameCount(sound);
  lua_pushinteger(L, frames);
  return 1;
}

static int l_lovrSoundGetSampleCount(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frames = lovrSoundGetFrameCount(sound);
  uint32_t channels = lovrSoundGetChannelCount(sound);
  lua_pushinteger(L, frames * channels);
  return 1;
}

static int l_lovrSoundGetDuration(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frames = lovrSoundGetFrameCount(sound);
  uint32_t rate = lovrSoundGetSampleRate(sound);
  lua_pushnumber(L, (double) frames / rate);
  return 1;
}

static int l_lovrSoundIsCompressed(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  bool compressed = lovrSoundIsCompressed(sound);
  lua_pushboolean(L, compressed);
  return 1;
}

static int l_lovrSoundGetFrame(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frame = luax_checku32(L, 2);
  uint32_t channels = lovrSoundGetChannelCount(sound);
  luax_check(L, frame < lovrSoundGetFrameCount(sound), "Frame offset is out of range");

  if (lovrSoundGetFormat(sound) == SAMPLE_I16) {
    int16_t samples[MAX_CHANNELS];
    lovrSoundRead(sound, frame, 1, samples);
    for (uint32_t c = 0; c < channels; c++) {
      lua_pushinteger(L, samples[c]);
    }
  } else {
    float samples[MAX_CHANNELS];
    lovrSoundRead(sound, frame, 1, samples);
    for (uint32_t c = 0; c < channels; c++) {
      lua_pushnumber(L, samples[c]);
    }
  }

  return channels;
}

static int l_lovrSoundSetFrame(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frame = luax_checku32(L, 2);
  uint32_t channels = lovrSoundGetChannelCount(sound);
  luax_check(L, frame < lovrSoundGetFrameCount(sound), "Frame offset is out of range");

  if (lovrSoundGetFormat(sound) == SAMPLE_I16) {
    int16_t samples[MAX_CHANNELS];
    for (uint32_t c = 0; c < channels; c++) {
      samples[c] = (int16_t) luaL_checknumber(L, 3 + c);
    }
    lovrSoundWrite(sound, frame, 1, samples, NULL);
  } else {
    float samples[MAX_CHANNELS];
    for (uint32_t c = 0; c < channels; c++) {
      samples[c] = luax_checkfloat(L, 3 + c);
    }
    lovrSoundWrite(sound, frame, 1, samples, NULL);
  }

  return 0;
}

static int l_lovrSoundGetFrames(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  size_t stride = lovrSoundGetStride(sound);
  SampleFormat format = lovrSoundGetFormat(sound);
  uint32_t channels = lovrSoundGetChannelCount(sound);
  uint32_t frameCount = lovrSoundGetFrameCount(sound);

  int index = lua_type(L, 2) == LUA_TNUMBER ? 2 : 3;
  uint32_t dstOffset = luax_optu32(L, index + 2, 0);
  uint32_t srcOffset = luax_optu32(L, index + 1, 0);
  uint32_t count = luax_optu32(L, index, frameCount - srcOffset);
  luax_check(L, srcOffset + count <= frameCount, "Tried to read samples past the end of the Sound");
  lua_settop(L, 2);

  switch (lua_type(L, 2)) {
    case LUA_TNIL:
    case LUA_TNONE:
    case LUA_TNUMBER:
      lua_pop(L, 1);
      lua_createtable(L, dstOffset + count * channels, 0);
    // fallthrough
    case LUA_TTABLE: {
      uint32_t frames = 0;
      while (frames < count) {
        char buffer[4096];
        uint32_t chunk = MIN((uint32_t) (sizeof(buffer) / stride), count - frames);
        uint32_t read = lovrSoundRead(sound, srcOffset + frames, chunk, buffer);
        uint32_t samples = read * channels;
        if (read == 0) break;

        if (format == SAMPLE_I16) { // Couldn't get compiler to hoist this branch
          short* shorts = (short*) buffer;
          for (uint32_t i = 0; i < samples; i++) {
            lua_pushnumber(L, *shorts++);
            lua_rawseti(L, 2, dstOffset + (frames * channels) + i + 1);
          }
        } else {
          float* floats = (float*) buffer;
          for (uint32_t i = 0; i < samples; i++) {
            lua_pushnumber(L, *floats++);
            lua_rawseti(L, 2, dstOffset + (frames * channels) + i + 1);
          }
        }

        frames += read;
      }
      lua_pushinteger(L, frames);
      return 2;
    }
    case LUA_TUSERDATA: {
      Sound* other = luax_totype(L, 2, Sound);
      Blob* blob = luax_totype(L, 2, Blob);
      if (blob) {
        luax_check(L, dstOffset + count * stride <= blob->size, "Blob is not big enough to hold all of the requested audio data");
        char* data = (char*) blob->data + dstOffset;
        uint32_t frames = 0;
        while (frames < count) {
          uint32_t read = lovrSoundRead(sound, srcOffset + frames, count - frames, data);
          data += read * stride;
          frames += read;
          if (read == 0) break;
        }
        lua_pushinteger(L, frames);
        return 1;
      } else if (other) {
        uint32_t frames;
        luax_assert(L, lovrSoundCopy(sound, other, count, srcOffset, dstOffset, &frames));
        lua_pushinteger(L, frames);
        return 1;
      }
    }
    // fallthrough
    default:
      return luax_typeerror(L, 2, "nil, number, table, Blob, or Sound");
  }
}

static int l_lovrSoundSetFrames(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  size_t stride = lovrSoundGetStride(sound);
  SampleFormat format = lovrSoundGetFormat(sound);
  uint32_t frameCount = lovrSoundGetFrameCount(sound);
  uint32_t channels = lovrSoundGetChannelCount(sound);

  if (lua_isuserdata(L, 2)) {
    Blob* blob = luax_totype(L, 2, Blob);

    if (blob) {
      uint32_t srcOffset = luax_optu32(L, 5, 0);
      uint32_t dstOffset = luax_optu32(L, 4, 0);
      uint32_t defaultCount = (uint32_t) MIN((blob->size - srcOffset) / stride, UINT32_MAX);
      uint32_t count = luax_optu32(L, 3, defaultCount);
      uint32_t frames;
      luax_assert(L, lovrSoundWrite(sound, dstOffset, count, (char*) blob->data + srcOffset, &frames));
      lua_pushinteger(L, frames);
      return 1;
    }

    Sound* other = luax_totype(L, 2, Sound);

    if (other) {
      uint32_t srcOffset = luax_optu32(L, 5, 0);
      uint32_t dstOffset = luax_optu32(L, 4, 0);
      uint32_t count = luax_optu32(L, 3, lovrSoundGetFrameCount(other) - srcOffset);
      uint32_t frames;
      luax_assert(L, lovrSoundCopy(other, sound, count, srcOffset, dstOffset,  &frames));
      lua_pushinteger(L, frames);
      return 1;
    }
  }

  if (!lua_istable(L, 2)) {
    return luax_typeerror(L, 2, "table, Blob, or Sound");
  }

  int length = luax_len(L, 2);
  uint32_t srcOffset = luax_optu32(L, 5, 1);
  uint32_t dstOffset = luax_optu32(L, 4, 0);
  uint32_t limit = MIN(frameCount - dstOffset, (length - srcOffset) / channels + 1);
  uint32_t count = luax_optu32(L, 3, limit);
  luax_check(L, count <= limit, "Tried to write too many frames (%d is over limit %d)", count, limit);

  uint32_t frames = 0;
  while (frames < count) {
    char buffer[4096];
    uint32_t chunk = MIN((uint32_t) (sizeof(buffer) / stride), count - frames);
    uint32_t samples = chunk * channels;

    if (format == SAMPLE_I16) {
      short* shorts = (short*) buffer;
      for (uint32_t i = 0; i < samples; i++) {
        lua_rawgeti(L, 2, srcOffset + (frames * channels) + i);
        *shorts++ = lua_tointeger(L, -1);
        lua_pop(L, 1);
      }
    } else if (format == SAMPLE_F32) {
      float* floats = (float*) buffer;
      for (uint32_t i = 0; i < samples; i++) {
        lua_rawgeti(L, 2, srcOffset + (frames * channels) + i);
        *floats++ = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
    }

    uint32_t written;
    luax_assert(L, lovrSoundWrite(sound, dstOffset + frames, chunk, buffer, &written));
    if (written == 0) break;
    frames += written;
  }
  lua_pushinteger(L, frames);
  return 1;
}

const luaL_Reg lovrSound[] = {
  { "getBlob", l_lovrSoundGetBlob },
  { "getFormat", l_lovrSoundGetFormat },
  { "getChannelLayout", l_lovrSoundGetChannelLayout },
  { "getChannelCount", l_lovrSoundGetChannelCount },
  { "getSampleRate", l_lovrSoundGetSampleRate },
  { "getByteStride", l_lovrSoundGetByteStride },
  { "getFrameCount", l_lovrSoundGetFrameCount },
  { "getSampleCount", l_lovrSoundGetSampleCount },
  { "getDuration", l_lovrSoundGetDuration },
  { "isCompressed", l_lovrSoundIsCompressed },
  { "getFrame", l_lovrSoundGetFrame },
  { "setFrame", l_lovrSoundSetFrame },
  { "getFrames", l_lovrSoundGetFrames },
  { "setFrames", l_lovrSoundSetFrames },
  { NULL, NULL }
};
