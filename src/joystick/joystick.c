#include "joystick.h"
#include "../glfw.h"
#include "../osvr.h"

extern OSVR_ClientContext ctx;

void lovrJoystickDestroy(Joystick* joystick) {
  if (joystick->isTracked) {
    int i;
    OSVR_ClientInterface* interface;

    vec_foreach(&joystick->osvrButtonInterfaces, interface, i) {
      osvrClientFreeInterface(ctx, *interface);
      free(joystick->osvrButtonInterfaces.data[i]);
    }

    vec_foreach(&joystick->osvrAxisInterfaces, interface, i) {
      osvrClientFreeInterface(ctx, *interface);
      free(joystick->osvrAxisInterfaces.data[i]);
    }
  }

  free(joystick);
}

void lovrJoystickGetAngularAcceleration(Joystick* joystick, float* w, float* x, float* y, float* z) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_AngularAccelerationState state;
    osvrGetAngularAccelerationState(joystick->osvrTrackerInterface,  &timestamp, &state);
    *w = osvrQuatGetW(&state.incrementalRotation);
    *x = osvrQuatGetX(&state.incrementalRotation);
    *y = osvrQuatGetY(&state.incrementalRotation);
    *z = osvrQuatGetZ(&state.incrementalRotation);
  } else {
    *w = *x = *y = *z = 0.f;
  }
}

void lovrJoystickGetAngularVelocity(Joystick* joystick, float* w, float* x, float* y, float* z) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_AngularVelocityState state;
    osvrGetAngularVelocityState(joystick->osvrTrackerInterface,  &timestamp, &state);
    *w = osvrQuatGetW(&state.incrementalRotation);
    *x = osvrQuatGetX(&state.incrementalRotation);
    *y = osvrQuatGetY(&state.incrementalRotation);
    *z = osvrQuatGetZ(&state.incrementalRotation);
  } else {
    *w = *x = *y = *z = 0.f;
  }
}

void lovrJoystickGetAxes(Joystick* joystick, vec_float_t* result) {
  if (joystick->isTracked) {
    interface_vec_t axes = joystick->osvrAxisInterfaces;
    for (int i = 0; i < axes.length; i++) {
      OSVR_TimeValue timestamp;
      OSVR_AnalogState state;
      osvrGetAnalogState(*axes.data[i], &timestamp, &state);
      vec_push(result, state);
    }
  } else {
    int count;
    const float* axes = glfwGetJoystickAxes(joystick->glfwIndex, &count);
    vec_pusharr(result, axes, count);
  }
}

float lovrJoystickGetAxis(Joystick* joystick, int index) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_AnalogState state;
    OSVR_ClientInterface interface = *joystick->osvrAxisInterfaces.data[index];
    osvrGetAnalogState(interface, &timestamp, &state);
    return (float)state;
  } else {
    const float* axes = glfwGetJoystickAxes(joystick->glfwIndex, NULL);
    return axes[index];
  }
}

int lovrJoystickGetAxisCount(Joystick* joystick) {
  if (joystick->isTracked) {
    return joystick->osvrAxisInterfaces.length;
  } else {
    int count;
    glfwGetJoystickAxes(joystick->glfwIndex, &count);
    return count;
  }
}

int lovrJoystickGetButtonCount(Joystick* joystick) {
  if (joystick->isTracked) {
    return joystick->osvrButtonInterfaces.length;
  } else {
    int count;
    glfwGetJoystickButtons(joystick->glfwIndex, &count);
    return count;
  }
}

void lovrJoystickGetLinearAcceleration(Joystick* joystick, float* x, float* y, float* z) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_LinearAccelerationState state;
    osvrGetLinearAccelerationState(joystick->osvrTrackerInterface,  &timestamp, &state);
    *x = osvrVec3GetX(&state);
    *y = osvrVec3GetY(&state);
    *z = osvrVec3GetZ(&state);
  } else {
    *x = *y = *z = 0.f;
  }
}

void lovrJoystickGetLinearVelocity(Joystick* joystick, float* x, float* y, float* z) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_LinearVelocityState state;
    osvrGetLinearVelocityState(joystick->osvrTrackerInterface,  &timestamp, &state);
    *x = osvrVec3GetX(&state);
    *y = osvrVec3GetY(&state);
    *z = osvrVec3GetZ(&state);
  } else {
    *x = *y = *z = 0.f;
  }
}

const char* lovrJoystickGetName(Joystick* joystick) {
  if (joystick->isTracked) {
    return "Tracked controller";
  } else {
    return glfwGetJoystickName(joystick->glfwIndex);
  }
}

void lovrJoystickGetOrientation(Joystick* joystick, float* w, float* x, float* y, float* z) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_OrientationState state;
    osvrGetOrientationState(joystick->osvrTrackerInterface,  &timestamp, &state);
    *w = osvrQuatGetW(&state);
    *x = osvrQuatGetX(&state);
    *y = osvrQuatGetY(&state);
    *z = osvrQuatGetZ(&state);
  } else {
    *w = *x = *y = *z = 0.f;
  }
}

void lovrJoystickGetPosition(Joystick* joystick, float* x, float* y, float* z) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_PositionState state;
    osvrGetPositionState(joystick->osvrTrackerInterface,  &timestamp, &state);
    *x = osvrVec3GetX(&state);
    *y = osvrVec3GetY(&state);
    *z = osvrVec3GetZ(&state);
  } else {
    *x = *y = *z = 0.f;
  }
}

int lovrJoystickIsDown(Joystick* joystick, int index) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_ButtonState state;
    osvrGetButtonState(*joystick->osvrButtonInterfaces.data[index], &timestamp, &state);
    return state > 0;
  } else {
    const unsigned char* buttons = glfwGetJoystickButtons(joystick->glfwIndex, NULL);
    return buttons[index];
  }
}

int lovrJoystickIsTracked(Joystick* joystick) {
  return joystick->isTracked;
}
