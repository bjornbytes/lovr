#include "lovr/audio.h"
#include "lovr/types/source.h"
#include "audio/audio.h"
#include "audio/source.h"
#include "loaders/source.h"
#include "filesystem/filesystem.h"

const luaL_Reg lovrAudio[] = {
  { "update", l_lovrAudioUpdate },
  { "newSource", l_lovrAudioNewSource },
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
