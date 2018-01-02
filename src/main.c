#include "lovr.h"

int main(int argc, char** argv) {
  do {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

    lovrInit(L, argc, argv);
    lovrRun(L);
  } while (lovrReloadPending);

  return 0;
}
