#include "lovr.h"
#include "audio/audio.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "graphics/graphics.h"
#include "headset/headset.h"
#include "math/math.h"
#include "physics/physics.h"
#ifndef EMSCRIPTEN
#include "thread/thread.h"
#endif
#include "timer/timer.h"
#include "resources/boot.lua.h"
#include "lib/glfw.h"
#include "luax.h"
#include "api.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

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

    lovrDestroy();

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

static void onGlfwError(int code, const char* description) {
  lovrThrow(description);
}

void lovrDestroy() {
  lovrAudioDestroy();
  lovrEventDestroy();
  lovrFilesystemDestroy();
  lovrGraphicsDestroy();
  lovrHeadsetDestroy();
  lovrMathDestroy();
  lovrPhysicsDestroy();
#ifndef EMSCRIPTEN
  lovrThreadDeinit();
#endif
  lovrTimerDestroy();
}

bool lovrRun(int argc, char** argv, int* status) {
  bool checkingArgs = true;
  while (checkingArgs) {
    checkingArgs = false; // Set true to loop again
    if (argc > 1 && strcmp(argv[1], "--live") == 0) {
      if (!lovrFilesystemReloadEnable) {
        printf("Running LOVR in live-reload mode: lua files on disk will be watched.\n");
        lovrFilesystemReloadEnable = true;
      }
      argc--;
      argv++;
      checkingArgs = true;
    }
  }

  lua_State* L = lovrErrorContext = luaL_newstate();
  luaL_openlibs(L);

  glfwSetErrorCallback(onGlfwError);
  lovrAssert(glfwInit(), "Error initializing GLFW");
  glfwSetTime(0);

  // arg
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
  if (luaL_loadbuffer(L, (const char*) boot_lua, boot_lua_len, "boot.lua") || lua_pcall(L, 0, 1, -2)) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    lovrDestroy();
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
  while (lua_resume(L, 0) == LUA_YIELD) ;

  *status = lua_tonumber(L, -1);
  bool restart = lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "restart");

  lovrDestroy();
  lua_close(L);

  if (!restart) {
    glfwTerminate();
  }

  return restart;
#endif
}

void lovrQuit(int status) {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit = { false, status } });
}

const char* lovrGetOS() {
#ifdef _WIN32
  return "Windows";
#elif __APPLE__
  return "macOS";
#elif EMSCRIPTEN
  return "Web";
#elif __linux__
  return "Linux";
#else
  return NULL;
#endif
}

void lovrGetVersion(int* major, int* minor, int* patch) {
  *major = LOVR_VERSION_MAJOR;
  *minor = LOVR_VERSION_MINOR;
  *patch = LOVR_VERSION_PATCH;
}
