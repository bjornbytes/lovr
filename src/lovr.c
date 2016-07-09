#include "lovr.h"
#include "util.h"

#include "graphics.h"
#include "model.h"
#include "event.h"

extern lua_State* L;

void lovrInit(lua_State* L) {

  // Write top-level lovr global
  lua_newtable(L);
  lua_setglobal(L, "lovr");

  // Register modules
  luaRegisterModule(L, "event", lovrEvent);
  luaRegisterModule(L, "graphics", lovrGraphics);

  // Register types
  luaRegisterType(L, "Model", lovrModel);

  // Run "main.lua" which will override/define pieces of lovr
  if (luaL_dofile(L, "main.lua")) {
    error("Failed to run main.lua");
    lua_pop(L, 1);
    exit(EXIT_FAILURE);
  }
}

void lovrDestroy() {
  glfwTerminate();
  exit(EXIT_SUCCESS);
}

void lovrRun(lua_State* L) {

  // lovr.run()
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  lua_call(L, 0, 0);
}

void lovrOnError(int code, const char* description) {
  error(description);
}

void lovrOnClose(GLFWwindow* _window) {
  if (_window == window) {

    // lovr.quit()
    lua_getglobal(L, "lovr");
    lua_getfield(L, -1, "quit");
    lua_call(L, 0, 0);

    if (glfwWindowShouldClose(window)) {
      glfwDestroyWindow(window);
      lovrDestroy();
    }
  }
}
