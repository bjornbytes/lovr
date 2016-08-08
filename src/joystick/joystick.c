#include "joystick.h"
#include "../glfw.h"
#include "../osvr.h"
#include "../vendor/map/map.h"

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

void lovrJoystickDestroy(Joystick* joystick) {
  if (joystick->type == JOYSTICK_TYPE_OSVR) {
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

int lovrJoystickMapAxis(Joystick* joystick, const char* key) {
  int* axisIndex = NULL;

  if ((axisIndex = map_get(&joystick->axisMapping, key)) != NULL) {
    return *axisIndex;
  }

  return -1;
}

int lovrJoystickMapButton(Joystick* joystick, const char* key) {
  int* buttonIndex = NULL;

  if ((buttonIndex = map_get(&joystick->buttonMapping, key)) != NULL) {
    return *buttonIndex;
  }

  return -1;
}

unsigned char lovrJoystickGetButtonState(Joystick* joystick, int buttonIndex) {
  if (joystick->type == JOYSTICK_TYPE_OSVR) {
    OSVR_TimeValue timestamp;
    OSVR_ButtonState state;
    osvrGetButtonState(*joystick->osvrButtonInterfaces[buttonIndex], &timestamp, &state);
    return state > 0;
  } else if (joystick->type == JOYSTICK_TYPE_GLFW) {
    int buttonCount;
    const unsigned char* buttons = glfwGetJoystickButtons(joystick->glfwIndex, &buttonCount);
    return buttons[buttonIndex];
  }

  return 0;
}

float lovrJoystickGetAxisState(Joystick* joystick, int axisIndex) {
  if (joystick->type == JOYSTICK_TYPE_OSVR) {
    OSVR_TimeValue timestamp;
    OSVR_AnalogState state;
    osvrGetAnalogState(*joystick->osvrAxisInterfaces[axisIndex], &timestamp, &state);
    return (float)state;
  } else if (joystick->type == JOYSTICK_TYPE_GLFW) {
    int axisCount;
    const float* axes = glfwGetJoystickAxes(joystick->glfwIndex, &axisCount);
    return axes[axisIndex];
  }

  return 0;
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

int lovrJoystickIsDown(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  const char* buttonName = luaL_checkstring(L, 2);

  int buttonIndex = lovrJoystickMapButton(joystick, buttonName);

  if (buttonIndex == -1) {
    return luaL_error(L, "Unknown button '%s'\n", buttonName);
  }

  lua_pushboolean(L, lovrJoystickGetButtonState(joystick, buttonIndex));

  return 1;
}

int lovrJoystickGetAxis(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  const char* axisName = luaL_checkstring(L, 2);

  int axisIndex = lovrJoystickMapAxis(joystick, axisName);

  if (axisIndex == -1) {
    return luaL_error(L, "Unknown axis '%s'\n", axisName);
  }

  lua_pushnumber(L, lovrJoystickGetAxisState(joystick, axisIndex));

  return 1;
}

int lovrJoystickGetPosition(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->type != JOYSTICK_TYPE_OSVR) {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 3;
  }

  OSVR_TimeValue timestamp;
  OSVR_PositionState state;
  osvrGetPositionState(*joystick->osvrTrackerInterface,  &timestamp, &state);
  lua_pushnumber(L, osvrVec3GetX(&state));
  lua_pushnumber(L, osvrVec3GetY(&state));
  lua_pushnumber(L, osvrVec3GetZ(&state));
  return 3;
}

int lovrJoystickGetOrientation(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->type != JOYSTICK_TYPE_OSVR) {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 4;
  }

  OSVR_TimeValue timestamp;
  OSVR_OrientationState state;
  osvrGetOrientationState(*joystick->osvrTrackerInterface,  &timestamp, &state);
  lua_pushnumber(L, osvrQuatGetW(&state));
  lua_pushnumber(L, osvrQuatGetX(&state));
  lua_pushnumber(L, osvrQuatGetY(&state));
  lua_pushnumber(L, osvrQuatGetZ(&state));
  return 4;
}

int lovrJoystickGetLinearVelocity(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->type != JOYSTICK_TYPE_OSVR) {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 3;
  }

  OSVR_TimeValue timestamp;
  OSVR_LinearVelocityState state;
  osvrGetLinearVelocityState(*joystick->osvrTrackerInterface,  &timestamp, &state);
  lua_pushnumber(L, osvrVec3GetX(&state));
  lua_pushnumber(L, osvrVec3GetY(&state));
  lua_pushnumber(L, osvrVec3GetZ(&state));
  return 3;
}

