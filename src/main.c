#include "lovr.h"

int main(int argc, char** argv) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  lovrInit(L, argc, argv);
  lovrRun(L);

  return 0;
}
