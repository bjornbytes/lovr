#include "lovr.h"
#include "glfw.h"

int main(int argc, char** argv) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  initGlfw(lovrOnGlfwError, lovrOnClose, L);
  lovrInit(L);
  lovrRun(L, argc > 1 ? argv[1] : NULL);

  return 0;
}
