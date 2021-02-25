#include "api.h"
#include "audio/audio.h"
#include "core/maf.h"
#include "core/util.h"

static int l_lovrSourceClone(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  Source* clone = lovrSourceClone(source);
  luax_pushtype(L, Source, clone);
  lovrRelease(clone, lovrSourceDestroy);
  return 1;
}

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  if (lua_isboolean(L, -1)) {
    lovrSourceSetLooping(source, lua_toboolean(L, -1));
  }
  bool played = lovrSourcePlay(source);
  lua_pushboolean(L, played);
  return 1;
}

static int l_lovrSourcePause(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePause(source);
  return 0;
}

static int l_lovrSourceStop(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourceStop(source);
  return 0;
}

static int l_lovrSourceIsPlaying(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushboolean(L, lovrSourceIsPlaying(source));
  return 1;
}

static int l_lovrSourceIsLooping(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushboolean(L, lovrSourceIsLooping(source));
  return 1;
}

static int l_lovrSourceSetLooping(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourceSetLooping(source, lua_toboolean(L, 2));
  return 0;
}

static int l_lovrSourceGetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lua_pushnumber(L, lovrSourceGetVolume(source));
  return 1;
}

static int l_lovrSourceSetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float volume = luax_checkfloat(L, 2);
  lovrSourceSetVolume(source, volume);
  return 0;
}

static int l_lovrSourceGetDuration(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  double duration = lovrSourceGetDuration(source, units);
  lua_pushnumber(L, duration);
  return 1;
}

static int l_lovrSourceGetTime(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  double time = lovrSourceGetTime(source, units);
  lua_pushnumber(L, time);
  return 1;
}

static int l_lovrSourceSetTime(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  double seconds = luaL_checknumber(L, 2);
  TimeUnit units = luax_checkenum(L, 3, TimeUnit, "seconds");
  lovrSourceSetTime(source, seconds, units);
  return 0;
}

static int l_lovrSourceIsSpatial(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool spatial = lovrSourceIsSpatial(source);
  lua_pushboolean(L, spatial);
  return 1;
}

static int l_lovrSourceGetSpatialBlend(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float blend = lovrSourceGetSpatialBlend(source);
  lua_pushnumber(L, blend);
  return 1;
}

static int l_lovrSourceSetSpatialBlend(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float blend = luax_checkfloat(L, 2);
  lovrSourceSetSpatialBlend(source, blend);
  return 0;
}

static int l_lovrSourceGetInterpolation(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  SourceInterpolation interpolation = lovrSourceGetInterpolation(source);
  luax_pushenum(L, SourceInterpolation, interpolation);
  return 1;
}

static int l_lovrSourceSetInterpolation(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  SourceInterpolation interpolation = luax_checkenum(L, 2, SourceInterpolation, NULL);
  lovrSourceSetInterpolation(source, interpolation);
  return 0;
}

static int l_lovrSourceGetPose(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4], angle, ax, ay, az;
  lovrSourceGetPose(source, position, orientation);
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

static int l_lovrSourceSetPose(lua_State *L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4];
  int index = 2;
  index = luax_readvec3(L, index, position, NULL);
  index = luax_readquat(L, index, orientation, NULL);
  lovrSourceSetPose(source, position, orientation);
  return 0;
}

static int l_lovrSourceGetDirectivity(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float weight, power;
  lovrSourceGetDirectivity(source, &weight, &power);
  lua_pushnumber(L, weight);
  lua_pushnumber(L, power);
  return 2;
}

static int l_lovrSourceSetDirectivity(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float weight = luax_optfloat(L, 2, 0.f);
  float power = luax_optfloat(L, 3, 0.f);
  lovrSourceSetDirectivity(source, weight, power);
  return 0;
}

static int l_lovrSourceGetRadius(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float radius = lovrSourceGetRadius(source);
  lua_pushnumber(L, radius);
  return 1;
}

static int l_lovrSourceSetRadius(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float radius = luax_checkfloat(L, 2);
  lovrSourceSetRadius(source, radius);
  return 0;
}

static int l_lovrSourceIsAbsorptionEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lovrSourceIsAbsorptionEnabled(source);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrSourceSetAbsorptionEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lua_toboolean(L, 2);
  lovrSourceSetAbsorptionEnabled(source, enabled);
  return 0;
}

static int l_lovrSourceIsFalloffEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lovrSourceIsFalloffEnabled(source);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrSourceSetFalloffEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lua_toboolean(L, 2);
  lovrSourceSetFalloffEnabled(source, enabled);
  return 0;
}

static int l_lovrSourceIsOcclusionEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lovrSourceIsOcclusionEnabled(source);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrSourceSetOcclusionEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lua_toboolean(L, 2);
  lovrSourceSetOcclusionEnabled(source, enabled);
  return 0;
}

static int l_lovrSourceIsReverbEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lovrSourceIsReverbEnabled(source);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrSourceSetReverbEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lua_toboolean(L, 2);
  lovrSourceSetReverbEnabled(source, enabled);
  return 0;
}

static int l_lovrSourceIsTransmissionEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lovrSourceIsTransmissionEnabled(source);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrSourceSetTransmissionEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  bool enabled = lua_toboolean(L, 2);
  lovrSourceSetTransmissionEnabled(source, enabled);
  return 0;
}

const luaL_Reg lovrSource[] = {
  { "clone", l_lovrSourceClone },
  { "play", l_lovrSourcePlay },
  { "pause", l_lovrSourcePause },
  { "stop", l_lovrSourceStop },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isLooping", l_lovrSourceIsLooping },
  { "setLooping", l_lovrSourceSetLooping },
  { "getVolume", l_lovrSourceGetVolume },
  { "setVolume", l_lovrSourceSetVolume },
  { "getDuration", l_lovrSourceGetDuration },
  { "getTime", l_lovrSourceGetTime },
  { "setTime", l_lovrSourceSetTime },
  { "isSpatial", l_lovrSourceIsSpatial },
  { "getSpatialBlend", l_lovrSourceGetSpatialBlend },
  { "setSpatialBlend", l_lovrSourceSetSpatialBlend },
  { "getInterpolation", l_lovrSourceGetInterpolation },
  { "setInterpolation", l_lovrSourceSetInterpolation },
  { "getPose", l_lovrSourceGetPose },
  { "setPose", l_lovrSourceSetPose },
  { "getRadius", l_lovrSourceGetRadius },
  { "setRadius", l_lovrSourceSetRadius },
  { "getDirectivity", l_lovrSourceGetDirectivity },
  { "setDirectivity", l_lovrSourceSetDirectivity },
  { "isAbsorptionEnabled", l_lovrSourceIsAbsorptionEnabled },
  { "setAbsorptionEnabled", l_lovrSourceSetAbsorptionEnabled },
  { "isFalloffEnabled", l_lovrSourceIsFalloffEnabled },
  { "setFalloffEnabled", l_lovrSourceSetFalloffEnabled },
  { "isOcclusionEnabled", l_lovrSourceIsOcclusionEnabled },
  { "setOcclusionEnabled", l_lovrSourceSetOcclusionEnabled },
  { "isReverbEnabled", l_lovrSourceIsReverbEnabled },
  { "setReverbEnabled", l_lovrSourceSetReverbEnabled },
  { "isTransmissionEnabled", l_lovrSourceIsTransmissionEnabled },
  { "setTransmissionEnabled", l_lovrSourceSetTransmissionEnabled },
  { NULL, NULL }
};
