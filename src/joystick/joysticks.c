#include "joysticks.h"
#include "../osvr.h"
#include "../lovr.h"
#include "../vendor/vec/vec.h"
#include <stdlib.h>

typedef struct {
  joystick_vec_t list;
} JoystickState;

static JoystickState joystickState;

void lovrJoysticksInit() {
  glfwSetJoystickCallback(lovrJoysticksOnJoystickChanged);

  vec_init(&joystickState.list);

  // GLFW joysticks
  for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
    if (glfwJoystickPresent(i)) {
      Joystick* joystick = malloc(sizeof(Joystick));
      joystick->isTracked = 0;
      joystick->glfwIndex = i;
      joystick->osvrTrackerInterface = NULL;
      vec_push(&joystickState.list, joystick);
    }
  }

  // OSVR joysticks
  if (osvrClientCheckStatus(ctx) != OSVR_RETURN_FAILURE) {
    char buffer[128];
    const char* hands[2] = {
      "/me/hands/left",
      "/me/hands/right"
    };

    Joystick* joystick = NULL;

    for (int i = 0; i < 2; i++) {
      if (joystick == NULL) {
        joystick = malloc(sizeof(Joystick));
      }

      osvrClientGetInterface(ctx, hands[i], &joystick->osvrTrackerInterface);

      if (joystick->osvrTrackerInterface) {
        joystick->isTracked = 1;
        joystick->glfwIndex = -1;
        vec_init(&joystick->osvrAxisInterfaces);
        vec_init(&joystick->osvrButtonInterfaces);

        // Buttons
        const char* buttons[6] = {
          "system",
          "menu",
          "grip",
          "trackpad/touch",
          "trackpad/button",
          "trigger/button"
        };

        OSVR_ClientInterface* buttonInterface = NULL;

        for (int j = 0; i < 6; j++) {
          if (buttonInterface == NULL) {
            buttonInterface = malloc(sizeof(buttonInterface));
          }

          snprintf(buffer, sizeof(buffer), "%s/%s", hands[i], buttons[j]);
          osvrClientGetInterface(ctx, buffer, buttonInterface);

          if (buttonInterface) {
            vec_push(&joystick->osvrButtonInterfaces, buttonInterface);
            buttonInterface = NULL;
          }
        }

        free(buttonInterface);

        // Axes
        const char* axes[3] = {
          "trackpad/x",
          "trackpad/y",
          "trigger"
        };

        OSVR_ClientInterface* axisInterface = NULL;

        for (int j = 0; j < 3; j++) {
          if (axisInterface == NULL) {
            axisInterface = malloc(sizeof(axisInterface));
          }

          snprintf(buffer, sizeof(buffer), "%s/%s", hands[i], axes[j]);
          osvrClientGetInterface(ctx, buffer, axisInterface);

          if (axisInterface) {
            vec_push(&joystick->osvrAxisInterfaces, axisInterface);
            axisInterface = NULL;
          }
        }

        free(axisInterface);
      }
    }

    free(joystick);
  }
}

void lovrJoysticksOnJoystickChanged(int index, int event) {
  if (event == GLFW_CONNECTED) {
    Joystick* joystick = malloc(sizeof(Joystick));
    joystick->isTracked = 0;
    joystick->glfwIndex = index;
    joystick->osvrTrackerInterface = NULL;
    vec_push(&joystickState.list, joystick);
    lovrOnJoystickAdded(joystick);
  } else if (event == GLFW_DISCONNECTED) {
    Joystick* joystick;
    int i;
    vec_foreach(&joystickState.list, joystick, i) {
      if (joystick->glfwIndex == index) {
        lovrOnJoystickRemoved(joystick);
        lovrJoystickDestroy(joystick);
        vec_splice(&joystickState.list, i, 1);
        return;
      }
    }
  }
}

int lovrJoysticksGetJoystickCount() {
  return joystickState.list.length;
}

void lovrJoysticksGetJoysticks(joystick_vec_t* joysticks) {
  vec_pusharr(joysticks, joystickState.list.data, joystickState.list.length);
}
