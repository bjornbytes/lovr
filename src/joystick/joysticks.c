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

static JoystickState joystickState;

int lovrJoysticksGetJoystickCount(lua_State* L) {
  lua_pushnumber(L, joystickState.count);
  return 1;
}

int lovrJoysticksGetJoysticks(lua_State* L) {
  lua_newtable(L);

  for (int i = 0; joystickState.list[i] != NULL && i < sizeof(joystickState.list); i++) {
    luax_pushjoystick(L, joystickState.list[i]);
    lua_rawseti(L, -2, i + 1);
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

  int i, j;

  for (i = 0; i < 32; i++) {
    if (joystickState.list[i] != NULL) {
      lovrJoystickDestroy(joystickState.list[i]);
      free(joystickState.list[i]);
    }

    joystickState.list[i] = NULL;
  }

  int count = 0;

  for (i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
    if (glfwJoystickPresent(i)) {
      Joystick* joystick = malloc(sizeof(Joystick));
      joystick->isTracked = 0;
      joystick->glfwIndex = i;

      joystickState.list[count++] = joystick;
    }
  }

  if (osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    char buffer[128];
    const char* hands[2] = {
      "/me/hands/left",
      "/me/hands/right"
    };

    for (i = 0; i < sizeof(hands); i++) {
      OSVR_ClientInterface* trackerInterface = malloc(sizeof(OSVR_ClientInterface));
      osvrClientGetInterface(ctx, hands[i], trackerInterface);

      if (trackerInterface != NULL) {
        Joystick* joystick = malloc(sizeof(Joystick));
        joystick->isTracked = 1;
        joystick->glfwIndex = -1;
        joystick->osvrTrackerInterface = trackerInterface;

        int buttonCount = 0;
        const char* buttons[6] = {
          "system",
          "menu",
          "grip",
          "trackpad/touch",
          "trackpad/button",
          "trigger/button"
        };

        for (j = 0; i < sizeof(buttons); j++) {
          snprintf(buffer, sizeof(buffer), "%s/%s", hands[i], buttons[j]);
          OSVR_ClientInterface* buttonInterface = malloc(sizeof(OSVR_ClientInterface));

          if (buttonInterface != NULL) {
            joystick->osvrButtonInterfaces[buttonCount++] = buttonInterface;
          }
        }

        int axisCount = 0;
        const char* axes[3] = {
          "trackpad/x",
          "trackpad/y",
          "trigger"
        };

        for (j = 0; j < sizeof(axes); j++) {
          snprintf(buffer, sizeof(buffer), "%s/%s", hands[i], axes[j]);
          OSVR_ClientInterface* axisInterface = malloc(sizeof(OSVR_ClientInterface));

          if (axisInterface != NULL) {
            joystick->osvrAxisInterfaces[axisCount++] = axisInterface;
          }
        }

        joystickState.list[count++] = joystick;
      }
    }
  }

  joystickState.count = count;

  return 1;
}
