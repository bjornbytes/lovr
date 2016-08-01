#include "lovr.h"
#include "util.h"

#include "event.h"
#include "device.h"
#include "graphics.h"
#include "timer.h"

extern lua_State* L;

void lovrInit(lua_State* L) {

  // Write top-level lovr global
  lua_newtable(L);
  lua_setglobal(L, "lovr");

  // Register modules
  luaPreloadModule(L, "lovr.event", lovrPushEvent);
  luaPreloadModule(L, "lovr.device", lovrPushDevice);
  luaPreloadModule(L, "lovr.graphics", lovrPushGraphics);
  luaPreloadModule(L, "lovr.timer", lovrPushTimer);

  // Bootstrap
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "%s",
    "local conf = { "
    "  modules = { "
    "    event = true, "
    "    device = true, "
    "    graphics = true, "
    "    timer = true "
    "  } "
    "} "
    " "
    "local success, err = pcall(require, 'conf') "
    "if lovr.conf then "
    "  success, err = pcall(lovr.conf, conf) "
    "end "
    " "
    "if not success and err then "
    "  error(err, -1) "
    "end "
    " "
    "local modules = { 'event', 'device', 'graphics', 'timer' } "
    "for _, module in ipairs(modules) do "
    "  if conf.modules[module] then "
    "    lovr[module] = require('lovr.' .. module) "
    "  end "
    "end "
    " "
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

  if (luaL_dostring(L, buffer)) {
    const char* message = luaL_checkstring(L, 1);
    error("Unable to bootstrap LOVR: %s", message);
  }
}

void lovrDestroy() {
  glfwTerminate();
  exit(EXIT_SUCCESS);
}

void lovrRun(lua_State* L) {

  // Run "main.lua" which will override/define pieces of lovr
  if (luaL_dofile(L, "main.lua")) {
    error("Failed to run main.lua");
    lua_pop(L, 1);
    exit(EXIT_FAILURE);
  }

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
