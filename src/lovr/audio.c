#include "lovr/audio.h"
#include "lovr/types/source.h"
#include "audio/audio.h"
#include "audio/source.h"
#include "loaders/source.h"
#include "filesystem/filesystem.h"

const luaL_Reg lovrAudio[] = {
  { "update", l_lovrAudioUpdate },
  { "getOrientation", l_lovrAudioGetOrientation },
  { "getPosition", l_lovrAudioGetPosition },
  { "getVolume", l_lovrAudioGetVolume },
  { "newSource", l_lovrAudioNewSource },
  { "pause", l_lovrAudioPause },
  { "resume", l_lovrAudioResume },
  { "rewind", l_lovrAudioRewind },
  { "setOrientation", l_lovrAudioSetOrientation },
  { "setPosition", l_lovrAudioSetPosition },
  { "setVolume", l_lovrAudioSetVolume },
  { "stop", l_lovrAudioStop },
  { NULL, NULL }
};

int l_lovrAudioInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrAudio);
  luax_registertype(L, "Source", lovrSource);

  map_init(&TimeUnits);
  map_set(&TimeUnits, "seconds", UNIT_SECONDS);
  map_set(&TimeUnits, "samples", UNIT_SAMPLES);

  lovrAudioInit();
  return 1;
}

int l_lovrAudioUpdate(lua_State* L) {
  lovrAudioUpdate();
  return 0;
}

int l_lovrAudioGetOrientation(lua_State* L) {
  float fx, fy, fz, ux, uy, uz;
  lovrAudioGetOrientation(&fx, &fy, &fz, &ux, &uy, &uz);
  lua_pushnumber(L, fx);
  lua_pushnumber(L, fy);
  lua_pushnumber(L, fz);
  lua_pushnumber(L, ux);
  lua_pushnumber(L, uy);
  lua_pushnumber(L, uz);
  return 6;
}

int l_lovrAudioGetPosition(lua_State* L) {
  float x, y, z;
  lovrAudioGetPosition(&x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrAudioGetVolume(lua_State* L) {
  lua_pushnumber(L, lovrAudioGetVolume());
  return 1;
}

int l_lovrAudioNewSource(lua_State* L) {
  const char* filename = luaL_checkstring(L, 1);
  if (!strstr(filename, ".ogg")) {
    return luaL_error(L, "Only .ogg files are supported");
  }

  int size;
  void* data = lovrFilesystemRead(filename, &size);
  if (!data) {
    return luaL_error(L, "Could not load source from file '%s'", filename);
  }

  SourceData* sourceData = lovrSourceDataFromFile(data, size);
  luax_pushtype(L, Source, lovrSourceCreate(sourceData));
  return 1;
}

int l_lovrAudioPause(lua_State* L) {
  lovrAudioPause();
  return 0;
}

int l_lovrAudioResume(lua_State* L) {
  lovrAudioResume();
  return 0;
}

int l_lovrAudioRewind(lua_State* L) {
  lovrAudioRewind();
  return 0;
}

int l_lovrAudioSetOrientation(lua_State* L) {
  float fx = luaL_checknumber(L, 1);
  float fy = luaL_checknumber(L, 2);
  float fz = luaL_checknumber(L, 3);
  float ux = luaL_checknumber(L, 4);
  float uy = luaL_checknumber(L, 5);
  float uz = luaL_checknumber(L, 6);
  lovrAudioSetOrientation(fx, fy, fz, ux, uy, uz);
  return 0;
}

int l_lovrAudioSetPosition(lua_State* L) {
  float x = luaL_checknumber(L, 1);
  float y = luaL_checknumber(L, 2);
  float z = luaL_checknumber(L, 3);
  lovrAudioSetPosition(x, y, z);
  return 0;
}

int l_lovrAudioSetVolume(lua_State* L) {
  float volume = luaL_checknumber(L, 1);
  lovrAudioSetVolume(volume);
  return 0;
}

int l_lovrAudioStop(lua_State* L) {
  lovrAudioStop();
  return 0;
}
