#ifdef LOVR_ENABLE_EVENT
#include "event/event.h"
#endif
#include "resources/boot.lua.h"
#include "lib/glfw.h"
#include "version.h"
#include "luax.h"
#include "util.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool lovrRun(int argc, char** argv, int* status);
void lovrQuit(int status);

int main(int argc, char** argv) {
  if (argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
    printf("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

  int status;
  while (1) {
    if (!lovrRun(argc, argv, &status)) {
      return status;
    }
  }
}

#ifdef EMSCRIPTEN
#include <emscripten.h>
typedef struct {
  lua_State* L;
  int ref;
  int argc;
  char** argv;
} lovrEmscriptenContext;

static void emscriptenLoop(void* arg) {
  lovrEmscriptenContext* context = arg;
  lua_State* L = context->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, context->ref);
  if (lua_resume(L, 0) != LUA_YIELD) {
    int status = lua_tonumber(L, -1);
    bool isRestart = lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "restart");

    lua_close(L);
    emscripten_cancel_main_loop();

    if (isRestart) {
      lovrRun(context->argc, context->argv, &status);
    } else {
      glfwTerminate();
      exit(status);
    }
  }
}
#endif

static int loadSelf(lua_State* L) {
  const char* moduleFunction = luaL_gsub(L, lua_tostring(L, -1), ".", "_");
  char* hyphen = strchr(moduleFunction, '-');
  moduleFunction = hyphen ? hyphen + 1 : moduleFunction;
  char executablePath[1024] = { 0 };
  lovrGetExecutablePath(executablePath, 1024);
  luax_loadlib(L, executablePath, moduleFunction);
  return 1;
}

static void onGlfwError(int code, const char* description) {
  lovrThrow(description);
}

bool lovrRun(int argc, char** argv, int* status) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  lovrSetErrorCallback((lovrErrorHandler) luax_vthrow, L);

  glfwSetErrorCallback(onGlfwError);
  lovrAssert(glfwInit(), "Error initializing GLFW");
  glfwSetTime(0);

  // arg
  lua_newtable(L);
  lua_pushstring(L, "lovr");
  lua_rawseti(L, -2, -1);
  for (int i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i == 0 ? -2 : i);
  }
  lua_setglobal(L, "arg");

  luax_registerloader(L, loadSelf, 2);

  lua_pushcfunction(L, luax_getstack);
  if (luaL_loadbuffer(L, (const char*) boot_lua, boot_lua_len, "boot.lua") || lua_pcall(L, 0, 1, -2)) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    lua_close(L);
    *status = 1;
    return false;
  }

  lua_newthread(L);
  lua_pushvalue(L, -2);
#ifdef EMSCRIPTEN
  lovrEmscriptenContext context = { L, luaL_ref(L, LUA_REGISTRYINDEX), argc, argv };
  emscripten_set_main_loop_arg(emscriptenLoop, (void*) &context, 0, 1);
  *status = 0;
  return false;
#else
  while (lua_resume(L, 0) == LUA_YIELD) {
    lovrSleep(.001);
  }

  *status = lua_tonumber(L, -1);
  bool restart = lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "restart");
  lua_close(L);

  if (!restart) {
    glfwTerminate();
  }

  return restart;
#endif
}

void lovrQuit(int status) {
#ifdef LOVR_ENABLE_EVENT
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, status } });
#endif
}
