#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int lovrJoysticksGetJoystickCount(lua_State* L);

extern const luaL_Reg lovrJoysticks[];
int lovrInitJoysticks(lua_State* L);
