#include "headset/headset.h"
#include "core/os.h"
#include "core/util.h"
#include "core/maf.h"
#include <stdlib.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GAMEPAD_COUNT (DEVICE_GAMEPAD_LAST - DEVICE_GAMEPAD_FIRST + 1)

typedef struct {
  bool present;
  int jid;
} GamepadState;

static struct {
  bool inited;
  GamepadState gamepad[GAMEPAD_COUNT];
  int gamepadsPresent;
} state;

static GamepadState *getGamepad(Device device) {
  if (device >= DEVICE_GAMEPAD_FIRST && device <= DEVICE_GAMEPAD_LAST) {
    return &state.gamepad[device - DEVICE_GAMEPAD_FIRST];
  }
  return NULL;
}

void discoverGamepads() {
  for(int jid = 0; jid < GLFW_JOYSTICK_LAST; jid++) {
    if (glfwJoystickPresent(jid)) {
      bool alreadyTracked = false;
      for(int gamepadIdx = 0; gamepadIdx < GAMEPAD_COUNT; gamepadIdx++) {
        GamepadState *gamepad = &state.gamepad[gamepadIdx];
        if (gamepad->present && gamepad->jid == jid) {
          alreadyTracked = true;
          break;
        }
      }
      if (!alreadyTracked) {
        for(int gamepadIdx = 0; gamepadIdx < GAMEPAD_COUNT; gamepadIdx++) {
          GamepadState *gamepad = &state.gamepad[gamepadIdx];
          if (!gamepad->present) {
            gamepad->present = true;
            gamepad->jid = jid;
            state.gamepadsPresent++;
            printf("FOUND NEW GAMEPAD %d: %s\n", gamepadIdx, glfwGetJoystickName(jid));
            break;
          }
        }
      }
    }
  }
}

static void refreshGamepad(int jid, int event) {
  if (event == GLFW_CONNECTED)
  {
    for(int idx = 0; idx < GAMEPAD_COUNT; idx++) {
      GamepadState *gamepad = &state.gamepad[idx];
      if (!gamepad->present) {
        gamepad->present = true;
        gamepad->jid = jid;
        state.gamepadsPresent++;
        printf("CONNECTED NEW GAMEPAD %d: %s\n", idx, glfwGetJoystickName(jid));
      }
    }
  }
  else if (event == GLFW_DISCONNECTED)
  {
    for(int idx = 0; idx < GAMEPAD_COUNT; idx++) {
      GamepadState *gamepad = &state.gamepad[idx];
      if (gamepad->present && gamepad->jid == jid) {
        bool needRebuild = state.gamepadsPresent == GAMEPAD_COUNT;
        gamepad->present = false;
        state.gamepadsPresent--;
        printf("DISCONNECTED GAMEPAD %d\n", idx);
        if (needRebuild)
          discoverGamepads();
        break;
      }
    }
  }
}

static void attempt_init() {
  if (!state.inited && lovrPlatformHasWindow()) {
    glfwSetJoystickCallback(refreshGamepad);
    discoverGamepads();
    state.inited = true;
  }
}

static bool gamepad_init(float offset, uint32_t msaa) {
  attempt_init(); // This is currently assumed to fail
  return true;
}

static void gamepad_destroy(void) {
}

static bool gamepad_getPose(Device device, vec3 position, quat orientation) {
  attempt_init();
  GamepadState *gamepad = getGamepad(device);

  if (!gamepad)
    return false;
  vec3_set(position, 0, 0, 0);
  quat_set(orientation, 0, 0, 0,0);
  return true;
}

static bool gamepad_getVelocity(Device device, vec3 velocity, vec3 angularVelocity) {
  return false;
}

static bool gamepad_isDown(Device device, DeviceButton button, bool* down) {
  attempt_init();
  GamepadState *gamepad = getGamepad(device);
  if (!gamepad)
    return false;

  GLFWgamepadstate gamepadState;

  if (!glfwGetGamepadState(gamepad->jid, &gamepadState))
    return false;

  switch (button) {
    case BUTTON_TRIGGER: *down = MAX(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]) > 0.5f; return true;
    case BUTTON_GRIP: *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]||gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]; return true;
    case BUTTON_THUMBSTICK: *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]; return true;
    case BUTTON_MENU:  *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_START]; return true;
    case BUTTON_A: *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A]; return true;
    case BUTTON_B: *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B]; return true;
    case BUTTON_X: *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X]; return true;
    case BUTTON_Y: *down = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y]; return true;
    default: return false;
  }
}

static bool gamepad_isTouched(Device device, DeviceButton button, bool* touched) {
  return false;
}

static bool gamepad_getAxis(Device device, DeviceAxis axis, float* value) {
  attempt_init();
  GamepadState *gamepad = getGamepad(device);
  if (!gamepad)
    return false;
  
  GLFWgamepadstate gamepadState;

  if (!glfwGetGamepadState(gamepad->jid, &gamepadState))
    return false;

  switch (axis) {
    case AXIS_TRIGGER: *value = MAX(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]); return true;
    case AXIS_GRIP: *value = (gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]||gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER])?1.0f:0.0f; return true;
    case AXIS_THUMBSTICK:
      value[0] = gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
      value[1] = gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
      return true;
    default: return false;
  }
}

static bool gamepad_vibrate(Device device, float strength, float duration, float frequency) {
  return false;
}

static struct ModelData* gamepad_newModelData(Device device) {
  return NULL;
}

static void gamepad_update(float dt) {
}

HeadsetInterface lovrHeadsetGamepadDriver = {
  .driverType = DRIVER_GAMEPAD,
  .init = gamepad_init,
  .destroy = gamepad_destroy,
  .getPose = gamepad_getPose,
  .getVelocity = gamepad_getVelocity,
  .isDown = gamepad_isDown,
  .isTouched = gamepad_isTouched,
  .getAxis = gamepad_getAxis,
  .vibrate = gamepad_vibrate,
  .newModelData = gamepad_newModelData,
  .update = gamepad_update
};
