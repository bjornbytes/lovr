#include "joystick.h"
#include "../glfw.h"
#include "../osvr.h"

extern OSVR_ClientContext ctx;

void luax_pushjoystick(lua_State* L, Joystick* joystick) {
  Joystick** userdata = (Joystick**) lua_newuserdata(L, sizeof(Joystick*));
  luaL_getmetatable(L, "Joystick");
  lua_setmetatable(L, -2);
  *userdata = joystick;
}

Joystick* luax_checkjoystick(lua_State* L, int index) {
  return *(Joystick**) luaL_checkudata(L, index, "Joystick");
}

static unsigned char lovrJoystickGetButtonState(Joystick* joystick, int buttonIndex) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_ButtonState state;
    osvrGetButtonState(*joystick->osvrButtonInterfaces[buttonIndex], &timestamp, &state);
    return state > 0;
  } else {
    int buttonCount;
    const unsigned char* buttons = glfwGetJoystickButtons(joystick->glfwIndex, &buttonCount);
    return buttons[buttonIndex];
  }

  return 0;
}

static float lovrJoystickGetAxisState(Joystick* joystick, int axisIndex) {
  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_AnalogState state;
    osvrGetAnalogState(*joystick->osvrAxisInterfaces[axisIndex], &timestamp, &state);
    return (float)state;
  } else {
    int axisCount;
    const float* axes = glfwGetJoystickAxes(joystick->glfwIndex, &axisCount);
    return axes[axisIndex];
  }
}

void lovrJoystickDestroy(Joystick* joystick) {
  if (joystick->isTracked) {
    osvrClientFreeInterface(ctx, *joystick->osvrTrackerInterface);
    int i;

    for (i = 0; i < sizeof(joystick->osvrButtonInterfaces); i++) {
      osvrClientFreeInterface(ctx, *joystick->osvrButtonInterfaces[i]);
    }

    for (i = 0; i < sizeof(joystick->osvrAxisInterfaces); i++) {
      osvrClientFreeInterface(ctx, *joystick->osvrAxisInterfaces[i]);
    }
  }
}

int lovrJoystickGetAngularAcceleration(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_AngularAccelerationState state;
    osvrGetAngularAccelerationState(*joystick->osvrTrackerInterface,  &timestamp, &state);
    lua_pushnumber(L, osvrQuatGetW(&state.incrementalRotation));
    lua_pushnumber(L, osvrQuatGetX(&state.incrementalRotation));
    lua_pushnumber(L, osvrQuatGetY(&state.incrementalRotation));
    lua_pushnumber(L, osvrQuatGetZ(&state.incrementalRotation));
    lua_pushnumber(L, state.dt);
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
  }

  return 5;
}

int lovrJoystickGetAngularVelocity(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_AngularVelocityState state;
    osvrGetAngularVelocityState(*joystick->osvrTrackerInterface,  &timestamp, &state);
    lua_pushnumber(L, osvrQuatGetW(&state.incrementalRotation));
    lua_pushnumber(L, osvrQuatGetX(&state.incrementalRotation));
    lua_pushnumber(L, osvrQuatGetY(&state.incrementalRotation));
    lua_pushnumber(L, osvrQuatGetZ(&state.incrementalRotation));
    lua_pushnumber(L, state.dt);
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
  }

  return 5;
}

int lovrJoystickGetAxes(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int axisCount = 0;

  if (joystick->isTracked) {
    OSVR_ClientInterface** axes = joystick->osvrAxisInterfaces;
    for (; axisCount < sizeof(axes) && axes[axisCount] != NULL; axisCount++) {
      lua_pushnumber(L, lovrJoystickGetAxisState(joystick, axisCount));
    }
  } else {
    const float* axes = glfwGetJoystickAxes(joystick->glfwIndex, &axisCount);
    for (int i = 0; i < axisCount; i++) {
      lua_pushnumber(L, axes[i]);
    }
  }

  return axisCount;
}

int lovrJoystickGetAxis(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int axisIndex = luaL_checkint(L, 2);
  lua_pushnumber(L, lovrJoystickGetAxisState(joystick, axisIndex));
  return 1;
}

