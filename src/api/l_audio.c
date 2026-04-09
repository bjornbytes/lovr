#include "api.h"
#include "audio/audio.h"
#include "data/blob.h"
#include "data/sound.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>

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

StringEntry lovrReverbMode[] = {
  [REVERB_LISTENER] = ENTRY("listener"),
  [REVERB_SOURCE] = ENTRY("source"),
  { 0 }
};

StringEntry lovrReverbType[] = {
  [REVERB_CONVOLUTION] = ENTRY("convolution"),
  [REVERB_PARAMETRIC] = ENTRY("parametric"),
  { 0 }
};

static int l_lovrAudioGetSampleRate(lua_State *L) {
  lua_pushinteger(L, lovrAudioGetSampleRate());
  return 1;
}

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
  bool read = type == AUDIO_CAPTURE || lua_toboolean(L, 3);
  AudioStream* stream = luax_totype(L, 3, AudioStream);
  AudioShareMode shareMode = luax_checkenum(L, 4, AudioShareMode, "shared");
  return luax_pushsuccess(L, lovrAudioSetDevice(type, id, size, read, stream, shareMode));
}

static int l_lovrAudioGetStream(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  AudioStream* stream = lovrAudioGetStream(type);
  luax_pushtype(L, AudioStream, stream);
  return 1;
}

static int l_lovrAudioStart(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  return luax_pushsuccess(L, lovrAudioStart(type));
}

static int l_lovrAudioStop(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  return luax_pushsuccess(L, lovrAudioStop(type));
}

static int l_lovrAudioIsStarted(lua_State* L) {
  AudioType type = luax_checkenum(L, 1, AudioType, "playback");
  bool started = lovrAudioIsStarted(type);
  lua_pushboolean(L, started);
  return 1;
}

static int l_lovrAudioUpdate(lua_State* L) {
  lovrAudioUpdate(luax_checkfloat(L, 1));
  return 0;
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

static int l_lovrAudioSetHRTF(lua_State* L) {
  Blob* blob = lua_isnoneornil(L, 1) ? NULL : luax_readblob(L, 1, "HRTF");
  luax_assert(L, lovrAudioSetHRTF(blob));
  lovrRelease(blob, lovrBlobDestroy);
  return 0;
}

typedef struct {
  Blob* blob;
  bool spatial;
  bool pitchable;
  Source* source;
} SourceContext;

static bool luax_loadsource(void** userdata) {
  SourceContext* context = *userdata;
  Sound* sound = lovrSoundLoad(context->blob, true);
  lovrRelease(context->blob, lovrBlobDestroy);
  context->spatial |= lovrSoundGetChannelCount(sound) > 2;
  context->source = lovrSourceCreate(sound, context->pitchable, context->spatial);
  lovrRelease(sound, lovrSoundDestroy);
  return context->source;
}

static int luax_pushsource(lua_State* L, bool success, void* userdata) {
  SourceContext* context = userdata;
  if (success) {
    luax_pushtype(L, Source, context->source);
    lovrRelease(context->source, lovrSourceDestroy);
    lovrFree(context);
    return 1;
  } else {
    lovrFree(context);
    return 0;
  }
}

static int l_lovrAudioNewSource(lua_State* L) {
  bool decode = false;
  bool spatial = false;
  bool pitchable = true;

  if (lua_gettop(L) >= 2) {
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_getfield(L, 2, "decode");
    decode = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "spatial");
    if (!lua_isnil(L, -1)) spatial = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "pitchable");
    if (!lua_isnil(L, -1)) pitchable = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  AudioStream* stream = luax_totype(L, 1, AudioStream);
  Sound* sound = stream ? lovrAudioStreamGetSound(stream) : luax_totype(L, 1, Sound);

  if (lua_type(L, 1) == LUA_TSTRING || luax_totype(L, 1, Blob)) {
    Blob* blob = luax_readblob(L, 1, "Source");

    if (decode) {
      SourceContext* context = lovrCalloc(sizeof(SourceContext));
      context->blob = blob;
      context->spatial = spatial;
      context->pitchable = pitchable;
      return luax_yieldjob(L, luax_loadsource, luax_pushsource, context, 1);
    } else {
      sound = lovrSoundLoad(blob, decode);
      lovrRelease(blob, lovrBlobDestroy);
      luax_assert(L, sound);
    }
  } else if (sound) {
    lovrRetain(sound);
  } else {
    return luax_typeerror(L, 1, "string, Blob, Sound, or AudioStream");
  }

  Source* source = lovrSourceCreate(sound, pitchable, spatial);
  lovrRelease(sound, lovrSoundDestroy);
  luax_assert(L, source);
  luax_pushtype(L, Source, source);
  lovrRelease(source, lovrSourceDestroy);
  return 1;
}

