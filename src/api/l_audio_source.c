#include "api.h"
#include "audio/audio.h"
#include "core/maf.h"
#include <stdbool.h>

static int l_lovrSourceGetBitDepth(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetBitDepth(source));
  return 1;
}

static int l_lovrSourceGetChannelCount(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetChannelCount(source));
  return 1;
}

static int l_lovrSourceGetCone(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float innerAngle, outerAngle, outerGain;
  lovrSourceGetCone(source, &innerAngle, &outerAngle, &outerGain);
  lua_pushnumber(L, innerAngle);
  lua_pushnumber(L, outerAngle);
  lua_pushnumber(L, outerGain);
  return 3;
}

static int l_lovrSourceGetDuration(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luax_checkenum(L, 2, TimeUnit, "seconds");
  size_t duration = lovrSourceGetDuration(source);

  if (unit == UNIT_SECONDS) {
    lua_pushnumber(L, (float) duration / lovrSourceGetSampleRate(source));
  } else {
    lua_pushinteger(L, duration);
  }

  return 1;
}

static int l_lovrSourceGetFalloff(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float reference, max, rolloff;
  lovrSourceGetFalloff(source, &reference, &max, &rolloff);
  lua_pushnumber(L, reference);
  lua_pushnumber(L, max);
  lua_pushnumber(L, rolloff);
  return 3;
}

static int l_lovrSourceGetOrientation(lua_State* L) {
  float orientation[4], angle, ax, ay, az;
  Source* source = luax_checktype(L, 1, Source);
  lovrSourceGetOrientation(source, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrSourceGetPitch(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetPitch(source));
  return 1;
}

static int l_lovrSourceGetPose(lua_State* L) {
  float position[4], orientation[4], angle, ax, ay, az;
  Source* source = luax_checktype(L, 1, Source);
  lovrSourceGetPosition(source, position);
  lovrSourceGetOrientation(source, orientation);
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

static int l_lovrSourceGetPosition(lua_State* L) {
  float position[4];
  lovrSourceGetPosition(luax_checktype(L, 1, Source), position);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrSourceGetSampleRate(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushinteger(L, lovrSourceGetSampleRate(source));
  return 1;
}

static int l_lovrSourceGetType(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  luax_pushenum(L, SourceType, lovrSourceGetType(source));
  return 1;
}

static int l_lovrSourceGetVelocity(lua_State* L) {
  float velocity[4];
  lovrSourceGetVelocity(luax_checktype(L, 1, Source), velocity);
  lua_pushnumber(L, velocity[0]);
  lua_pushnumber(L, velocity[1]);
  lua_pushnumber(L, velocity[2]);
  return 3;
}

static int l_lovrSourceGetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetVolume(source));
  return 1;
}

static int l_lovrSourceGetVolumeLimits(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float min, max;
  lovrSourceGetVolumeLimits(source, &min, &max);
  lua_pushnumber(L, min);
  lua_pushnumber(L, max);
  return 2;
}

static int l_lovrSourceIsLooping(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsLooping(luax_checktype(L, 1, Source)));
  return 1;
}

static int l_lovrSourceIsPlaying(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsPlaying(luax_checktype(L, 1, Source)));
  return 1;
}

static int l_lovrSourceIsRelative(lua_State* L) {
  lua_pushboolean(L, lovrSourceIsRelative(luax_checktype(L, 1, Source)));
  return 1;
}

static int l_lovrSourcePause(lua_State* L) {
  lovrSourcePause(luax_checktype(L, 1, Source));
  return 0;
}

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePlay(source);
  lovrAudioAdd(source);
  return 0;
}

static int l_lovrSourceSeek(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luax_checkenum(L, 3, TimeUnit, "seconds");

  if (unit == UNIT_SECONDS) {
    float seconds = luax_checkfloat(L, 2);
    int sampleRate = lovrSourceGetSampleRate(source);
    lovrSourceSeek(source, (int) (seconds * sampleRate + .5f));
  } else {
    lovrSourceSeek(source, luaL_checkinteger(L, 2));
  }

  return 0;
}

static int l_lovrSourceSetCone(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float innerAngle = luax_checkfloat(L, 2);
  float outerAngle = luax_checkfloat(L, 3);
  float outerGain = luax_checkfloat(L, 4);
  lovrSourceSetCone(source, innerAngle, outerAngle, outerGain);
  return 0;
}

