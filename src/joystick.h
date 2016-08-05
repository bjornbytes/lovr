#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef enum {
  JOYSTICK_TYPE_GLFW,
  JOYSTICK_TYPE_OSVR
} JoystickType;

typedef struct {
  JoystickType type;
  int index;
} Joystick;

void luax_pushjoystick(lua_State* L, Joystick* joystick);
Joystick* luax_checkjoystick(lua_State* L, int index);

int lovrJoystickIsGamepad(lua_State* L);
int lovrJoystickIsTracked(lua_State* L);
int lovrJoystickGetRawAxes(lua_State* L);
int lovrJoystickGetRawButtons(lua_State* L);
extern const luaL_Reg lovrJoystick[];
