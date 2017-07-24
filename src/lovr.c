#include "lovr.h"
#include "util.h"
#include "api/lovr.h"
#include "data/boot.lua.h"
#include "data/logo.png.h"
#include "lib/glfw.h"
#include <stdlib.h>

int luaopen_cjson(lua_State* L);
int luaopen_enet(lua_State* L);

static void onGlfwError(int code, const char* description) {
  error(description);
}

static int getStackTrace(lua_State* L) {
  const char* message = luaL_checkstring(L, -1);
  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");
  lua_pushstring(L, message);
  lua_pushinteger(L, 3);
  lua_call(L, 2, 1);
  return 1;
}

static void handleError(lua_State* L, const char* message) {
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "errhand");
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, message);
    lua_pcall(L, 1, 0, 0);
  } else {
    error(message);
  }
  lovrDestroy(1);
}

static int lovrGetOS(lua_State* L) {
#ifdef _WIN32
  lua_pushstring(L, "Windows");
  return 1;
#elif __APPLE__
  lua_pushstring(L, "macOS");
  return 1;
#elif LOVR_WEB
  lua_pushstring(L, "Web");
#else
  lua_pushnil(L);
#endif
  return 1;
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

  glfwSetErrorCallback(onGlfwError);

  if (!glfwInit()) {
    error("Error initializing glfw");
  }

  glfwSetTime(0);

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
  lua_pushcfunction(L, lovrGetOS);
  lua_setfield(L, -2, "getOS");
  lua_pushcfunction(L, lovrGetVersion);
  lua_setfield(L, -2, "getVersion");
  lua_pushlstring(L, (const char*) logo_png, logo_png_len);
  lua_setfield(L, -2, "_logo");
  lua_setglobal(L, "lovr");

  // Preload modules
  luax_preloadmodule(L, "lovr.audio", l_lovrAudioInit);
  luax_preloadmodule(L, "lovr.event", l_lovrEventInit);
  luax_preloadmodule(L, "lovr.filesystem", l_lovrFilesystemInit);
  luax_preloadmodule(L, "lovr.graphics", l_lovrGraphicsInit);
  luax_preloadmodule(L, "lovr.headset", l_lovrHeadsetInit);
  luax_preloadmodule(L, "lovr.math", l_lovrMathInit);
  luax_preloadmodule(L, "lovr.physics", l_lovrPhysicsInit);
  luax_preloadmodule(L, "lovr.timer", l_lovrTimerInit);

  // Preload libraries
  luax_preloadmodule(L, "json", luaopen_cjson);
  luax_preloadmodule(L, "enet", luaopen_enet);

  lua_pushcfunction(L, getStackTrace);
  if (luaL_loadbuffer(L, (const char*) boot_lua, boot_lua_len, "boot.lua") || lua_pcall(L, 0, 0, -2)) {
    handleError(L, lua_tostring(L, -1));
  }
}

void lovrDestroy(int exitCode) {
  glfwTerminate();
  exit(exitCode);
}

#ifdef LOVR_WEB
#include <emscripten.h>

static int stepRef = 0;

static void emscriptenLoop(void* arg) {
  lua_State* L = arg;

  lua_rawgeti(L, LUA_REGISTRYINDEX, stepRef);
  lua_call(L, 0, 0);
}

void lovrRun(lua_State* L) {

  // lovr.load
  lua_getglobal(L, "lovr");
  if (!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "load");
    if (!lua_isnil(L, -1)) {
      lua_call(L, 0, 0);
    } else {
      lua_pop(L, 1);
    }

    lua_getfield(L, -1, "step");
    stepRef = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  emscripten_set_main_loop_arg(emscriptenLoop, (void*) L, 0, 1);
}
#else
void lovrRun(lua_State* L) {
  lua_pushcfunction(L, getStackTrace);

  // lovr.run()
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  if (lua_pcall(L, 0, 1, -3)) {
    handleError(L, lua_tostring(L, -1));
  }

  // Exit with return value from lovr.run
  int exitCode = luaL_optint(L, -1, 0);
  lua_pop(L, 2);

  lovrDestroy(exitCode);
}
#endif