int lovrJoystickGetAngularVelocity(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->type != JOYSTICK_TYPE_OSVR) {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 5;
  }

  OSVR_TimeValue timestamp;
  OSVR_AngularVelocityState state;
  osvrGetAngularVelocityState(*joystick->osvrTrackerInterface,  &timestamp, &state);
  lua_pushnumber(L, osvrQuatGetW(&state.incrementalRotation));
  lua_pushnumber(L, osvrQuatGetX(&state.incrementalRotation));
  lua_pushnumber(L, osvrQuatGetY(&state.incrementalRotation));
  lua_pushnumber(L, osvrQuatGetZ(&state.incrementalRotation));
  lua_pushnumber(L, state.dt);
  return 5;
}

int lovrJoystickGetLinearAcceleration(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->type != JOYSTICK_TYPE_OSVR) {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 3;
  }

  OSVR_TimeValue timestamp;
  OSVR_LinearAccelerationState state;
  osvrGetLinearAccelerationState(*joystick->osvrTrackerInterface,  &timestamp, &state);
  lua_pushnumber(L, osvrVec3GetX(&state));
  lua_pushnumber(L, osvrVec3GetY(&state));
  lua_pushnumber(L, osvrVec3GetZ(&state));
  return 3;
}

int lovrJoystickGetAngularAcceleration(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  if (joystick->type != JOYSTICK_TYPE_OSVR) {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 5;
  }

  OSVR_TimeValue timestamp;
  OSVR_AngularAccelerationState state;
  osvrGetAngularAccelerationState(*joystick->osvrTrackerInterface,  &timestamp, &state);
  lua_pushnumber(L, osvrQuatGetW(&state.incrementalRotation));
  lua_pushnumber(L, osvrQuatGetX(&state.incrementalRotation));
  lua_pushnumber(L, osvrQuatGetY(&state.incrementalRotation));
  lua_pushnumber(L, osvrQuatGetZ(&state.incrementalRotation));
  lua_pushnumber(L, state.dt);
  return 5;
}

int lovrJoystickGetMapping(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  const char* key;
  map_iter_t iterator;

  lua_newtable(L);

  lua_newtable(L);
  iterator = map_iter(&joystick->axisMapping);
  while ((key = map_next(&joystick->axisMapping, &iterator)) != NULL) {
    lua_pushstring(L, key);
    lua_pushnumber(L, *map_get(&joystick->axisMapping, key));
    lua_settable(L, -3);
  }
  lua_setfield(L, -2, "axis");

  lua_newtable(L);
  iterator = map_iter(&joystick->buttonMapping);
  while ((key = map_next(&joystick->buttonMapping, &iterator)) != NULL) {
    lua_pushstring(L, key);
    lua_pushnumber(L, *map_get(&joystick->buttonMapping, key));
    lua_settable(L, -3);
  }
  lua_setfield(L, -2, "button");

  return 1;
}

int lovrJoystickSetMapping(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);

  // make a map

  lua_pushnil(L);
  while (lua_next(L, 2) != 0) {
    const char* key = luaL_checkstring(L, -2);
    int index = luaL_checkint(L, -1);

    // map stuff

    lua_pop(L, 1);
  }

  return 0;
}

int lovrJoystickGetRawAxes(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);

  lua_newtable(L);

  if (joystick->type == JOYSTICK_TYPE_OSVR) {
    for (int i = 0; i < sizeof(joystick->osvrAxisInterfaces); i++) {
      lua_pushnumber(L, lovrJoystickGetAxisState(joystick, i));
      lua_rawseti(L, -2, i + 1);
    }

    return 1;
  }

  int axisCount;
  const float* axes = glfwGetJoystickAxes(joystick->glfwIndex, &axisCount);

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
    for (int i = 0; i < sizeof(joystick->osvrButtonInterfaces); i++) {
      lua_pushboolean(L, lovrJoystickGetButtonState(joystick, i));
      lua_rawseti(L, -2, i + 1);
    }

    return 1;
  }

  int buttonCount;
  const unsigned char* buttons = glfwGetJoystickButtons(joystick->glfwIndex, &buttonCount);

  for (int i = 0; i < buttonCount; i++) {
    lua_pushboolean(L, buttons[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

const luaL_Reg lovrJoystick[] = {
  { "isGamepad", lovrJoystickIsGamepad },
  { "isTracked", lovrJoystickIsTracked },
  { "isDown", lovrJoystickIsDown },
  { "getAxis", lovrJoystickGetAxis },
  { "getPosition", lovrJoystickGetPosition },
  { "getOrientation", lovrJoystickGetOrientation },
  { "getLinearVelocity", lovrJoystickGetLinearVelocity },
  { "getAngularVelocity", lovrJoystickGetAngularVelocity },
  { "getLinearAcceleration", lovrJoystickGetLinearAcceleration },
  { "getAngularAcceleration", lovrJoystickGetAngularAcceleration },
  { "getMapping", lovrJoystickGetMapping },
  { "setMapping", lovrJoystickSetMapping },
  { "getRawAxes", lovrJoystickGetRawAxes },
  { "getRawButtons", lovrJoystickGetRawButtons },
  { NULL, NULL }
};
