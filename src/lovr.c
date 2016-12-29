#include "lovr.h"
#include "lovr/event.h"
#include "lovr/filesystem.h"
#include "lovr/graphics.h"
#include "lovr/headset.h"
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
    lua_call(L, 1, 0);
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
  luax_preloadmodule(L, "lovr.event", l_lovrEventInit);
  luax_preloadmodule(L, "lovr.filesystem", l_lovrFilesystemInit);
  luax_preloadmodule(L, "lovr.graphics", l_lovrGraphicsInit);
  luax_preloadmodule(L, "lovr.headset", l_lovrHeadsetInit);
  luax_preloadmodule(L, "lovr.timer", l_lovrTimerInit);

  // Bootstrap
  char buffer[2048];
  snprintf(buffer, sizeof(buffer), "%s",
    "local conf = { "
    "  modules = { "
    "    event = true, "
    "    graphics = true, "
    "    headset = true, "
    "    timer = true "
    "  } "
    "} "

    "lovr.filesystem = require('lovr.filesystem') "
    "lovr.filesystem.init(arg[-2]) "
    "if not lovr.filesystem.setSource(lovr.filesystem.getExecutablePath()) and arg[1] then "
    "  lovr.filesystem.setSource(arg[1]) "
    "end "

    "local success, err = pcall(require, 'conf') "
    "if lovr.conf then "
    "  success, err = pcall(lovr.conf, conf) "
    "end "

    "local modules = { 'event', 'graphics', 'headset', 'timer' } "
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
    "    error('Unknown event: ', event) "
    "  end "
    "}) "

    "function lovr.step() "
    "  lovr.event.pump() "
    "  for name, a, b, c, d in lovr.event.poll() do "
    "    if name == 'quit' and (not lovr.quit or not lovr.quit()) then "
    "      return a "
    "    end "
    "    lovr.handlers[name](a, b, c, d) "
    "  end "
    "  local dt = lovr.timer.step() "
    "  if lovr.update then lovr.update(dt) end "
    "  lovr.graphics.clear() "
    "  lovr.graphics.origin() "
    "  if lovr.draw then "
    "    if lovr.headset and lovr.headset.isPresent() then "
    "      lovr.headset.renderTo(lovr.draw) "
    "    else "
    "      lovr.draw() "
    "    end "
    "  end "
    "  lovr.graphics.present() "
    "  lovr.timer.sleep(.001) "
    "end "

    "function lovr.run() "
    "  if lovr.load then lovr.load() end "
    "  while true do "
    "    local exit = lovr.step() "
    "    if exit then return exit end "
    "  end "
    "end "

    "function lovr.errhand(message, layer) "
    "  local stackTrace = debug.traceback('Error: ' .. tostring(message), 1 + (layer or 1)) "
    "  print((stackTrace:gsub('\\n[^\\n]+$', ''):gsub('stack traceback', 'Stack'))) "
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

#ifdef EMSCRIPTEN
#include <emscripten.h>
static void emscriptenLoop(void* arg) {
  lua_State* L = (lua_State*) arg;

  // lovr.step()
  lua_getglobal(L, "lovr");
  if (lua_isnil(L, -1)) {
    return;
  }

  lua_getfield(L, -1, "step");
  if (lua_isnil(L, -1)) {
    return;
  }

  lua_call(L, 0, 0);
}

void lovrRun(lua_State* L) {
  emscripten_set_main_loop_arg(emscriptenLoop, (void*) L, 0, 1);
}
#else
void lovrRun(lua_State* L) {

  // lovr.run()
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  lua_call(L, 0, 1);

  // Exit with return value from lovr.run
  int exitCode = luaL_optint(L, -1, 0);
  lovrDestroy(exitCode);
}
#endif
