#include "api.h"
#include "data/sound.h"
#include "data/blob.h"
#include "core/util.h"

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
  uint32_t count = frameCount;
  uint32_t offset = 0;
  uint32_t dstOffset = 0;

  int index = 2;
  if (lua_type(L, 2) == LUA_TNUMBER) {
    count = lua_tointeger(L, 2);
    index = 3;
    if (lua_type(L, 3) == LUA_TNUMBER) {
      offset = lua_tointeger(L, 3);
      index = 4;
    }
  }

  lovrAssert(offset + count <= frameCount, "Tried to read samples past the end of the Sound");

  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      lua_settop(L, index - 1);
      lua_createtable(L, count * lovrSoundGetChannelCount(sound), 0);
    // fallthrough
    case LUA_TTABLE:
      dstOffset = luaL_optinteger(L, index + 1, 1);
      lua_settop(L, index);
      uint32_t frames = 0;
      while (frames < count) {
        char buffer[4096];
        uint32_t chunk = MIN(sizeof(buffer) / stride, count - frames);
        uint32_t read = lovrSoundRead(sound, offset + frames, chunk, buffer);
        uint32_t samples = read * channels;
        if (read == 0) break;

        if (format == SAMPLE_I16) { // Couldn't get compiler to hoist this branch
          short* shorts = (short*) buffer;
          for (uint32_t i = 0; i < samples; i++) {
            lua_pushnumber(L, *shorts++);
            lua_rawseti(L, index, dstOffset + frames + i);
          }
        } else {
          float* floats = (float*) buffer;
          for (uint32_t i = 0; i < samples; i++) {
            lua_pushnumber(L, *floats++);
            lua_rawseti(L, index, dstOffset + frames + i);
          }
        }

        frames += read;
      }
      lua_pushinteger(L, frames);
      return 2;
    case LUA_TUSERDATA:
      dstOffset = luaL_optinteger(L, index + 1, 0);
      lua_settop(L, index);
      Sound* other = luax_totype(L, index, Sound);
      Blob* blob = luax_totype(L, index, Blob);
      if (blob) {
        lovrAssert(dstOffset + count * stride <= blob->size, "Tried to write samples past the end of the Blob");
        char* data = (char*) blob->data + dstOffset;
        uint32_t frames = 0;
        while (frames < count) {
          uint32_t read = lovrSoundRead(sound, offset + frames, count - frames, data);
          data += read * stride;
          if (read == 0) break;
        }
        lua_pushinteger(L, frames);
        return 2;
      } else if (other) {
        uint32_t frames = lovrSoundCopy(sound, other, count, offset, dstOffset);
        lua_pushinteger(L, frames);
        return 2;
      }
    // fallthrough
    default:
      return luax_typeerror(L, index, "nil, table, Blob, or Sound");
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
  uint32_t limit = MIN(frameCount - dstOffset, length - srcOffset + 1);
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
        lua_rawgeti(L, 2, dstOffset + frames + i);
        *shorts++ = lua_tointeger(L, -1);
        lua_pop(L, 1);
      }
    } else if (format == SAMPLE_F32) {
      float* floats = (float*) buffer;
      for (uint32_t i = 0; i < samples; i++) {
        lua_rawgeti(L, 2, dstOffset + frames + i);
        *floats++ = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
    }

    uint32_t written = lovrSoundWrite(sound, srcOffset + frames, chunk, buffer);
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
