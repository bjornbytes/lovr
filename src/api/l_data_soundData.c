#include "api.h"
#include "data/soundData.h"
#include "data/blob.h"
#include "core/util.h"

StringEntry lovrSampleFormat[] = {
  [SAMPLE_F32] = ENTRY("f32"),
  [SAMPLE_I16] = ENTRY("i16"),
  { 0 }
};

static int l_lovrSoundDataGetBlob(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  Blob* blob = lovrSoundDataGetBlob(soundData);
  luax_pushtype(L, Blob, blob);
  return 1;
}

static int l_lovrSoundDataGetFormat(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  luax_pushenum(L, SampleFormat, lovrSoundDataGetFormat(soundData));
  return 1;
}

static int l_lovrSoundDataGetChannelCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, lovrSoundDataGetChannelCount(soundData));
  return 1;
}

static int l_lovrSoundDataGetSampleRate(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  lua_pushinteger(L, lovrSoundDataGetSampleRate(soundData));
  return 1;
}

static int l_lovrSoundDataGetFrameCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  uint32_t frames = lovrSoundDataGetFrameCount(soundData);
  lua_pushinteger(L, frames);
  return 1;
}

static int l_lovrSoundDataGetSampleCount(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  uint32_t frames = lovrSoundDataGetFrameCount(soundData);
  uint32_t channels = lovrSoundDataGetChannelCount(soundData);
  lua_pushinteger(L, frames * channels);
  return 1;
}

static int l_lovrSoundDataGetDuration(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  uint32_t frames = lovrSoundDataGetFrameCount(soundData);
  uint32_t rate = lovrSoundDataGetSampleRate(soundData);
  lua_pushnumber(L, (double) frames / rate);
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

static int l_lovrSoundDataGetFrames(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  size_t stride = lovrSoundDataGetStride(soundData);
  SampleFormat format = lovrSoundDataGetFormat(soundData);
  uint32_t channels = lovrSoundDataGetChannelCount(soundData);
  uint32_t frameCount = lovrSoundDataGetFrameCount(soundData);
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

  lovrAssert(offset + count <= frameCount, "Tried to read samples past the end of the SoundData");

  switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
      lua_settop(L, index - 1);
      lua_createtable(L, count * lovrSoundDataGetChannelCount(soundData), 0);
    // fallthrough;
    case LUA_TTABLE:
      dstOffset = luaL_optinteger(L, index + 1, 1);
      lua_settop(L, index);
      uint32_t frames = 0;
      while (frames < count) {
        char buffer[4096];
        uint32_t chunk = MIN(sizeof(buffer) / stride, count - frames);
        uint32_t read = lovrSoundDataRead(soundData, offset + frames, chunk, buffer);
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
      SoundData* other = luax_totype(L, index, SoundData);
      Blob* blob = luax_totype(L, index, Blob);
      if (blob) {
        lovrAssert(dstOffset + count * stride <= blob->size, "Tried to write samples past the end of the Blob");
        char* data = (char*) blob->data + dstOffset;
        uint32_t frames = 0;
        while (frames < count) {
          uint32_t read = lovrSoundDataRead(soundData, offset + frames, count - frames, data);
          data += read * stride;
          if (read == 0) break;
        }
        lua_pushinteger(L, frames);
        return 2;
      } else if (other) {
        uint32_t frames = lovrSoundDataCopy(soundData, other, count, offset, dstOffset);
        lua_pushinteger(L, frames);
        return 2;
      }
    // fallthrough;
    default:
      return luax_typeerror(L, index, "nil, table, Blob, or SoundData");
  }
}

static int l_lovrSoundDataSetFrames(lua_State* L) {
  SoundData* soundData = luax_checktype(L, 1, SoundData);
  size_t stride = lovrSoundDataGetStride(soundData);
  SampleFormat format = lovrSoundDataGetFormat(soundData);
  uint32_t frameCount = lovrSoundDataGetFrameCount(soundData);
  uint32_t channels = lovrSoundDataGetChannelCount(soundData);

  if (lua_isuserdata(L, 2)) {
    Blob* blob = luax_totype(L, 2, Blob);

    if (blob) {
      uint32_t srcOffset = luaL_optinteger(L, 5, 0);
      uint32_t dstOffset = luaL_optinteger(L, 4, 0);
      uint32_t count = luaL_optinteger(L, 3, (blob->size - srcOffset) / stride);
      uint32_t frames = lovrSoundDataWrite(soundData, dstOffset, count, (char*) blob->data + srcOffset);
      lua_pushinteger(L, frames);
      return 1;
    }

    SoundData* other = luax_totype(L, 2, SoundData);

    if (other) {
      uint32_t srcOffset = luaL_optinteger(L, 5, 0);
      uint32_t dstOffset = luaL_optinteger(L, 4, 0);
      uint32_t count = luaL_optinteger(L, 3, lovrSoundDataGetFrameCount(other) - srcOffset);
      uint32_t frames = lovrSoundDataCopy(other, soundData, count, srcOffset, dstOffset);
      lua_pushinteger(L, frames);
      return 1;
    }
  }

  if (!lua_istable(L, 2)) {
    return luax_typeerror(L, 2, "table, Blob, or SoundData");
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

    uint32_t written = lovrSoundDataWrite(soundData, srcOffset + frames, chunk, buffer);
    if (written == 0) break;
    frames += written;
  }
  lua_pushinteger(L, frames);
  return 1;
}

const luaL_Reg lovrSoundData[] = {
  { "getBlob", l_lovrSoundDataGetBlob },
  { "getFormat", l_lovrSoundDataGetFormat },
  { "getChannelCount", l_lovrSoundDataGetChannelCount },
  { "getSampleRate", l_lovrSoundDataGetSampleRate },
  { "getFrameCount", l_lovrSoundDataGetFrameCount },
  { "getSampleCount", l_lovrSoundDataGetSampleCount },
  { "getDuration", l_lovrSoundDataGetDuration },
  { "isCompressed", l_lovrSoundDataIsCompressed },
  { "isStream", l_lovrSoundDataIsStream },
  { "getFrames", l_lovrSoundDataGetFrames },
  { "setFrames", l_lovrSoundDataSetFrames },
  { NULL, NULL }
};
