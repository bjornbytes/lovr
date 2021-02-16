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
  lua_pushboolean(L, lovrSourceIsSpatial(source));
  return 1;
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

static int l_lovrSourceGetAbsorption(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float absorption[3];
  lovrSourceGetAbsorption(source, absorption);
  if (absorption[0] == 0.f && absorption[1] == 0.f && absorption[2] == 0.f) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushnumber(L, absorption[0]);
  lua_pushnumber(L, absorption[1]);
  lua_pushnumber(L, absorption[2]);
  return 3;
}

static int l_lovrSourceSetAbsorption(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float absorption[3] = { 0.f };
  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      break;
    case LUA_TBOOLEAN:
      if (lua_toboolean(L, 2)) {
        absorption[0] = .0002f;
        absorption[1] = .0017f;
        absorption[2] = .0182f;
      }
      break;
    default:
      absorption[0] = luax_checkfloat(L, 2);
      absorption[1] = luax_checkfloat(L, 3);
      absorption[2] = luax_checkfloat(L, 4);
      break;
  }
  lovrSourceSetAbsorption(source, absorption);
  return 0;
}

static int l_lovrSourceGetFalloff(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float falloff = lovrSourceGetFalloff(source);
  if (falloff <= 0.f) {
    lua_pushnil(L);
  } else {
    lua_pushnumber(L, falloff);
  }
  return 1;
}

static int l_lovrSourceSetFalloff(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  float falloff;
  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      falloff = 0.f;
      break;
    case LUA_TBOOLEAN:
      falloff = lua_toboolean(L, 2) ? 1.f : 0.f;
      break;
    default:
      falloff = luax_checkfloat(L, 2);
      break;
  }
  lovrSourceSetFalloff(source, falloff);
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
  { "getPose", l_lovrSourceGetPose },
  { "setPose", l_lovrSourceSetPose },
  { "getAbsorption", l_lovrSourceGetAbsorption },
  { "setAbsorption", l_lovrSourceSetAbsorption },
  { "getFalloff", l_lovrSourceGetFalloff },
  { "setFalloff", l_lovrSourceSetFalloff },
  { NULL, NULL }
};
