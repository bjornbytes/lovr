#include "api.h"
#include "thread/thread.h"

int l_lovrThreadStart(lua_State* L) {
  Thread* thread = luax_checktype(L, 1, Thread);
  lovrThreadStart(thread);
  return 0;
}

const luaL_Reg lovrThread[] = {
  { "start", l_lovrThreadStart },
  { NULL, NULL }
};
