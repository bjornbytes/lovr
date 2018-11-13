#ifdef LOVR_ENABLE_EVENT
#include "event/event.h"
#endif
#include "resources/boot.lua.h"
#include "lib/glfw.h"
#include "version.h"
#include "luax.h"
#include "platform.h"
#include "util.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

lua_State* lovrInit(lua_State* L, int argc, char** argv);
void lovrQuit(int status);

#ifndef LOVR_USE_OCULUS_MOBILE

int main(int argc, char** argv) {
  if (argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
    lovrLog("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

  int status;
  bool restart;

  do {
    lua_State* L = luaL_newstate();
    luax_setmainstate(L);
    luaL_openlibs(L);

    lovrSetErrorCallback((lovrErrorHandler) luax_vthrow, L);

    lua_State* T = lovrInit(L, argc, argv);
    if (!T) {
      return 1;
    }

#ifdef EMSCRIPTEN
    lovrEmscriptenContext context = { L, T, argc, argv };
    emscripten_set_main_loop_arg(emscriptenLoop, (void*) &context, 0, 1);
    return 0;
#else
    while (lua_resume(T, 0) == LUA_YIELD) {
      lovrSleep(.001);
    }

    restart = lua_type(T, -1) == LUA_TSTRING && !strcmp(lua_tostring(T, -1), "restart");
    status = lua_tonumber(T, -1);
    lua_close(L);
    luax_setmainstate(NULL);
#endif
  } while (restart);

  glfwTerminate();

  return status;
}

#endif

#ifdef EMSCRIPTEN
#include <emscripten.h>
typedef struct {
  lua_State* L;
  lua_State* T;
  int argc;
  char** argv;
} lovrEmscriptenContext;

static void emscriptenLoop(void* arg) {
  lovrEmscriptenContext* context = arg;

  lua_getglobal(L, "_lovrHeadsetRenderError"); // webvr.c renderCallback failed
  bool haveRenderError = !lua_isnil(L, -1);
  if (!haveRenderError) {
    lua_pop(L, 1);
  }

  int coroutineArgs = luax_pushLovrHeadsetRenderError(context->T);

  if (lua_resume(context->T, coroutineArgs) != LUA_YIELD) {
    bool restart = lua_type(context->T, -1) == LUA_TSTRING && !strcmp(lua_tostring(context->T, -1), "restart");
    int status = lua_tonumber(context->T, -1);

    lua_close(context->L);
    emscripten_cancel_main_loop();

    if (restart) {
      main(context->argc, context->argv);
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

lua_State* lovrInit(lua_State* L, int argc, char** argv) {
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

  luax_registerloader(L, loadSelf, 1);

  lua_pushcfunction(L, luax_getstack);
  if (luaL_loadbuffer(L, (const char*) boot_lua, boot_lua_len, "boot.lua") || lua_pcall(L, 0, 1, -2)) {
    lovrWarn("%s\n", lua_tostring(L, -1));
    return NULL;
  }

  lua_State* T = lua_newthread(L);
  lua_pushvalue(L, -2);
  lua_xmove(L, T, 1);
  return T;
}

void lovrQuit(int status) {
#ifdef LOVR_ENABLE_EVENT
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, status } });
#endif
}