static int l_lovrAudioNewAudioMesh(lua_State* L) {
  float* vertices;
  uint32_t* indices;
  uint32_t vertexCount, indexCount;
  int index = luax_readmesh(L, 1, &vertices, &vertexCount, &indices, &indexCount);

  AudioMaterial material;
  AudioMaterial* materials = NULL;
  if (lua_istable(L, index)) {
    materials = lovrMalloc(indexCount / 3 * sizeof(AudioMaterial));
    for (uint32_t i = 0; i < indexCount / 3; i++) {
      lua_rawgeti(L, index, i + 1);
      materials[i] = luax_checkenum(L, index, AudioMaterial, "generic");
      lua_pop(L, 1);
    }
  } else {
    material = luax_checkenum(L, index, AudioMaterial, "generic");
  }

  AudioMesh* mesh = lovrAudioMeshCreate(vertices, indices, vertexCount, indexCount, materials, material);
  lovrFree(vertices);
  lovrFree(indices);
  lovrFree(materials);
  luax_assert(L, mesh);
  luax_pushtype(L, AudioMesh, mesh);
  lovrRelease(mesh, lovrAudioMeshDestroy);
  return 1;
}

static const luaL_Reg lovrAudio[] = {
  { "getSampleRate", l_lovrAudioGetSampleRate },
  { "getDevices", l_lovrAudioGetDevices },
  { "getDevice", l_lovrAudioGetDevice },
  { "setDevice", l_lovrAudioSetDevice },
  { "getStream", l_lovrAudioGetStream },
  { "start", l_lovrAudioStart },
  { "stop", l_lovrAudioStop },
  { "isStarted", l_lovrAudioIsStarted },
  { "update", l_lovrAudioUpdate },
  { "getVolume", l_lovrAudioGetVolume },
  { "setVolume", l_lovrAudioSetVolume },
  { "getPosition", l_lovrAudioGetPosition },
  { "setPosition", l_lovrAudioSetPosition },
  { "getOrientation", l_lovrAudioGetOrientation },
  { "setOrientation", l_lovrAudioSetOrientation },
  { "getPose", l_lovrAudioGetPose },
  { "setPose", l_lovrAudioSetPose },
  { "setHRTF", l_lovrAudioSetHRTF },
  { "newSource", l_lovrAudioNewSource },
  { "newAudioMesh", l_lovrAudioNewAudioMesh },
  { NULL, NULL }
};

extern const luaL_Reg lovrSource[];
extern const luaL_Reg lovrAudioMesh[];

int luaopen_lovr_audio(lua_State* L) {
  AudioConfig config = {
    .debug = false,
    .autostart = true,
    .sampleRate = 48000,
    .reverb.type = REVERB_CONVOLUTION,
    .reverb.rays = 4096,
    .reverb.bounces = 4,
    .reverb.duration = 2.f,
    .reverb.rate = .1f
  };

  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "audio");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "debug");
      config.debug = lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "samplerate");
      config.sampleRate = lua_isnil(L, -1) ? config.sampleRate : luax_checku32(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "start");
      config.autostart = lua_isnil(L, -1) || lua_toboolean(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "reverb");
      if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "type");
        config.reverb.type = luax_checkenum(L, -1, ReverbType, NULL);
        lua_pop(L, 1);

        lua_getfield(L, -1, "rays");
        config.reverb.rays = luax_checku32(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "bounces");
        config.reverb.bounces = luax_checku32(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "duration");
        config.reverb.duration = luax_checkfloat(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "rate");
        config.reverb.rate = luax_checkfloat(L, -1);
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  luax_assert(L, lovrAudioInit(&config));
  luax_atexit(L, lovrAudioDestroy);

  lua_newtable(L);
  luax_register(L, lovrAudio);
  luax_registertype(L, Source);
  luax_registertype(L, AudioMesh);
  return 1;
}
