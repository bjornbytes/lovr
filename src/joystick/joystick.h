#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../osvr.h"

typedef struct {
  int isTracked;
  int glfwIndex;
  OSVR_ClientInterface* osvrTrackerInterface;
  OSVR_ClientInterface* osvrAxisInterfaces[4];
  OSVR_ClientInterface* osvrButtonInterfaces[8];
} Joystick;

void luax_pushjoystick(lua_State* L, Joystick* joystick);
Joystick* luax_checkjoystick(lua_State* L, int index);

void lovrJoystickDestroy(Joystick* joystick);

int lovrJoystickGetAngularAcceleration(lua_State* L);
int lovrJoystickGetAngularVelocity(lua_State* L);
int lovrJoystickGetAxes(lua_State* L);
int lovrJoystickGetAxis(lua_State* L);
int lovrJoystickGetAxisCount(lua_State* L);
int lovrJoystickGetButtonCount(lua_State* L);
int lovrJoystickGetLinearAcceleration(lua_State* L);
int lovrJoystickGetLinearVelocity(lua_State* L);
int lovrJoystickGetName(lua_State* L);
int lovrJoystickGetOrientation(lua_State* L);
int lovrJoystickGetPosition(lua_State* L);
int lovrJoystickIsDown(lua_State* L);
int lovrJoystickIsTracked(lua_State* L);
extern const luaL_Reg lovrJoystick[];
