#include "event.h"
#include "lovr.h"
#include <GLFW/glfw3.h>
#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/ClientKit/InterfaceStateC.h>

extern GLFWwindow* window;
extern OSVR_ClientContext ctx;

int lovrEventPoll(lua_State* L) {
  glfwPollEvents();
  osvrClientUpdate(ctx);

  return 0;
}

int lovrEventQuit(lua_State* L) {
  lovrDestroy();

  return 0;
}

const luaL_Reg lovrEvent[] = {
  { "poll", lovrEventPoll },
  { "quit", lovrEventQuit },
  { NULL, NULL }
};
