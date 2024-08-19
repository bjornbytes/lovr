#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/sound.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>

StringEntry lovrEffect[] = {
  [EFFECT_ABSORPTION] = ENTRY("absorption"),
  [EFFECT_ATTENUATION] = ENTRY("attenuation"),
  [EFFECT_OCCLUSION] = ENTRY("occlusion"),
  [EFFECT_REVERB] = ENTRY("reverb"),
  [EFFECT_SPATIALIZATION] = ENTRY("spatialization"),
  [EFFECT_TRANSMISSION] = ENTRY("transmission"),
  { 0 }
};

StringEntry lovrAudioMaterial[] = {
  [MATERIAL_GENERIC] = ENTRY("generic"),
  [MATERIAL_BRICK] = ENTRY("brick"),
  [MATERIAL_CARPET] = ENTRY("carpet"),
  [MATERIAL_CERAMIC] = ENTRY("ceramic"),
  [MATERIAL_CONCRETE] = ENTRY("concrete"),
  [MATERIAL_GLASS] = ENTRY("glass"),
  [MATERIAL_GRAVEL] = ENTRY("gravel"),
  [MATERIAL_METAL] = ENTRY("metal"),
  [MATERIAL_PLASTER] = ENTRY("plaster"),
  [MATERIAL_ROCK] = ENTRY("rock"),
  [MATERIAL_WOOD] = ENTRY("wood"),
  { 0 }
};

StringEntry lovrAudioShareMode[] = {
  [AUDIO_SHARED] = ENTRY("shared"),
  [AUDIO_EXCLUSIVE] = ENTRY("exclusive"),
  { 0 }
};

StringEntry lovrAudioType[] = {
  [AUDIO_PLAYBACK] = ENTRY("playback"),
  [AUDIO_CAPTURE] = ENTRY("capture"),
  { 0 }
};

StringEntry lovrTimeUnit[] = {
  [UNIT_SECONDS] = ENTRY("seconds"),
  [UNIT_FRAMES] = ENTRY("frames"),
  { 0 }
};

StringEntry lovrVolumeUnit[] = {
  [UNIT_LINEAR] = ENTRY("linear"),
  [UNIT_DECIBELS] = ENTRY("db"),
  { 0 }
};

static void onDevice(AudioDevice* device, void* userdata) {
  lua_State* L = userdata;
  lua_createtable(L, 0, 3);
  void* p = lua_newuserdata(L, device->idSize);
  memcpy(p, device->id, device->idSize);
  lua_setfield(L, -2, "id");
  lua_pushstring(L, device->name);
  lua_setfield(L, -2, "name");
  lua_pushboolean(L, device->isDefault);
  lua_setfield(L, -2, "default");
  lua_rawseti(L, -2, luax_len(L, -2) + 1);
}

static int l_lovrAudioGetDevices(lua_State *L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  lua_newtable(L);
  lovrAudioEnumerateDevices(type, onDevice, L);
  return 1;
}

static int l_lovrAudioGetDevice(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  AudioDevice device;
  if (lovrAudioGetDevice(type, &device)) {
    lua_pushstring(L, device.name);
    void* p = lua_newuserdata(L, device.idSize);
    memcpy(p, device.id, device.idSize);
    return 2;
  }
  return 0;
}

static int l_lovrAudioSetDevice(lua_State *L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  void* id = lua_touserdata(L, 2);
  size_t size = id ? luax_len(L, 2) : 0;
  Sound* sink = lua_isnoneornil(L, 3) ? NULL : luax_checktype(L, 3, Sound);
  AudioShareMode shareMode = luax_checkenum(L, 4, AudioShareMode, "shared");
  return luax_pushsuccess(L, lovrAudioSetDevice(type, id, size, sink, shareMode));
}

static int l_lovrAudioStart(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool started = lovrAudioStart(type);
  lua_pushboolean(L, started);
  return 1;
}

static int l_lovrAudioStop(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool stopped = lovrAudioStop(type);
  lua_pushboolean(L, stopped);
  return 1;
}

static int l_lovrAudioIsStarted(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool started = lovrAudioIsStarted(type);
  lua_pushboolean(L, started);
  return 1;
}

static int l_lovrAudioGetVolume(lua_State* L) {
  VolumeUnit units = luax_checkenum(L, 1, VolumeUnit, "linear");
  lua_pushnumber(L, lovrAudioGetVolume(units));
  return 1;
}

static int l_lovrAudioSetVolume(lua_State* L) {
  float volume = luax_checkfloat(L, 1);
  VolumeUnit units = luax_checkenum(L, 2, VolumeUnit, "linear");
  lovrAudioSetVolume(volume, units);
  return 0;
}

