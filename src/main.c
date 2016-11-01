#include "lovr.h"
#include "glfw.h"

int main(int argc, char** argv) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  initGlfw(lovrOnGlfwError, lovrOnClose, L);
  lovrInit(L, argc, argv);
  lovrRun(L);

  return 0;
}
