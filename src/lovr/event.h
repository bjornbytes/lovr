#include "luax.h"
#include "vendor/map/map.h"

map_int_t EventTypes;

extern const luaL_Reg lovrEvent[];
int l_lovrEventInit(lua_State* L);
int l_lovrEventClear(lua_State* L);
int l_lovrEventPoll(lua_State* L);
int l_lovrEventPump(lua_State* L);
int l_lovrEventPush(lua_State* L);
int l_lovrEventQuit(lua_State* L);
