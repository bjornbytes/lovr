#include "lovr.h"
#include "glfw.h"

lua_State* L;

int main(int argc, char** argv) {
  L = luaL_newstate();
  luaL_openlibs(L);

  initGlfw(lovrOnGlfwError, lovrOnClose);
  lovrInit(L);
  lovrRun(L, argc > 1 ? argv[1] : NULL);

  return 0;
}
