#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../joystick/joystick.h"

void luax_pushjoystick(lua_State* L, Joystick* joystick);
Joystick* luax_checkjoystick(lua_State* L, int index);
int luax_destroyjoystick(lua_State* L);
extern const luaL_Reg lovrJoystick[];

int l_lovrJoystickGetAngularAcceleration(lua_State* L);
int l_lovrJoystickGetAngularVelocity(lua_State* L);
int l_lovrJoystickGetAxes(lua_State* L);
int l_lovrJoystickGetAxis(lua_State* L);
int l_lovrJoystickGetAxisCount(lua_State* L);
int l_lovrJoystickGetButtonCount(lua_State* L);
int l_lovrJoystickGetLinearAcceleration(lua_State* L);
int l_lovrJoystickGetLinearVelocity(lua_State* L);
int l_lovrJoystickGetName(lua_State* L);
int l_lovrJoystickGetOrientation(lua_State* L);
int l_lovrJoystickGetPosition(lua_State* L);
int l_lovrJoystickIsDown(lua_State* L);
int l_lovrJoystickIsTracked(lua_State* L);
