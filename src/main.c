#include <stdlib.h>
#include <luajit.h>
#include <lua.h>

#include "lovr.h"
#include "glfw.h"
#include "osvr.h"

lua_State* L;

int main(int argc, char* argv[]) {
  L = luaL_newstate();
  luaL_openlibs(L);

  initGlfw(lovrOnError, lovrOnClose);
  lovrInit(L);
  lovrRun(L);

  exit(0);
}
