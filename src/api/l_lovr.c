#include "api.h"
#include "util.h"
#include "platform.h"
#include "resources/logo.png.h"
#include "lib/lua-cjson/lua_cjson.h"
#include "lib/lua-enet/enet.h"

const luaL_Reg lovrModules[] = {
  { "lovr", luaopen_lovr },
#ifdef LOVR_ENABLE_AUDIO
  { "lovr.audio", luaopen_lovr_audio },
#endif
#ifdef LOVR_ENABLE_DATA
  { "lovr.data", luaopen_lovr_data },
#endif
#ifdef LOVR_ENABLE_EVENT
  { "lovr.event", luaopen_lovr_event },
#endif
#ifdef LOVR_ENABLE_FILESYSTEM
  { "lovr.filesystem", luaopen_lovr_filesystem },
#endif
#ifdef LOVR_ENABLE_GRAPHICS
  { "lovr.graphics", luaopen_lovr_graphics },
#endif
#ifdef LOVR_ENABLE_HEADSET
  { "lovr.headset", luaopen_lovr_headset },
#endif
#ifdef LOVR_ENABLE_MATH
  { "lovr.math", luaopen_lovr_math },
#endif
#ifdef LOVR_ENABLE_PHYSICS
  { "lovr.physics", luaopen_lovr_physics },
#endif
#ifdef LOVR_ENABLE_THREAD
  { "lovr.thread", luaopen_lovr_thread },
#endif
#ifdef LOVR_ENABLE_TIMER
  { "lovr.timer", luaopen_lovr_timer },
#endif
#ifdef LOVR_ENABLE_JSON
  { "cjson", luaopen_cjson },
#endif
#ifdef LOVR_ENABLE_ENET
  { "enet", luaopen_enet },
#endif
  { NULL, NULL }
};

static int l_lovrGetOS(lua_State* L) {
  lua_pushstring(L, lovrPlatformGetName());
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
  lua_pushlstring(L, (const char*) logo_png, logo_png_len);
  lua_setfield(L, -2, "_logo");
  return 1;
}
