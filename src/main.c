#include "lovr.h"

int main(int argc, char** argv) {
  bool reloading;
  do {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

    lovrInit(L, argc, argv);
    reloading = lovrRun(L);
  } while (reloading);

  return 0;
}
