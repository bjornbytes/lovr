#include <luajit.h>
#include <lua.h>

#include "lovr.h"
#include "glfw.h"

lua_State* L;

int main(int argc, char* argv[]) {
  L = luaL_newstate();
  luaL_openlibs(L);

  lovrInit(L);
  initGlfw();

  lovrRun(L);

  return 0;
}
