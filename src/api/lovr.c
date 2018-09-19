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

#ifdef LOVR_ENABLE_AUDIO
  luax_preloadmodule(L, "lovr.audio", l_lovrAudioInit);
#endif
#ifdef LOVR_ENABLE_DATA
  luax_preloadmodule(L, "lovr.data", l_lovrDataInit);
#endif
#ifdef LOVR_ENABLE_EVENT
  luax_preloadmodule(L, "lovr.event", l_lovrEventInit);
#endif
#ifdef LOVR_ENABLE_FILESYSTEM
  luax_preloadmodule(L, "lovr.filesystem", l_lovrFilesystemInit);
#endif
#ifdef LOVR_ENABLE_GRAPHICS
  luax_preloadmodule(L, "lovr.graphics", l_lovrGraphicsInit);
#endif
#ifdef LOVR_ENABLE_HEADSET
  luax_preloadmodule(L, "lovr.headset", l_lovrHeadsetInit);
#endif
#ifdef LOVR_ENABLE_MATH
  luax_preloadmodule(L, "lovr.math", l_lovrMathInit);
#endif
#ifdef LOVR_ENABLE_PHYSICS
  luax_preloadmodule(L, "lovr.physics", l_lovrPhysicsInit);
#endif
#ifdef LOVR_ENABLE_THREAD
  luax_preloadmodule(L, "lovr.thread", l_lovrThreadInit);
#endif
#ifdef LOVR_ENABLE_TIMER
  luax_preloadmodule(L, "lovr.timer", l_lovrTimerInit);
#endif
#ifdef LOVR_ENABLE_ENET
  luax_preloadmodule(L, "enet", luaopen_enet);
#endif
#ifdef LOVR_ENABLE_JSON
  luax_preloadmodule(L, "json", luaopen_cjson);
#endif
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
