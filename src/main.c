#include "lovr.h"
#include "luax.h"
#include "api.h"
#include "util.h"
#include "resources/boot.lua.h"
#include "lib/glfw.h"
#include <stdlib.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

static int errorCount = 0;

static void destroy(lua_State* L, int exitCode) {
  lovrDestroy();
  free(lovrCatch);
  lovrCatch = NULL;
  lua_close(L);
  glfwTerminate();
  exit(exitCode);
}

static void onGlfwError(int code, const char* description) {
  lovrThrow(description);
}

static void handleError(lua_State* L, const char* message) {
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "errhand");
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, message);
    lua_pcall(L, 1, 0, 0);
  } else {
    fprintf(stderr, "%s\n", message);
  }
  destroy(L, 1);
}

#ifdef EMSCRIPTEN
static int stepRef = 0;
static void emscriptenLoop(void* arg) {
  lua_State* L = arg;
  lua_rawgeti(L, LUA_REGISTRYINDEX, stepRef);
  lua_call(L, 0, 0);
}
#endif

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "--version") == 0) {
    printf("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

#ifndef EMSCRIPTEN
  do {
#endif

  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  glfwSetTime(0);
  glfwSetErrorCallback(onGlfwError);

  if (!glfwInit()) {
    lovrThrow("Error initializing glfw");
  }

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

  l_lovrInit(L);
  lua_setglobal(L, "lovr");

  lua_pushcfunction(L, luax_getstack);
  if (luaL_loadbuffer(L, (const char*) boot_lua, boot_lua_len, "boot.lua") || lua_pcall(L, 0, 0, -2)) {
    handleError(L, lua_tostring(L, -1));
  }

#ifdef EMSCRIPTEN
  lua_getglobal(L, "lovr");
  if (!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "load");
    if (!lua_isnil(L, -1)) {
      lua_getglobal(L, "arg");
      lua_call(L, 1, 0);
    } else {
      lua_pop(L, 1);
    }

    lua_getfield(L, -1, "step");
    stepRef = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  emscripten_set_main_loop_arg(emscriptenLoop, (void*) L, 0, 1);
  return 0;
#else
  lovrCatch = malloc(sizeof(jmp_buf));

  // Global error handler
  if (setjmp(*lovrCatch)) {

    // Failsafe in event that errhand throws
    if (errorCount++) {
      destroy(L, 1);
    }

    lua_pushcfunction(L, luax_getstack);
    lua_pushstring(L, lovrErrorMessage);
    lua_pcall(L, 1, 1, 0);
    handleError(L, lua_tostring(L, -1));
    destroy(L, 1);
  }

  // lovr.run()
  lua_pushcfunction(L, luax_getstack);
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  if (lua_pcall(L, 0, 1, -3)) {
    handleError(L, lua_tostring(L, -1));
  }

  // Exit with return value from lovr.run
  int exitCode = 0;
  int returnType = lua_type(L, -1);
  if (returnType == LUA_TSTRING && 0 == strcmp("restart", lua_tostring(L, -1))) {
    lua_close(L);
    lovrDestroy();
    continue;
  }

  exitCode = luaL_optint(L, -1, 0);
  destroy(L, exitCode);
#endif

#ifndef EMSCRIPTEN
  } while (true);
#endif

  return 0;
}
