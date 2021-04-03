#include "api.h"
#include "audio/audio.h"
#include "core/maf.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrSourceClone(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  Source* clone = lovrSourceClone(source);
  luax_pushtype(L, Source, clone);
  lovrRelease(clone, lovrSourceDestroy);
  return 1;
}

static int l_lovrSourceGetSound(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  struct Sound* sound = lovrSourceGetSound(source);
  luax_pushtype(L, Sound, sound);
  return 1;
}

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
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
  VolumeUnit units = luax_checkenum(L, 2, VolumeUnit, "linear");
  lua_pushnumber(L, lovrSourceGetVolume(source, units));
  return 1;
}

static int l_lovrSourceSetVolume(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float volume = luax_checkfloat(L, 2);
  VolumeUnit units = luax_checkenum(L, 3, VolumeUnit, "linear");
  lovrSourceSetVolume(source, volume, units);
  return 0;
}

static int l_lovrSourceSeek(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  double seconds = luaL_checknumber(L, 2);
  TimeUnit units = luax_checkenum(L, 3, TimeUnit, "seconds");
  lovrSourceSeek(source, seconds, units);
  return 0;
}

static int l_lovrSourceTell(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  double time = lovrSourceTell(source, units);
  lua_pushnumber(L, time);
  return 1;
}

static int l_lovrSourceGetDuration(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  TimeUnit units = luax_checkenum(L, 2, TimeUnit, "seconds");
  double duration = lovrSourceGetDuration(source, units);
  lua_pushnumber(L, duration);
  return 1;
}

static int l_lovrSourceGetPosition(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4];
  lovrSourceGetPose(source, position, orientation);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  return 3;
}

static int l_lovrSourceSetPosition(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4];
  lovrSourceGetPose(source, position, orientation);
  luax_readvec3(L, 2, position, NULL);
  lovrSourceSetPose(source, position, orientation);
  return 0;
}

static int l_lovrSourceGetOrientation(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4], angle, ax, ay, az;
  lovrSourceGetPose(source, position, orientation);
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 4;
}

static int l_lovrSourceSetOrientation(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float position[4], orientation[4];
  lovrSourceGetPose(source, position, orientation);
  luax_readquat(L, 2, orientation, NULL);
  lovrSourceSetPose(source, position, orientation);
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
  float power = luax_optfloat(L, 3, 1.f);
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

static int l_lovrSourceIsEffectEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  Effect effect = luax_checkenum(L, 2, Effect, NULL);
  bool enabled = lovrSourceIsEffectEnabled(source, effect);
  lua_pushboolean(L, enabled);
  return 1;
}

static int l_lovrSourceSetEffectEnabled(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  Effect effect = luax_checkenum(L, 2, Effect, NULL);
  bool enabled = lua_isnoneornil(L, -1) ? true : lua_toboolean(L, -1);
  lovrSourceSetEffectEnabled(source, effect, enabled);
  return 0;
}

const luaL_Reg lovrSource[] = {
  { "clone", l_lovrSourceClone },
  { "getSound", l_lovrSourceGetSound },
  { "play", l_lovrSourcePlay },
  { "pause", l_lovrSourcePause },
  { "stop", l_lovrSourceStop },
  { "isPlaying", l_lovrSourceIsPlaying },
  { "isLooping", l_lovrSourceIsLooping },
  { "setLooping", l_lovrSourceSetLooping },
  { "getVolume", l_lovrSourceGetVolume },
  { "setVolume", l_lovrSourceSetVolume },
  { "seek", l_lovrSourceSeek },
  { "tell", l_lovrSourceTell },
  { "getDuration", l_lovrSourceGetDuration },
  { "getPosition", l_lovrSourceGetPosition },
  { "setPosition", l_lovrSourceSetPosition },
  { "getOrientation", l_lovrSourceGetOrientation },
  { "setOrientation", l_lovrSourceSetOrientation },
  { "getPose", l_lovrSourceGetPose },
  { "setPose", l_lovrSourceSetPose },
  { "getRadius", l_lovrSourceGetRadius },
  { "setRadius", l_lovrSourceSetRadius },
  { "getDirectivity", l_lovrSourceGetDirectivity },
  { "setDirectivity", l_lovrSourceSetDirectivity },
  { "isEffectEnabled", l_lovrSourceIsEffectEnabled },
  { "setEffectEnabled", l_lovrSourceSetEffectEnabled },
  { NULL, NULL }
};
