#include "resources/boot.lua.h"
#include "version.h"
#include "api.h"
#include "luax.h"
#include "platform.h"
#include "util.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

lua_State* lovrInit(lua_State* L, int argc, char** argv);
void lovrQuit(int status);
int main(int argc, char** argv);

#ifndef LOVR_USE_OCULUS_MOBILE

#ifdef EMSCRIPTEN
#include <emscripten.h>
typedef struct {
  lua_State* L;
  lua_State* T;
  int argc;
  char** argv;
} lovrEmscriptenContext;

void lovrDestroy(void* arg) {
  lovrEmscriptenContext* context = arg;
  lua_State* L = context->L;
  emscripten_cancel_main_loop();
  lua_close(L);
}

static void emscriptenLoop(void* arg) {
  lovrEmscriptenContext* context = arg;
  lua_getglobal(context->L, "_lovrHeadsetRenderError"); // webvr.c renderCallback failed
  bool haveRenderError = !lua_isnil(context->L, -1);
  if (!haveRenderError) {
    lua_pop(context->L, 1);
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
      lovrPlatformDestroy();
      exit(status);
    }
  }
}
#endif

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

    lua_State* T = lovrInit(L, argc, argv);
    if (!T) {
      return 1;
    }

    lovrSetErrorCallback((lovrErrorHandler) luax_vthrow, T);

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

  lovrPlatformDestroy();

  return status;
}

#endif

typedef enum { // What flag is being searched for?
  ARGFLAG_NONE, // Not processing a flag
  ARGFLAG_ROOT
} ArgFlag;

lua_State* lovrInit(lua_State* L, int argc, char** argv) {
  lovrAssert(lovrPlatformInit(), "Failed to initialize platform");

  // arg table
  // Args follow the lua standard https://en.wikibooks.org/wiki/Lua_Programming/command_line_parameter
  // In this standard, the contents of argv are put in a global table named "arg", but with indices offset
  // such that the "script" (in lovr, the game path) is at index 0. So all arguments (if any) intended for
  // the script/game are at successive indices starting with 1, and the exe and its arguments are in normal
  // order but stored in negative indices.
  //
  // Lovr can be run in ways normal lua can't. It can be run with an empty argv, or with no script (if fused).
  // So in the case of lovr:
  // * The script path will always be at index 0, but it can be nil.
  // * For a fused executable, whatever is given in argv as script name is still at 0, but will be ignored.
  // * The exe (argv[0]) will always be the lowest-integer index. If it's present, it will always be -1 or less.
  // * Any named arguments parsed by Lovr will be stored in the arg table at their named keys.
  // * An exe name will always be stored in the arg table at string key "exe", even if none was supplied in argv.
  lua_newtable(L);
  // push dummy "lovr" in case argv is empty
  lua_pushliteral(L, "lovr");
  lua_setfield(L, -2, "exe");

  bool exeFound = false;
  ArgFlag currentFlag = ARGFLAG_NONE;
  int lovrArgs = 0; // Arg count up to but not including the game path
  // One pass to parse flags
  for (int i = 0; i < argc; i++) {
    if (lovrArgs > 0) {
      // This argument is an argument to a -- flag
      if (currentFlag == ARGFLAG_ROOT) {
        lua_pushstring(L, argv[i]);
        lua_setfield(L, -2, "root");
        currentFlag = ARGFLAG_NONE;

      // This argument is a -- flag
      } else if (!strcmp(argv[1], "--root") || !strcmp(argv[1], "-r")) {
        currentFlag = ARGFLAG_ROOT;

      // This is the game path
      } else {
        break;
      }
    } else { // Found exe name
      lua_pushstring(L, argv[i]);
      lua_setfield(L, -2, "exe");
    }

    lovrArgs++;
  }
  // Now that we know the negative offset to start at, copy all args in the table
  for (int i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, -lovrArgs + i);
  }

  lua_setglobal(L, "arg");

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luaL_register(L, NULL, lovrModules);
  lua_pop(L, 2);

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
