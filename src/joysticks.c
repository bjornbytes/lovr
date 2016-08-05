#include "joysticks.h"
#include "glfw.h"
#include "joystick.h"
#include "util.h"
#include <stdlib.h>

typedef struct {
  Joystick* list[32];
  int count;
} JoystickState;

JoystickState joystickState;

void lovrJoysticksRefresh() {
  for (int i = 0; i < 32; i++) {
    joystickState.list[i] = NULL;
  }

  int count = 0;
  for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
    if (glfwJoystickPresent(i)) {
      int index = count++;
      joystickState.list[index] = malloc(sizeof(Joystick));
      joystickState.list[index]->index = i;
    }
  }

  joystickState.count = count;
}

int lovrJoysticksGetJoystickCount(lua_State* L) {
  lua_pushnumber(L, joystickState.count);

  return 1;
}

int lovrJoysticksGetJoysticks(lua_State* L) {
  lua_newtable(L);

  int i = 0;

  while (joystickState.list[i] != NULL) {
    luax_pushjoystick(L, joystickState.list[i]);
    lua_rawseti(L, -2, ++i);
  }

  return 1;
}

const luaL_Reg lovrJoysticks[] = {
  { "getJoystickCount", lovrJoysticksGetJoystickCount },
  { "getJoysticks", lovrJoysticksGetJoysticks },
  { NULL, NULL }
};

int lovrInitJoysticks(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrJoysticks);
  luaRegisterType(L, "Joystick", lovrJoystick);
  lovrJoysticksRefresh();
  return 1;
}
