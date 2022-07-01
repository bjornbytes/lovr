#include "api.h"
#include "data/sound.h"
#include "data/blob.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>

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
  luax_pushenum(L, ChannelLayout, lovrSoundGetChannelLayout(sound));
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
  lovrCheck(stride < UINT32_MAX, "Sound contains impossibly many channels");
  lua_pushinteger(L, (uint32_t)stride);
  return 1;
}

static int l_lovrSoundGetFrameCount(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frames = lovrSoundGetFrameCount(sound);
  lua_pushinteger(L, frames);
  return 1;
}

static int l_lovrSoundGetCapacity(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  uint32_t frames = lovrSoundGetCapacity(sound);
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

static int l_lovrSoundIsStream(lua_State* L) {
  Sound* sound = luax_checktype(L, 1, Sound);
  bool stream = lovrSoundIsStream(sound);
  lua_pushboolean(L, stream);
  return 1;
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
  lovrAssert(srcOffset + count <= frameCount, "Tried to read samples past the end of the Sound");
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
        uint32_t chunk = MIN((uint32_t)(sizeof(buffer) / stride), count - frames);
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
        lovrAssert(dstOffset + count * stride <= blob->size, "This Blob can hold %d bytes, which is not enough space to hold %d bytes of audio data at the requested offset (%d)", blob->size, count * stride, dstOffset);
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
        uint32_t frames = lovrSoundCopy(sound, other, count, srcOffset, dstOffset);
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
  uint32_t frameCount = lovrSoundGetCapacity(sound);
  uint32_t channels = lovrSoundGetChannelCount(sound);

  if (lua_isuserdata(L, 2)) {
    Blob* blob = luax_totype(L, 2, Blob);

    if (blob) {
      uint32_t srcOffset = luax_optu32(L, 5, 0);
      uint32_t dstOffset = luax_optu32(L, 4, 0);
      size_t defaultCount = (blob->size - srcOffset) / stride;
      lovrCheck(defaultCount <= UINT32_MAX, "Sound is too big to work with (somewhere over 4 GiB)");
      uint32_t count = luax_optu32(L, 3, (uint32_t)defaultCount);
      uint32_t frames = lovrSoundWrite(sound, dstOffset, count, (char*) blob->data + srcOffset);
      lua_pushinteger(L, frames);
      return 1;
    }

    Sound* other = luax_totype(L, 2, Sound);

    if (other) {
      uint32_t srcOffset = luax_optu32(L, 5, 0);
      uint32_t dstOffset = luax_optu32(L, 4, 0);
      uint32_t count = luax_optu32(L, 3, lovrSoundGetCapacity(other) - srcOffset);
      uint32_t frames = lovrSoundCopy(other, sound, count, srcOffset, dstOffset);
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
  lovrAssert(count <= limit, "Tried to write too many frames (%d is over limit %d)", count, limit);

  uint32_t frames = 0;
  while (frames < count) {
    char buffer[4096];
    uint32_t chunk = MIN((uint32_t)(sizeof(buffer) / stride), count - frames);
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

    uint32_t written = lovrSoundWrite(sound, dstOffset + frames, chunk, buffer);
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
  { "getCapacity", l_lovrSoundGetCapacity },
  { "getSampleCount", l_lovrSoundGetSampleCount },
  { "getDuration", l_lovrSoundGetDuration },
  { "isCompressed", l_lovrSoundIsCompressed },
  { "isStream", l_lovrSoundIsStream },
  { "getFrames", l_lovrSoundGetFrames },
  { "setFrames", l_lovrSoundSetFrames },
  { NULL, NULL }
};
