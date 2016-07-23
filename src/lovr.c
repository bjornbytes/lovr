#include "lovr.h"
#include "util.h"

#include "event.h"
#include "device.h"
#include "graphics.h"
#include "timer.h"

#include "model.h"
#include "buffer.h"
#include "interface.h"

extern lua_State* L;

void lovrInit(lua_State* L) {

  // Write top-level lovr global
  lua_newtable(L);
  lua_setglobal(L, "lovr");

  // Register modules
  luaRegisterModule(L, "event", lovrEvent);
  luaRegisterModule(L, "device", lovrDevice);
  luaRegisterModule(L, "graphics", lovrGraphics);
  luaRegisterModule(L, "timer", lovrTimer);

  // Register types
  luaRegisterType(L, "Model", lovrModel);
  luaRegisterType(L, "Buffer", lovrBuffer);
  luaRegisterType(L, "Interface", lovrInterface);

  // Default lovr.run
  char buffer[512];
  snprintf(buffer, sizeof(buffer), "%s",
    "function lovr.run() "
    "  if lovr.load then lovr.load() end "
    "  while true do "
    "    lovr.event.poll() "
    "    local dt = lovr.timer.step() "
    "    if lovr.update then lovr.update(dt) end "
    "    lovr.graphics.clear() "
    "    if lovr.draw then lovr.draw() end "
    "    lovr.graphics.present() "
    "  end "
    "end"
  );

  luaL_dostring(L, buffer);

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

    if (!lua_isnil(L, -1)) {
      lua_call(L, 0, 0);
    }

    if (glfwWindowShouldClose(window)) {
      glfwDestroyWindow(window);
      lovrDestroy();
    }
  }
}
