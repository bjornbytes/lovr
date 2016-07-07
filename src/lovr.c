#include "lovr.h"
#include "util.h"

#include "graphics.h"
#include "event.h"

void lovrInit(lua_State* L) {

  // Write top-level lovr global
  lua_newtable(L);
  lua_setglobal(L, "lovr");

  // Register modules
  luaRegisterModule(L, "event", lovrEvent);
  luaRegisterModule(L, "graphics", lovrGraphics);

  // Register types
  luaRegisterType(L, "model", lovrModel);

  // Run "main.lua" which will override/define pieces of lovr
  if (luaL_dofile(L, "main.lua")) {
    error("Failed to run main.lua");
    lua_pop(L, 1);
    exit(EXIT_FAILURE);
  }
}

void lovrRun(lua_State* L) {

  // lovr.run()
  lua_getglobal(L, "lovr");
  lua_getfield(L, -1, "run");
  lua_call(L, 0, 0);
}
