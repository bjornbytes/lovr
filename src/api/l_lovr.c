#include "api.h"
#include "core/log.h"
#include "core/os.h"
#include "core/util.h"
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

static int l_lovrLog(lua_State* L) {
  luaL_Buffer buffer;
  int n = lua_gettop(L);
  lua_getglobal(L, "tostring");
  luaL_buffinit(L, &buffer);
  for (int i = 1; i <= n; i++) {
    lua_pushvalue(L, -1);
    lua_pushvalue(L, i);
    lua_call(L, 1, 1);
    lovrAssert(lua_type(L, -1) == LUA_TSTRING, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
    if (i > 1) {
      luaL_addchar(&buffer, '\t');
    }
    luaL_addvalue(&buffer);
  }
  luaL_pushresult(&buffer);
  log_write(LOG_INFO, "%s\n", lua_tostring(L, -1));
  return 0;
}

static const luaL_Reg lovr[] = {
  { "_setConf", luax_setconf },
  { "getOS", l_lovrGetOS },
  { "getVersion", l_lovrGetVersion },
  { "log", l_lovrLog },
  { NULL, NULL }
};

LOVR_EXPORT int luaopen_lovr(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovr);
  return 1;
}
