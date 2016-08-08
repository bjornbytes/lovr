#include "joysticks.h"
#include "joystick.h"
#include "../glfw.h"
#include "../util.h"
#include "../osvr.h"
#include <stdlib.h>

typedef struct {
  Joystick* list[32];
  int count;
} JoystickState;

JoystickState joystickState;

void lovrJoysticksRefresh() {
  for (int i = 0; i < 32; i++) {
    if (joystickState.list[i] != NULL) {
      free(joystickState.list[i]);
    }

    joystickState.list[i] = NULL;
  }

  int count = 0;

  for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
    if (glfwJoystickPresent(i)) {
      Joystick* joystick = malloc(sizeof(Joystick));
      joystick->type = JOYSTICK_TYPE_GLFW;
      joystick->index = i;

      joystickState.list[count++] = joystick;
    }
  }

  // TODO handle OSVR joysticks
  // /me/hands/left
  // /me/hands/right

  if (osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    const char* hands[2] = { "/me/hands/left", "/me/hands/right" };

    for (int i = 0; i < sizeof(hands); i++) {
      OSVR_ClientInterface* interface = (OSVR_ClientInterface*) malloc(sizeof(OSVR_ClientInterface));
      osvrClientGetInterface(ctx, hands[i], interface);

      if (interface != NULL) {
        Joystick* joystick = malloc(sizeof(Joystick));
        joystick->type = JOYSTICK_TYPE_OSVR;
        joystick->index = -1;
        joystick->interface = interface;
      }
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
