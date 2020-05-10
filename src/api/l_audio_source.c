#include "api.h"
#include "audio/audio.h"

static int l_lovrSourcePlay(lua_State* L) {
  Source* source = luax_checktype(L, 1, Source);
  lovrSourcePlay(source);
  return 0;
}

const luaL_Reg lovrSource[] = {
  { "play", l_lovrSourcePlay },
  { NULL, NULL }
};
