#include "api/api.h"
#include "event/event.h"
#include "core/os.h"
#include "util.h"
#include "boot.lua.h"
#include "nogame.lua.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>

typedef struct {
  lua_State* L;
  lua_State* T;
  int argc;
  char** argv;
} lovrEmscriptenContext;

static void emscriptenLoop(void*);
#endif

static Variant cookie;

static int luaopen_lovr_nogame(lua_State* L) {
  if (!luaL_loadbuffer(L, (const char*) etc_nogame_lua, etc_nogame_lua_len, "@nogame.lua")) {
    lua_call(L, 0, 1);
  }

  return 1;
}

int main(int argc, char** argv) {
  if (argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
    os_open_console();
    printf("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

  if (argc > 1 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
    os_open_console();
    printf(
      "usage: lovr [options] [<source>]\n\n"
      "options:\n"
      "  -h, --help\t\tShow help and exit\n"
      "  -v, --version\t\tShow version and exit\n"
      "  --console\t\tAttach Windows console\n\n"
      "<source> can be a Lua file, a folder, or a zip archive\n"
    );
    exit(0);
  }

  if (!os_init()) {
    fprintf(stderr, "Failed to initialize platform");
    exit(1);
  }

  int status;
  bool restart;

  do {
    lua_State* L = luaL_newstate();
    luax_setmainthread(L);
    luaL_openlibs(L);
    luax_preload(L);

    const luaL_Reg nogame[] = {
      { "nogame", luaopen_lovr_nogame },
      { NULL, NULL }
    };

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    luax_register(L, nogame);
    lua_pop(L, 2);

    // arg table
    lua_newtable(L);
    lua_pushstring(L, argc > 0 ? argv[0] : "lovr");
    lua_setfield(L, -2, "exe");
    luax_pushvariant(L, &cookie);
    lua_setfield(L, -2, "restart");

    int argOffset = 1;
    for (int i = 1; i < argc; i++, argOffset++) {
      if (!strcmp(argv[i], "--console")) {
        os_open_console();
      } else {
        break; // This is the project path
      }
    }

    // Now that we know the negative offset to start at, copy all args in the table
    for (int i = 0; i < argc; i++) {
      lua_pushstring(L, argv[i]);
      lua_rawseti(L, -2, -argOffset + i);
    }
    lua_setglobal(L, "arg");

    lua_pushcfunction(L, luax_getstack);
    if (luaL_loadbuffer(L, (const char*) etc_boot_lua, etc_boot_lua_len, "@boot.lua") || lua_pcall(L, 0, 1, -2)) {
      fprintf(stderr, "%s\n", lua_tostring(L, -1));
      return 1;
    }

    lua_State* T = lua_newthread(L);
    lua_pushvalue(L, -2);
    lua_xmove(L, T, 1);

    lovrSetErrorCallback(luax_vthrow, T);
    lovrSetLogCallback(luax_vlog, T);

#ifdef EMSCRIPTEN
    lovrEmscriptenContext context = { L, T, argc, argv };
    emscripten_set_main_loop_arg(emscriptenLoop, (void*) &context, 0, 1);
    return 0;
#endif

    while (luax_resume(T, 0) == LUA_YIELD) {
      os_sleep(0.);
    }

    restart = lua_type(T, 1) == LUA_TSTRING && !strcmp(lua_tostring(T, 1), "restart");
    status = lua_tonumber(T, 1);
    luax_checkvariant(T, 2, &cookie);
    if (cookie.type == TYPE_OBJECT) {
      cookie.type = TYPE_NIL;
      memset(&cookie.value, 0, sizeof(cookie.value));
    }
    lua_close(L);
  } while (restart);

  os_destroy();

  return status;
}

#ifdef EMSCRIPTEN
// Called by JS, don't delete
void lovrDestroy(void* arg) {
  if (arg) {
    lovrEmscriptenContext* context = arg;
    lua_State* L = context->L;
    emscripten_cancel_main_loop();
    lua_close(L);
    os_destroy();
  }
}

static void emscriptenLoop(void* arg) {
  lovrEmscriptenContext* context = arg;
  lua_State* T = context->T;

  if (luax_resume(T, 0) != LUA_YIELD) {
    bool restart = lua_type(T, 1) == LUA_TSTRING && !strcmp(lua_tostring(T, 1), "restart");
    int status = lua_tonumber(T, 1);
    luax_checkvariant(T, 2, &cookie);
    if (cookie.type == TYPE_OBJECT) {
      cookie.type = TYPE_NIL;
      memset(&cookie.value, 0, sizeof(cookie.value));
    }

    lua_close(context->L);
    emscripten_cancel_main_loop();

    if (restart) {
      main(context->argc, context->argv);
    } else {
      os_destroy();
      exit(status);
    }
  }
}
#endif
