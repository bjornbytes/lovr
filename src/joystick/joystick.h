#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../vendor/map/map.h"
#include "../osvr.h"

typedef enum {
  JOYSTICK_TYPE_GLFW,
  JOYSTICK_TYPE_OSVR
} JoystickType;

typedef map_int_t JoystickMapping;

typedef struct {
  JoystickType type;
  JoystickMapping axisMapping;
  JoystickMapping buttonMapping;
  int glfwIndex;
  OSVR_ClientInterface* osvrTrackerInterface;
  OSVR_ClientInterface* osvrAxisInterfaces[4];
  OSVR_ClientInterface* osvrButtonInterfaces[8];
} Joystick;

void luax_pushjoystick(lua_State* L, Joystick* joystick);
Joystick* luax_checkjoystick(lua_State* L, int index);

void lovrJoystickDestroy(Joystick* joystick);
int lovrJoystickMapAxis(Joystick* joystick, const char* key);
int lovrJoystickMapButton(Joystick* joystick, const char* key);
unsigned char lovrJoystickGetButtonState(Joystick* joystick, int buttonIndex);

int lovrJoystickIsGamepad(lua_State* L);
int lovrJoystickIsTracked(lua_State* L);
int lovrJoystickIsDown(lua_State* L);
int lovrJoystickGetAxis(lua_State* L);
int lovrJoystickGetPosition(lua_State* L);
int lovrJoystickGetOrientation(lua_State* L);
int lovrJoystickGetLinearVelocity(lua_State* L);
int lovrJoystickGetAngularVelocity(lua_State* L);
int lovrJoystickGetLinearAcceleration(lua_State* L);
int lovrJoystickGetAngularAcceleration(lua_State* L);
int lovrJoystickGetMapping(lua_State* L);
int lovrJoystickSetMapping(lua_State* L);
int lovrJoystickGetRawAxes(lua_State* L);
int lovrJoystickGetRawButtons(lua_State* L);
extern const luaL_Reg lovrJoystick[];
