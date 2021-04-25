#include "api.h"
#include "data/sound.h"
#include "data/blob.h"
#include "core/util.h"
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
  uint32_t dstOffset = luaL_optinteger(L, index + 2, 0);
  uint32_t srcOffset = luaL_optinteger(L, index + 1, 0);
  uint32_t count = luaL_optinteger(L, index, frameCount - srcOffset);
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
        uint32_t chunk = MIN(sizeof(buffer) / stride, count - frames);
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
        lovrAssert(dstOffset + count * stride <= blob->size, "Tried to write samples past the end of the Blob");
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
  uint32_t frameCount = lovrSoundGetFrameCount(sound);
  uint32_t channels = lovrSoundGetChannelCount(sound);

  if (lua_isuserdata(L, 2)) {
    Blob* blob = luax_totype(L, 2, Blob);

    if (blob) {
      uint32_t srcOffset = luaL_optinteger(L, 5, 0);
      uint32_t dstOffset = luaL_optinteger(L, 4, 0);
      uint32_t count = luaL_optinteger(L, 3, (blob->size - srcOffset) / stride);
      uint32_t frames = lovrSoundWrite(sound, dstOffset, count, (char*) blob->data + srcOffset);
      lua_pushinteger(L, frames);
      return 1;
    }

    Sound* other = luax_totype(L, 2, Sound);

    if (other) {
      uint32_t srcOffset = luaL_optinteger(L, 5, 0);
      uint32_t dstOffset = luaL_optinteger(L, 4, 0);
      uint32_t count = luaL_optinteger(L, 3, lovrSoundGetFrameCount(other) - srcOffset);
      uint32_t frames = lovrSoundCopy(other, sound, count, srcOffset, dstOffset);
      lua_pushinteger(L, frames);
      return 1;
    }
  }

  if (!lua_istable(L, 2)) {
    return luax_typeerror(L, 2, "table, Blob, or Sound");
  }

  int length = luax_len(L, 2);
  uint32_t srcOffset = luaL_optinteger(L, 5, 1);
  uint32_t dstOffset = luaL_optinteger(L, 4, 0);
  uint32_t limit = MIN(frameCount - dstOffset, (length - srcOffset) / channels + 1);
  uint32_t count = luaL_optinteger(L, 3, limit);
  lovrAssert(count <= limit, "Tried to write too many frames");

  uint32_t frames = 0;
  while (frames < count) {
    char buffer[4096];
    uint32_t chunk = MIN(sizeof(buffer) / stride, count - frames);
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
  { "getFrameCount", l_lovrSoundGetFrameCount },
  { "getSampleCount", l_lovrSoundGetSampleCount },
  { "getDuration", l_lovrSoundGetDuration },
  { "isCompressed", l_lovrSoundIsCompressed },
  { "isStream", l_lovrSoundIsStream },
  { "getFrames", l_lovrSoundGetFrames },
  { "setFrames", l_lovrSoundSetFrames },
  { NULL, NULL }
};
