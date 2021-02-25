#include "api.h"
#include "core/util.h"

const luaL_Reg lovrModules[] = {
  { "lovr", luaopen_lovr },
#ifndef LOVR_DISABLE_AUDIO
  { "lovr.audio", luaopen_lovr_audio },
#endif
#ifndef LOVR_DISABLE_DATA
  { "lovr.data", luaopen_lovr_data },
#endif
#ifndef LOVR_DISABLE_EVENT
  { "lovr.event", luaopen_lovr_event },
#endif
#ifndef LOVR_DISABLE_FILESYSTEM
  { "lovr.filesystem", luaopen_lovr_filesystem },
#endif
#ifndef LOVR_DISABLE_GRAPHICS
  { "lovr.graphics", luaopen_lovr_graphics },
#endif
#ifndef LOVR_DISABLE_HEADSET
  { "lovr.headset", luaopen_lovr_headset },
#endif
#ifndef LOVR_DISABLE_MATH
  { "lovr.math", luaopen_lovr_math },
#endif
#ifndef LOVR_DISABLE_PHYSICS
  { "lovr.physics", luaopen_lovr_physics },
#endif
#ifndef LOVR_DISABLE_SYSTEM
  { "lovr.system", luaopen_lovr_system },
#endif
#ifndef LOVR_DISABLE_THREAD
  { "lovr.thread", luaopen_lovr_thread },
#endif
#ifndef LOVR_DISABLE_TIMER
  { "lovr.timer", luaopen_lovr_timer },
#endif
  { NULL, NULL }
};

static int l_lovrGetVersion(lua_State* L) {
  lua_pushinteger(L, LOVR_VERSION_MAJOR);
  lua_pushinteger(L, LOVR_VERSION_MINOR);
  lua_pushinteger(L, LOVR_VERSION_PATCH);
  return 3;
}

static const luaL_Reg lovr[] = {
  { "_setConf", luax_setconf },
  { "getVersion", l_lovrGetVersion },
  { NULL, NULL }
};

int luaopen_lovr(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovr);
  return 1;
}
