#include "api.h"
#include "audio/audio.h"

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePlay(source);
  return 0;
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

const luaL_Reg lovrSource[] = {
  { "play", l_lovrSourcePlay },
  { "isLooping", l_lovrSourceIsLooping },
  { "setLooping", l_lovrSourceSetLooping },
  { NULL, NULL }
};
