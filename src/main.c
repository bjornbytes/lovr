#include "lovr.h"
#include "glfw.h"

lua_State* L;

int main(int argc, char* argv[]) {
  L = luaL_newstate();
  luaL_openlibs(L);

  initGlfw(lovrOnError, lovrOnClose);
  lovrInit(L);
  lovrRun(L);

  return 0;
}
