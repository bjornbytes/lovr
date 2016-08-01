#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int lovrDeviceGetByName(lua_State* L);
int lovrDeviceGetHeadset(lua_State* L);
int lovrDeviceGetControllers(lua_State* L);

extern const luaL_Reg lovrDevice[];
int lovrPushDevice(lua_State* L);
