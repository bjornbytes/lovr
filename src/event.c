#include "event.h"
#include "lovr.h"
#include <GLFW/glfw3.h>

extern GLFWwindow* window;

int lovrEventPoll(lua_State* L) {
  glfwPollEvents();

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
