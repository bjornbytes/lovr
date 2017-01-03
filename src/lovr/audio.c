#include "lovr/audio.h"
#include "audio/audio.h"

const luaL_Reg lovrAudio[] = {
  { NULL, NULL }
};

int l_lovrAudioInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrAudio);
  lovrAudioInit();
  return 1;
}
