#include "api/api.h"
#include "core/os.h"
#include "util.h"
#include "boot.lua.h"
#include <lualib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>

static void loop(void* arg) {
  lua_State* T = arg;
  if (luax_resume(T, 0) != LUA_YIELD) {
    int status = lua_tointeger(T, -1);
    emscripten_cancel_main_loop();
    lua_close(T);
    os_destroy();
    exit(status);
  }
}
#endif

int main(int argc, char** argv) {
  os_init();

  for (;;) {
    lua_State* L = luaL_newstate();
    luax_setmainthread(L);
    luaL_openlibs(L);
    luax_preload(L);

    lua_newtable(L);
    static Variant cookie;
    luax_pushvariant(L, &cookie);
    lua_setfield(L, -2, "restart");
    for (int i = 0; i < argc; i++) {
      lua_pushstring(L, argv[i]);
      lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");

    lua_pushcfunction(L, luax_getstack);
    int status = luax_loadbufferx(L, (const char*) etc_boot_lua, etc_boot_lua_len, "@boot.lua", NULL);
    if (status != 0 || lua_pcall(L, 0, 1, -2)) {
      fprintf(stderr, "%s\n", lua_tostring(L, -1));
      os_destroy();
      return 1;
    }

    lua_State* T = lua_tothread(L, -1);
    lovrSetLogCallback(luax_vlog, T);

#ifdef EMSCRIPTEN
    emscripten_set_main_loop_arg(loop, T, 0, 1);
    return 0;
#endif

    while (luax_resume(T, 0) == LUA_YIELD) {
      os_sleep(0.);
    }

    if (lua_type(T, 1) == LUA_TSTRING && !strcmp(lua_tostring(T, 1), "restart")) {
      luax_checkvariant(T, 2, &cookie);
      if (cookie.type == TYPE_OBJECT) memset(&cookie, 0, sizeof(cookie));
      luax_close(L);
      continue;
    } else {
      int status = lua_tointeger(T, 1);
      luax_close(L);
      os_destroy();
      return status;
    }
  }
}