static int l_lovrSourceSetFalloff(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float reference = luax_checkfloat(L, 2);
  float max = luax_checkfloat(L, 3);
  float rolloff = luax_checkfloat(L, 4);
  lovrSourceSetFalloff(source, reference, max, rolloff);
  return 0;
}

static int l_lovrSourceSetLooping(lua_State* L) {
  lovrSourceSetLooping(luax_checktype(L, 1, Source), lua_toboolean(L, 2));
  return 0;
}

static int l_lovrSourceSetOrientation(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float orientation[4];
  luax_readquat(L, 2, orientation, NULL);
  lovrSourceSetOrientation(source, orientation);
  return 0;
}

static int l_lovrSourceSetPitch(lua_State* L) {
  lovrSourceSetPitch(luax_checktype(L, 1, Source), luax_checkfloat(L, 2));
  return 0;
}

static int l_lovrSourceSetPose(lua_State* L) {
  float position[4], orientation[4];
  int index = 2;
  Source* source = luax_checktype(L, 1, Source);
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrSourceSetPosition(source, position);
  lovrSourceSetOrientation(source, orientation);
  return 0;
}

static int l_lovrSourceSetPosition(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4];
  luax_readvec3(L, 2, position, NULL);
  lovrSourceSetPosition(source, position);
  return 0;
}

static int l_lovrSourceSetRelative(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool isRelative = lua_toboolean(L, 2);
  lovrSourceSetRelative(source, isRelative);
  return 0;
}

static int l_lovrSourceSetVelocity(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float velocity[4];
  luax_readvec3(L, 2, velocity, NULL);
  lovrSourceSetVelocity(source, velocity);
  return 0;
}

static int l_lovrSourceSetVolume(lua_State* L) {
  lovrSourceSetVolume(luax_checktype(L, 1, Source), luax_checkfloat(L, 2));
  return 0;
}

static int l_lovrSourceSetVolumeLimits(lua_State* L) {
  lovrSourceSetVolumeLimits(luax_checktype(L, 1, Source), luax_checkfloat(L, 2), luax_checkfloat(L, 3));
  return 0;
}

static int l_lovrSourceStop(lua_State* L) {
  lovrSourceStop(luax_checktype(L, 1, Source));
  return 0;
}

static int l_lovrSourceTell(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit unit = luax_checkenum(L, 2, TimeUnit, "seconds");
  size_t offset = lovrSourceTell(source);

  if (unit == UNIT_SECONDS) {
    lua_pushnumber(L, (float) offset / lovrSourceGetSampleRate(source));
  } else {
    lua_pushinteger(L, offset);
  }

  return 1;
}

const luaL_Reg lovrSource[] = {
  { "getBitDepth", l_lovrSourceGetBitDepth },
  { "getChannelCount", l_lovrSourceGetChannelCount },
  { "getCone", l_lovrSourceGetCone },
  { "getDuration", l_lovrSourceGetDuration },
  { "getFalloff", l_lovrSourceGetFalloff },
  { "getOrientation", l_lovrSourceGetOrientation },
  { "getPitch", l_lovrSourceGetPitch },
  { "getPose", l_lovrSourceGetPose },
  { "getPosition", l_lovrSourceGetPosition },
  { "getSampleRate", l_lovrSourceGetSampleRate },
  { "getType", l_lovrSourceGetType },
  { "getVelocity", l_lovrSourceGetVelocity },
  { "getVolume", l_lovrSourceGetVolume },
  { "getVolumeLimits", l_lovrSourceGetVolumeLimits },
  { "isLooping", l_lovrSourceIsLooping },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isRelative", l_lovrSourceIsRelative },
  { "pause", l_lovrSourcePause },
  { "play", l_lovrSourcePlay },
  { "seek", l_lovrSourceSeek },
  { "setCone", l_lovrSourceSetCone },
  { "setFalloff", l_lovrSourceSetFalloff },
  { "setLooping", l_lovrSourceSetLooping },
  { "setOrientation", l_lovrSourceSetOrientation },
  { "setPitch", l_lovrSourceSetPitch },
  { "setPose", l_lovrSourceSetPose },
  { "setPosition", l_lovrSourceSetPosition },
  { "setRelative", l_lovrSourceSetRelative },
  { "setVelocity", l_lovrSourceSetVelocity },
  { "setVolume", l_lovrSourceSetVolume },
  { "setVolumeLimits", l_lovrSourceSetVolumeLimits },
  { "stop", l_lovrSourceStop },
  { "tell", l_lovrSourceTell },
  { NULL, NULL }
};
