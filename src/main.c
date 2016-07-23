#include <stdlib.h>
#include <luajit.h>
#include <lua.h>

#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/ClientKit/InterfaceStateC.h>

#include "lovr.h"
#include "glfw.h"

lua_State* L;
OSVR_ClientContext ctx;

int main(int argc, char* argv[]) {
  L = luaL_newstate();
  luaL_openlibs(L);

  ctx = osvrClientInit("es.bjornbyt", 0);

  initGlfw(lovrOnError, lovrOnClose);
  lovrInit(L);

  lovrRun(L);

  exit(0);
}
