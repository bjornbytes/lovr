#include "lovr.h"
#include "lovr/audio.h"
#include "lovr/event.h"
#include "lovr/filesystem.h"
#include "lovr/graphics.h"
#include "lovr/headset.h"
#include "lovr/math.h"
#include "lovr/timer.h"
#include "glfw.h"
#include "util.h"
#include <stdlib.h>

static int handleLuaError(lua_State* L) {
  const char* message = luaL_checkstring(L, -1);
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "errhand");

  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, message);
    if (lua_pcall(L, 1, 0, 0)) {
      error(lua_tostring(L, -1));
    }
  } else {
    error(message);
  }

  return 0;
}

static int lovrGetVersion(lua_State* L) {
  lua_pushnumber(L, LOVR_VERSION_MAJOR);
  lua_pushnumber(L, LOVR_VERSION_MINOR);
  lua_pushnumber(L, LOVR_VERSION_PATCH);
  return 3;
}

void lovrInit(lua_State* L, int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "--version") == 0) {
    printf("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

  initGlfw();

  // arg global
  lua_newtable(L);
  if (argc > 0) {
    lua_pushstring(L, argv[0]);
    lua_rawseti(L, -2, -2);
  }

  lua_pushstring(L, "lovr");
  lua_rawseti(L, -2, -1);
  for (int i = 1; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i);
  }

  lua_setglobal(L, "arg");

  // lovr
  lua_newtable(L);
  lua_pushcfunction(L, lovrGetVersion);
  lua_setfield(L, -2, "getVersion");
  lua_setglobal(L, "lovr");

  // Preload modules
  luax_preloadmodule(L, "lovr.audio", l_lovrAudioInit);
  luax_preloadmodule(L, "lovr.event", l_lovrEventInit);
  luax_preloadmodule(L, "lovr.filesystem", l_lovrFilesystemInit);
  luax_preloadmodule(L, "lovr.graphics", l_lovrGraphicsInit);
  luax_preloadmodule(L, "lovr.headset", l_lovrHeadsetInit);
  luax_preloadmodule(L, "lovr.math", l_lovrMathInit);
  luax_preloadmodule(L, "lovr.timer", l_lovrTimerInit);

  // Bootstrap
  char buffer[8192];
  snprintf(buffer, sizeof(buffer), "%s",
    "local conf = { "
    "  modules = { "
    "    audio = true, "
    "    event = true, "
    "    graphics = true, "
    "    headset = true, "
    "    math = true, "
    "    timer = true "
    "  } "
    "} "

    "lovr.filesystem = require('lovr.filesystem') "

    "local success, err = pcall(require, 'conf') "
    "if lovr.conf then "
    "  success, err = pcall(lovr.conf, conf) "
    "end "

    "local modules = { 'audio', 'event', 'graphics', 'headset', 'math', 'timer' } "
    "for _, module in ipairs(modules) do "
    "  if conf.modules[module] then "
    "    lovr[module] = require('lovr.' .. module) "
    "  end "
    "end "

    "lovr.filesystem.setIdentity(conf.identity or 'default') "

    "lovr.handlers = setmetatable({ "
    "  quit = function() end, "
    "  controlleradded = function(c) "
    "    if lovr.controlleradded then lovr.controlleradded(c) end "
    "  end, "
    "  controllerremoved = function(c) "
    "    if lovr.controllerremoved then lovr.controllerremoved(c) end "
    "  end "
    "}, { "
    "  __index = function(self, event) "
    "    error('Unknown event: ' .. tostring(event)) "
    "  end "
    "}) "

    "function lovr.run() "
    "  if lovr.load then lovr.load() end "
    "  while true do "
    "    lovr.event.pump() "
    "    for name, a, b, c, d in lovr.event.poll() do "
    "      if name == 'quit' and (not lovr.quit or not lovr.quit()) then "
    "        return a "
    "      end "
    "      lovr.handlers[name](a, b, c, d) "
    "    end "
    "    local dt = lovr.timer.step() "
    "    if lovr.audio then "
    "      lovr.audio.update() "
    "      if lovr.headset and lovr.headset.isPresent() then "
    "        lovr.audio.setOrientation(lovr.headset.getOrientation()) "
    "        lovr.audio.setPosition(lovr.headset.getPosition()) "
    "        lovr.audio.setVelocity(lovr.headset.getVelocity()) "
    "      end "
    "    end "
    "    if lovr.update then lovr.update(dt) end "
    "    lovr.graphics.clear() "
    "    lovr.graphics.origin() "
    "    if lovr.draw then "
    "      if lovr.headset and lovr.headset.isPresent() then "
    "        lovr.headset.renderTo(lovr.draw) "
    "      else "
    "        lovr.draw() "
    "      end "
    "    end "
    "    lovr.graphics.present() "
    "    lovr.timer.sleep(.001) "
    "  end "
    "end "

    "function lovr.errhand(message) "
    "  local stackTrace = debug.traceback('Error: ' .. tostring(message), 1) "
    "  local message = stackTrace:gsub('\\n[^\\n]+$', ''):gsub('\\t', ''):gsub('stack traceback', 'Stack') "
    "  print(message) "
    "  lovr.graphics.setBackgroundColor(26, 25, 28) "
    "  lovr.graphics.setColor(220, 220, 220) "
    "  local function render() "
    "    lovr.graphics.origin() "
    "    lovr.graphics.print(message, 0, 0, -2) "
    "  end "
    "  while true do "
    "    lovr.event.pump() "
    "    for name in lovr.event.poll() do "
    "      if name == 'quit' then return end "
    "    end "
    "    lovr.graphics.clear() "
    "    if lovr.headset and lovr.headset.isPresent() then "
    "      lovr.headset.renderTo(render) "
    "    else "
    "      render() "
    "    end "
    "    lovr.graphics.present() "
    "    lovr.timer.sleep(.01) "
    "  end "
    "end "

    "if lovr.filesystem.isFile('main.lua') then "
    "  require('main') "
    "end"
  );

  if (luaL_dostring(L, buffer)) {
    const char* message = luaL_checkstring(L, 1);
    error("Unable to bootstrap LOVR: %s", message);
  }

  lua_atpanic(L, handleLuaError);
}

void lovrDestroy(int exitCode) {
  glfwTerminate();
  exit(exitCode);
}

void lovrRun(lua_State* L) {

  // lovr.run()
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  lua_call(L, 0, 1);

  // Exit with return value from lovr.run
  int exitCode = luaL_optint(L, -1, 0);
  lovrDestroy(exitCode);
}
