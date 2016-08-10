#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

extern const luaL_Reg lovrJoysticks[];
int l_lovrJoysticksInit(lua_State* L);
int l_lovrJoysticksGetJoystickCount(lua_State* L);
int l_lovrJoysticksGetJoysticks(lua_State* L);
