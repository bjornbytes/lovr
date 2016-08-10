#include "lovr.h"
#include "util.h"

#include "lovr/event.h"
#include "lovr/graphics.h"
#include "lovr/joysticks.h"
#include "lovr/joystick.h"
#include "lovr/timer.h"

extern lua_State* L;

void lovrInit(lua_State* L) {

  // lovr = {}
  lua_newtable(L);
  lua_setglobal(L, "lovr");

  // Preload modules
  luaPreloadModule(L, "lovr.event", l_lovrEventInit);
  luaPreloadModule(L, "lovr.graphics", l_lovrGraphicsInit);
  luaPreloadModule(L, "lovr.joystick", l_lovrJoysticksInit);
  luaPreloadModule(L, "lovr.timer", l_lovrTimerInit);

  // Bootstrap
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "%s",
    "local conf = { "
    "  modules = { "
    "    event = true, "
    "    graphics = true, "
    "    joystick = true, "
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

    "local modules = { 'event', 'graphics', 'joystick', 'timer' } "
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
    "    if lovr.draw then lovr.draw() end "
    "    lovr.graphics.present() "
    "  end "
    "end"
  );

  if (luaL_dostring(L, buffer)) {
    const char* message = luaL_checkstring(L, 1);
    error("Unable to bootstrap LOVR: %s", message);
  }

  lua_atpanic(L, lovrOnLuaError);
}

void lovrDestroy() {
  glfwTerminate();
  exit(EXIT_SUCCESS);
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
  if (luaL_dofile(L, path)) {
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

void lovrOnJoystickAdded(Joystick* joystick) {
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "joystickadded");

  if (lua_isfunction(L, -1)) {
    luax_pushjoystick(L, joystick);
    lua_call(L, 1, 0);
  }
}

void lovrOnJoystickRemoved(Joystick* joystick) {
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "joystickremoved");

  if (lua_isfunction(L, -1)) {
    luax_pushjoystick(L, joystick);
    lua_call(L, 1, 0);
  }
}