static int l_lovrAudioGetPosition(lua_State* L) {
  float position[3], orientation[4];
  lovrAudioGetPose(position, orientation);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrAudioSetPosition(lua_State* L) {
  float position[3], orientation[4];
  lovrAudioGetPose(position, orientation);
  luax_readvec3(L, 1, position, NULL);
  lovrAudioSetPose(position, orientation);
  return 0;
}

static int l_lovrAudioGetOrientation(lua_State* L) {
  float position[3], orientation[4], angle, ax, ay, az;
  lovrAudioGetPose(position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrAudioSetOrientation(lua_State* L) {
  float position[3], orientation[4];
  lovrAudioGetPose(position, orientation);
  luax_readquat(L, 1, orientation, NULL);
  lovrAudioSetPose(position, orientation);
  return 0;
}

static int l_lovrAudioGetPose(lua_State *L) {
  float position[3], orientation[4], angle, ax, ay, az;
  lovrAudioGetPose(position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrAudioSetPose(lua_State *L) {
  int index = 1;
  float position[3], orientation[4];
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrAudioSetPose(position, orientation);
  return 0;
}

static int l_lovrAudioSetGeometry(lua_State* L) {
  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount, indexCount;
  bool shouldFree;
  int index = luax_readmesh(L, 1, &vertices, &vertexCount, &indices, &indexCount, &shouldFree);
  AudioMaterial material = luax_checkenum(L, index, AudioMaterial, "generic");
  bool success = lovrAudioSetGeometry(vertices, indices, vertexCount, indexCount, material);
  if (shouldFree) {
    lovrFree(vertices);
    lovrFree(indices);
  }
  lua_pushboolean(L, success);
  return 1;
}

static int l_lovrAudioGetSpatializer(lua_State *L) {
  lua_pushstring(L, lovrAudioGetSpatializer());
  return 1;
}

static int l_lovrAudioGetSampleRate(lua_State *L) {
  lua_pushinteger(L, lovrAudioGetSampleRate());
  return 1;
}

static int l_lovrAudioGetAbsorption(lua_State* L) {
  float absorption[3];
  lovrAudioGetAbsorption(absorption);
  lua_pushnumber(L, absorption[0]);
  lua_pushnumber(L, absorption[1]);
  lua_pushnumber(L, absorption[2]);
  return 3;
}

static int l_lovrAudioSetAbsorption(lua_State* L) {
  float absorption[3];
  absorption[0] = luax_checkfloat(L, 1);
  absorption[1] = luax_checkfloat(L, 2);
  absorption[2] = luax_checkfloat(L, 3);
  lovrAudioSetAbsorption(absorption);
  return 0;
}

static int l_lovrAudioNewSource(lua_State* L) {
  Sound* sound = luax_totype(L, 1, Sound);

  bool decode = false;
  bool pitchable = true;
  bool spatial = true;
  uint32_t effects = ~0u;
  if (lua_gettop(L) >= 2) {
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_getfield(L, 2, "decode");
    decode = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "pitchable");
    if (!lua_isnil(L, -1)) pitchable = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "effects");
    if (!lua_isnil(L, -1)) {
      effects = 0;
      luax_check(L, lua_istable(L, -1), "Source effects must be a table");
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
          Effect effect = luax_checkenum(L, -2, Effect, NULL);
          bool enabled = lua_toboolean(L, -1);
          effects |= enabled << effect;
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
          Effect effect = luax_checkenum(L, -1, Effect, NULL);
          effects |= 1 << effect;
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "spatial");
    if (!lua_isnil(L, -1)) spatial = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  if (!sound) {
    Blob* blob = luax_readblob(L, 1, "Source");
    sound = lovrSoundCreateFromFile(blob, decode);
    lovrRelease(blob, lovrBlobDestroy);
  } else {
    lovrRetain(sound);
  }

  Source* source = lovrSourceCreate(sound, pitchable, spatial, effects);
  lovrRelease(sound, lovrSoundDestroy);
  luax_assert(L, source);
  luax_pushtype(L, Source, source);
  lovrRelease(source, lovrSourceDestroy);
  return 1;
}

static const luaL_Reg lovrAudio[] = {
  { "getDevices", l_lovrAudioGetDevices },
  { "getDevice", l_lovrAudioGetDevice },
  { "setDevice", l_lovrAudioSetDevice },
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "isStarted", l_lovrAudioIsStarted },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "getPosition", l_lovrAudioGetPosition },
  { "setPosition", l_lovrAudioSetPosition },
  { "getOrientation", l_lovrAudioGetOrientation },
  { "setOrientation", l_lovrAudioSetOrientation },
  { "getPose", l_lovrAudioGetPose },
  { "setPose", l_lovrAudioSetPose },
  { "setGeometry", l_lovrAudioSetGeometry },
  { "getSpatializer", l_lovrAudioGetSpatializer },
  { "getSampleRate", l_lovrAudioGetSampleRate },
  { "getAbsorption", l_lovrAudioGetAbsorption },
  { "setAbsorption", l_lovrAudioSetAbsorption },
  { "newSource", l_lovrAudioNewSource },
  { NULL, NULL }
};

extern const luaL_Reg lovrSource[];

int luaopen_lovr_audio(lua_State* L) {
  bool start = true;
  const char *spatializer = NULL;
  uint32_t sampleRate = 48000; // Set default here
  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "audio");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "spatializer");
      spatializer = lua_tostring(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "samplerate");
      sampleRate = lua_isnil(L, -1) ? sampleRate : luax_checku32(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "start");
      start = lua_isnil(L, -1) || lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  luax_assert(L, lovrAudioInit(spatializer, sampleRate));
  luax_atexit(L, lovrAudioDestroy);

  if (start) {
    lovrAudioSetDevice(AUDIO_PLAYBACK, NULL, 0, NULL, AUDIO_SHARED);
    lovrAudioStart(AUDIO_PLAYBACK);
  }

  lua_newtable(L);
  luax_register(L, lovrAudio);
  luax_registertype(L, Source);
  return 1;
}
