#include "api.h"
#include "version.h"
#include "resources/logo.png.h"

static int l_lovrGetOS(lua_State* L) {
#ifdef _WIN32
  lua_pushstring(L, "Windows");
#elif __APPLE__
  lua_pushstring(L, "macOS");
#elif EMSCRIPTEN
  lua_pushstring(L, "Web");
#elif __linux__
  lua_pushstring(L, "Linux");
#else
  lua_pushnil(L);
#endif
  return 1;
}

static int l_lovrGetVersion(lua_State* L) {
  lua_pushinteger(L, LOVR_VERSION_MAJOR);
  lua_pushinteger(L, LOVR_VERSION_MINOR);
  lua_pushinteger(L, LOVR_VERSION_PATCH);
  return 3;
}

static const luaL_Reg lovr[] = {
  { "_setConf", luax_setconf },
  { "getOS", l_lovrGetOS },
  { "getVersion", l_lovrGetVersion },
  { NULL, NULL }
};

int luaopen_lovr(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovr);
#ifdef LOVR_USE_LOGO
  lua_pushlstring(L, (const char*) logo_png, logo_png_len);
  lua_setfield(L, -2, "_logo");
#endif
  return 1;
}
