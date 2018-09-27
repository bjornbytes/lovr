#include "api.h"
#include "lovr.h"
#include "resources/logo.png.h"
#include "lib/lua-cjson/lua_cjson.h"
#include "lib/lua-enet/enet.h"

static int l_lovrGetOS(lua_State* L) {
  const char* os = lovrGetOS();
  if (os) {
    lua_pushstring(L, os);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrGetVersion(lua_State* L) {
  int major, minor, patch;
  lovrGetVersion(&major, &minor, &patch);
  lua_pushinteger(L, major);
  lua_pushinteger(L, minor);
  lua_pushinteger(L, patch);
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

  lua_pushlstring(L, (const char*) logo_png, logo_png_len);
  lua_setfield(L, -2, "_logo");

#ifdef LOVR_ENABLE_AUDIO
  luax_preloadmodule(L, "lovr.audio", luaopen_lovr_audio);
#endif
#ifdef LOVR_ENABLE_DATA
  luax_preloadmodule(L, "lovr.data", luaopen_lovr_data);
#endif
#ifdef LOVR_ENABLE_EVENT
  luax_preloadmodule(L, "lovr.event", luaopen_lovr_event);
#endif
#ifdef LOVR_ENABLE_FILESYSTEM
  luax_preloadmodule(L, "lovr.filesystem", luaopen_lovr_filesystem);
#endif
#ifdef LOVR_ENABLE_GRAPHICS
  luax_preloadmodule(L, "lovr.graphics", luaopen_lovr_graphics);
#endif
#ifdef LOVR_ENABLE_HEADSET
  luax_preloadmodule(L, "lovr.headset", luaopen_lovr_headset);
#endif
#ifdef LOVR_ENABLE_MATH
  luax_preloadmodule(L, "lovr.math", luaopen_lovr_math);
#endif
#ifdef LOVR_ENABLE_PHYSICS
  luax_preloadmodule(L, "lovr.physics", luaopen_lovr_physics);
#endif
#ifdef LOVR_ENABLE_THREAD
  luax_preloadmodule(L, "lovr.thread", luaopen_lovr_thread);
#endif
#ifdef LOVR_ENABLE_TIMER
  luax_preloadmodule(L, "lovr.timer", luaopen_lovr_timer);
#endif
#ifdef LOVR_ENABLE_ENET
  luax_preloadmodule(L, "enet", luaopen_enet);
#endif
#ifdef LOVR_ENABLE_JSON
  luax_preloadmodule(L, "json", luaopen_cjson);
#endif
  return 1;
}
