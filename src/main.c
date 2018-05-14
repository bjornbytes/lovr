#include "lovr.h"
#include "luax.h"
#include "api.h"
#include "util.h"
#include "resources/boot.lua.h"
#include "lib/glfw.h"
#include <stdlib.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
static int runRef = 0;
static void emscriptenLoop(void* arg) {
  lua_State* L = arg;
  lua_rawgeti(L, LUA_REGISTRYINDEX, runRef);
  lua_resume(L, 0);
}
#endif

static void onGlfwError(int code, const char* description) {
  lovrThrow(description);
}

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "--version") == 0) {
    printf("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

  while (true) {
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
      glfwTerminate();
      exit(1);
    }

    lua_newthread(L);
    lua_pushvalue(L, -2);
#ifdef EMSCRIPTEN
    runRef = luaL_ref(L, LUA_REGISTRYINDEX);
    emscripten_set_main_loop_arg(emscriptenLoop, (void*) L, 0, 1);
    return 0;
#else
    while (lua_resume(L, 0) == LUA_YIELD) ;

    int exitCode = lua_tonumber(L, -1);
    bool isRestart = lua_type(L, -1) == LUA_TSTRING && !strcmp(lua_tostring(L, -1), "restart");

    lovrDestroy();
    lua_close(L);

    if (!isRestart) {
      glfwTerminate();
      exit(exitCode);
    }
#endif
  }
}
