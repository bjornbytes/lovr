#include "api.h"
#include "lovr.h"
#include "resources/logo.png.h"
#include "lib/lua-cjson/lua_cjson.h"
#include "lib/lua-enet/enet.h"

int l_lovrInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovr);

  lua_pushlstring(L, (const char*) logo_png, logo_png_len);
  lua_setfield(L, -2, "_logo");

  luax_preloadmodule(L, "lovr.audio", l_lovrAudioInit);
  luax_preloadmodule(L, "lovr.data", l_lovrDataInit);
  luax_preloadmodule(L, "lovr.event", l_lovrEventInit);
  luax_preloadmodule(L, "lovr.filesystem", l_lovrFilesystemInit);
  luax_preloadmodule(L, "lovr.graphics", l_lovrGraphicsInit);
  luax_preloadmodule(L, "lovr.headset", l_lovrHeadsetInit);
  luax_preloadmodule(L, "lovr.math", l_lovrMathInit);
  luax_preloadmodule(L, "lovr.physics", l_lovrPhysicsInit);
  luax_preloadmodule(L, "lovr.thread", l_lovrThreadInit);
  luax_preloadmodule(L, "lovr.timer", l_lovrTimerInit);
  luax_preloadmodule(L, "json", luaopen_cjson);
  luax_preloadmodule(L, "enet", luaopen_enet);
  return 1;
}

int l_lovrGetOS(lua_State* L) {
  const char* os = lovrGetOS();
  if (os) {
    lua_pushstring(L, os);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

int l_lovrGetVersion(lua_State* L) {
  int major, minor, patch;
  lovrGetVersion(&major, &minor, &patch);
  lua_pushinteger(L, major);
  lua_pushinteger(L, minor);
  lua_pushinteger(L, patch);
  return 3;
}

const luaL_Reg lovr[] = {
  { "_setConf", luax_setconf },
  { "getOS", l_lovrGetOS },
  { "getVersion", l_lovrGetVersion },
  { NULL, NULL }
};