int lovrJoystickGetAxisCount(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int axisCount = 0;

  if (joystick->isTracked) {
    OSVR_ClientInterface** axes = joystick->osvrAxisInterfaces;
    for (; axisCount < sizeof(axes) && axes[axisCount] != NULL; axisCount++);
  } else {
    glfwGetJoystickAxes(joystick->glfwIndex, &axisCount);
  }

  lua_pushinteger(L, axisCount);

  return 1;
}

int lovrJoystickGetButtonCount(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int buttonCount = 0;

  if (joystick->isTracked) {
    OSVR_ClientInterface** buttons = joystick->osvrButtonInterfaces;
    for (; buttonCount < sizeof(buttons) && buttons[buttonCount] != NULL; buttonCount++);
  } else {
    glfwGetJoystickButtons(joystick->glfwIndex, &buttonCount);
  }

  lua_pushinteger(L, buttonCount);

  return 1;
}

int lovrJoystickGetLinearAcceleration(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_LinearAccelerationState state;
    osvrGetLinearAccelerationState(*joystick->osvrTrackerInterface,  &timestamp, &state);
    lua_pushnumber(L, osvrVec3GetX(&state));
    lua_pushnumber(L, osvrVec3GetY(&state));
    lua_pushnumber(L, osvrVec3GetZ(&state));
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
  }

  return 3;
}

int lovrJoystickGetLinearVelocity(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_LinearVelocityState state;
    osvrGetLinearVelocityState(*joystick->osvrTrackerInterface,  &timestamp, &state);
    lua_pushnumber(L, osvrVec3GetX(&state));
    lua_pushnumber(L, osvrVec3GetY(&state));
    lua_pushnumber(L, osvrVec3GetZ(&state));
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
  }

  return 3;
}

int lovrJoystickGetName(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    lua_pushstring(L, "Tracked controller");
  } else {
    lua_pushstring(L, glfwGetJoystickName(joystick->glfwIndex));
  }

  return 1;
}

int lovrJoystickGetOrientation(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_OrientationState state;
    osvrGetOrientationState(*joystick->osvrTrackerInterface,  &timestamp, &state);
    lua_pushnumber(L, osvrQuatGetW(&state));
    lua_pushnumber(L, osvrQuatGetX(&state));
    lua_pushnumber(L, osvrQuatGetY(&state));
    lua_pushnumber(L, osvrQuatGetZ(&state));
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
  }

  return 4;
}

int lovrJoystickGetPosition(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->isTracked) {
    OSVR_TimeValue timestamp;
    OSVR_PositionState state;
    osvrGetPositionState(*joystick->osvrTrackerInterface,  &timestamp, &state);
    lua_pushnumber(L, osvrVec3GetX(&state));
    lua_pushnumber(L, osvrVec3GetY(&state));
    lua_pushnumber(L, osvrVec3GetZ(&state));
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
  }

  return 3;
}

int lovrJoystickIsDown(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int buttonIndex = luaL_checkint(L, 2);
  lua_pushboolean(L, lovrJoystickGetButtonState(joystick, buttonIndex));
  return 1;
}

int lovrJoystickIsTracked(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushboolean(L, joystick->isTracked);
  return 1;
}

const luaL_Reg lovrJoystick[] = {
  { "getAngularAcceleration", lovrJoystickGetAngularAcceleration },
  { "getAngularVelocity", lovrJoystickGetAngularVelocity },
  { "getAxes", lovrJoystickGetAxes },
  { "getAxis", lovrJoystickGetAxis },
  { "getAxisCount", lovrJoystickGetAxisCount },
  { "getButtonCount", lovrJoystickGetButtonCount },
  { "getLinearAcceleration", lovrJoystickGetLinearAcceleration },
  { "getLinearVelocity", lovrJoystickGetLinearVelocity },
  { "getName", lovrJoystickGetName },
  { "getOrientation", lovrJoystickGetOrientation },
  { "getPosition", lovrJoystickGetPosition },
  { "isDown", lovrJoystickIsDown },
  { "isTracked", lovrJoystickIsTracked },
  { NULL, NULL }
};
