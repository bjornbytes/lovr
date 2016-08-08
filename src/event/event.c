#include "event.h"
#include "../lovr.h"
#include "../glfw.h"
#include "../osvr.h"

extern GLFWwindow* window;

int lovrEventPoll(lua_State* L) {
  glfwPollEvents();

  if (osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    osvrClientUpdate(ctx);
  }

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

int lovrInitEvent(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrEvent);
  return 1;
}
