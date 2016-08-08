#include "joysticks.h"
#include "joystick.h"
#include "../glfw.h"
#include "../util.h"
#include "../osvr.h"
#include <stdlib.h>

#define MAX_JOYSTICKS GLFW_JOYSTICK_LAST - GLFW_JOYSTICK_1 + 2

extern lua_State* L;

typedef struct {
  Joystick list[MAX_JOYSTICKS];
} JoystickState;

static JoystickState joystickState;

int lovrJoysticksGetJoystickCount(lua_State* L) {
  int count = 0;

  for (int i = 0; i < MAX_JOYSTICKS; i++) {
    if (lovrJoystickIsConnected(&joystickState.list[i])) {
      count++;
    }
  }

  lua_pushnumber(L, count);

  return 1;
}

int lovrJoysticksGetJoysticks(lua_State* L) {
  lua_newtable(L);
  int index = 1;

  for (int i = 0; i < MAX_JOYSTICKS; i++) {
    if (lovrJoystickIsConnected(&joystickState.list[i])) {
      luax_pushjoystick(L, &joystickState.list[i]);
      lua_rawseti(L, -2, index++);
    }
  }

  return 1;
}

void lovrJoysticksOnJoystickChanged(int joystickIndex, int event) {
  lua_getglobal(L, "lovr");

  if (event == GLFW_CONNECTED) {
    lua_getfield(L, -1, "joystickadded");
  } else if (event == GLFW_DISCONNECTED) {
    lua_getfield(L, -1, "joystickremoved");
  }

  if (lua_isfunction(L, -1)) {
    luax_pushjoystick(L, &joystickState.list[joystickIndex - GLFW_JOYSTICK_1]);
    lua_call(L, 1, 0);
  }
}

const luaL_Reg lovrJoysticks[] = {
  { "getJoystickCount", lovrJoysticksGetJoystickCount },
  { "getJoysticks", lovrJoysticksGetJoysticks },
  { NULL, NULL }
};

int lovrInitJoysticks(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrJoysticks);
  luaRegisterType(L, "Joystick", lovrJoystick, luax_destroyjoystick);
  glfwSetJoystickCallback(lovrJoysticksOnJoystickChanged);

  for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
    Joystick joystick = joystickState.list[i];
    joystick.isTracked = 0;
    joystick.glfwIndex = i;
  }

  if (osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    char buffer[128];
    const char* hands[2] = {
      "/me/hands/left",
      "/me/hands/right"
    };

    for (int i = 0; i < 2; i++) {
      Joystick joystick = joystickState.list[GLFW_JOYSTICK_LAST - GLFW_JOYSTICK_1 + i];
      joystick.isTracked = 1;
      joystick.glfwIndex = -1;
      joystick.osvrTrackerInterface = NULL;

      OSVR_ClientInterface* trackerInterface = malloc(sizeof(OSVR_ClientInterface));
      osvrClientGetInterface(ctx, hands[i], trackerInterface);

      if (trackerInterface != NULL) {
        joystick.osvrTrackerInterface = trackerInterface;

        int buttonCount = 0;
        const char* buttons[6] = {
          "system",
          "menu",
          "grip",
          "trackpad/touch",
          "trackpad/button",
          "trigger/button"
        };

        for (int j = 0; i < 6; j++) {
          snprintf(buffer, sizeof(buffer), "%s/%s", hands[i], buttons[j]);
          OSVR_ClientInterface* buttonInterface = malloc(sizeof(OSVR_ClientInterface));

          if (buttonInterface != NULL) {
            joystick.osvrButtonInterfaces[buttonCount++] = buttonInterface;
          }
        }

        int axisCount = 0;
        const char* axes[3] = {
          "trackpad/x",
          "trackpad/y",
          "trigger"
        };

        for (int j = 0; j < 3; j++) {
          snprintf(buffer, sizeof(buffer), "%s/%s", hands[i], axes[j]);
          OSVR_ClientInterface* axisInterface = malloc(sizeof(OSVR_ClientInterface));

          if (axisInterface != NULL) {
            joystick.osvrAxisInterfaces[axisCount++] = axisInterface;
          }
        }
      }
    }
  }

  return 1;
}
