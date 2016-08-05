#include "joystick.h"
#include "glfw.h"

void luax_pushjoystick(lua_State* L, Joystick* joystick) {
  Joystick** userdata = (Joystick**) lua_newuserdata(L, sizeof(Joystick*));

  luaL_getmetatable(L, "Joystick");
  lua_setmetatable(L, -2);

  *userdata = joystick;
}

Joystick* luax_checkjoystick(lua_State* L, int index) {
  return *(Joystick**) luaL_checkudata(L, index, "Joystick");
}

int lovrJoystickIsGamepad(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushboolean(L, joystick->type == JOYSTICK_TYPE_GLFW);
  return 1;
}

int lovrJoystickIsTracked(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushboolean(L, joystick->type == JOYSTICK_TYPE_OSVR);
  return 1;
}

int lovrJoystickGetRawAxes(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  lua_newtable(L);

  if (joystick->type == JOYSTICK_TYPE_OSVR) {
    return 1;
  }

  int axisCount;
  const float* axes = glfwGetJoystickAxes(joystick->index, &axisCount);

  for (int i = 0; i < axisCount; i++) {
    lua_pushnumber(L, axes[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

int lovrJoystickGetRawButtons(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  lua_newtable(L);

  if (joystick->type == JOYSTICK_TYPE_OSVR) {
    return 1;
  }

  int buttonCount;
  const unsigned char* buttons = glfwGetJoystickButtons(joystick->index, &buttonCount);

  for (int i = 0; i < buttonCount; i++) {
    lua_pushboolean(L, buttons[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

const luaL_Reg lovrJoystick[] = {
  { "isGamepad", lovrJoystickIsGamepad },
  { "isTracked", lovrJoystickIsTracked },
  { "getRawAxes", lovrJoystickGetRawAxes },
  { "getRawButtons", lovrJoystickGetRawButtons },
  { NULL, NULL }
};
