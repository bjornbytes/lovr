#include "lovr.h"
#include "util.h"
#include <stdlib.h>

#include "lovr/event.h"
#include "lovr/graphics.h"
#include "lovr/headset.h"
#include "lovr/timer.h"

void lovrInit(lua_State* L) {

  // lovr = {}
  lua_newtable(L);
  lua_setglobal(L, "lovr");

  // Preload modules
  luaPreloadModule(L, "lovr.event", l_lovrEventInit);
  luaPreloadModule(L, "lovr.graphics", l_lovrGraphicsInit);
  luaPreloadModule(L, "lovr.headset", l_lovrHeadsetInit);
  luaPreloadModule(L, "lovr.timer", l_lovrTimerInit);

  // Bootstrap
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "%s",
    "local conf = { "
    "  modules = { "
    "    event = true, "
    "    graphics = true, "
    "    headset = true, "
    "    timer = true "
    "  } "
    "} "

    "local success, err = pcall(require, 'conf') "
    "if lovr.conf then "
    "  success, err = pcall(lovr.conf, conf) "
    "end "

    "if not success and err then "
    "  print('Could not run conf.lua') "
    "end "

    "local modules = { 'event', 'graphics', 'headset', 'timer' } "
    "for _, module in ipairs(modules) do "
    "  if conf.modules[module] then "
    "    lovr[module] = require('lovr.' .. module) "
    "  end "
    "end "

    "function lovr.run() "
    "  if lovr.load then lovr.load() end "
    "  while true do "
    "    lovr.event.poll() "
    "    local dt = lovr.timer.step() "
    "    if lovr.update then lovr.update(dt) end "
    "    lovr.graphics.clear() "
    "    if lovr.draw then "
    "      if lovr.headset then lovr.headset.renderTo(lovr.draw) "
    "      else lovr.draw() end "
    "    end "
    "    lovr.graphics.present() "
    "    lovr.timer.sleep(.001) "
    "  end "
    "end"
  );

  if (luaL_dostring(L, buffer)) {
    const char* message = luaL_checkstring(L, 1);
    error("Unable to bootstrap LOVR: %s", message);
  }

  lua_atpanic(L, lovrOnLuaError);
}

void lovrDestroy(int exitCode) {
  glfwTerminate();
  exit(exitCode);
}

void lovrRun(lua_State* L, char* root) {

  // Construct path to main.lua based on command line argument
  char path[512];
  if (root) {
    snprintf(path, sizeof(path), "%s/main.lua", root);
  } else {
    snprintf(path, sizeof(path), "main.lua");
  }

  // Run "main.lua" which will override/define pieces of lovr
  if (fileExists(path) && luaL_dofile(L, path)) {
    error("Failed to run main.lua");
    lua_pop(L, 1);
    exit(EXIT_FAILURE);
  }

  // lovr.run()
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  lua_call(L, 0, 0);
}

int lovrOnLuaError(lua_State* L) {
  const char* message = luaL_checkstring(L, -1);
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "errhand");

  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, message);
    lua_call(L, 1, 0);
  } else {
    error(message);
  }

  return 0;
}

void lovrOnGlfwError(int code, const char* description) {
  error(description);
}

void lovrOnClose(GLFWwindow* _window) {
  if (_window == window) {

    lua_State* L = (lua_State*) glfwGetWindowUserPointer(window);

    // lovr.quit()
    lua_getglobal(L, "lovr");
    lua_getfield(L, -1, "quit");

    if (!lua_isnil(L, -1)) {
      lua_call(L, 0, 0);
    }

    if (glfwWindowShouldClose(window)) {
      glfwDestroyWindow(window);
      lovrDestroy(0);
    }
  }
}
