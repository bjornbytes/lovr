#include "joystick.h"
#include "../joystick/joystick.h"

void luax_pushjoystick(lua_State* L, Joystick* joystick) {
  Joystick** userdata = (Joystick**) lua_newuserdata(L, sizeof(Joystick*));
  luaL_getmetatable(L, "Joystick");
  lua_setmetatable(L, -2);
  *userdata = joystick;
}

Joystick* luax_checkjoystick(lua_State* L, int index) {
  return *(Joystick**) luaL_checkudata(L, index, "Joystick");
}

int luax_destroyjoystick(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lovrJoystickDestroy(joystick);
  return 0;
}

const luaL_Reg lovrJoystick[] = {
  { "getAngularAcceleration", l_lovrJoystickGetAngularAcceleration },
  { "getAngularVelocity", l_lovrJoystickGetAngularVelocity },
  { "getAxes", l_lovrJoystickGetAxes },
  { "getAxis", l_lovrJoystickGetAxis },
  { "getAxisCount", l_lovrJoystickGetAxisCount },
  { "getButtonCount", l_lovrJoystickGetButtonCount },
  { "getLinearAcceleration", l_lovrJoystickGetLinearAcceleration },
  { "getLinearVelocity", l_lovrJoystickGetLinearVelocity },
  { "getName", l_lovrJoystickGetName },
  { "getOrientation", l_lovrJoystickGetOrientation },
  { "getPosition", l_lovrJoystickGetPosition },
  { "isDown", l_lovrJoystickIsDown },
  { "isTracked", l_lovrJoystickIsTracked },
  { NULL, NULL }
};

int l_lovrJoystickGetAngularAcceleration(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  float w, x, y, z;
  lovrJoystickGetAngularAcceleration(joystick, &w, &x, &y, &z);
  lua_pushnumber(L, w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrJoystickGetAngularVelocity(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  float w, x, y, z;
  lovrJoystickGetAngularVelocity(joystick, &w, &x, &y, &z);
  lua_pushnumber(L, w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrJoystickGetAxes(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_newtable(L);
  vec_float_t axes;
  vec_init(&axes);
  lovrJoystickGetAxes(joystick, &axes);
  float axis;
  int i;
  vec_foreach(&axes, axis, i) {
    lua_pushnumber(L, axis);
    lua_rawseti(L, -2, i + 1);
  }
  vec_deinit(&axes);
  return 1;
}

int l_lovrJoystickGetAxis(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int axis = luaL_checknumber(L, 2);
  lua_pushnumber(L, lovrJoystickGetAxis(joystick, axis));
  return 1;
}

int l_lovrJoystickGetAxisCount(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushnumber(L, lovrJoystickGetAxisCount(joystick));
  return 1;
}

int l_lovrJoystickGetButtonCount(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushnumber(L, lovrJoystickGetButtonCount(joystick));
  return 1;
}

int l_lovrJoystickGetLinearAcceleration(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  float x, y, z;
  lovrJoystickGetLinearAcceleration(joystick, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrJoystickGetLinearVelocity(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  float x, y, z;
  lovrJoystickGetLinearVelocity(joystick, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrJoystickGetName(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushstring(L, lovrJoystickGetName(joystick));
  return 1;
}

int l_lovrJoystickGetOrientation(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  float w, x, y, z;
  lovrJoystickGetOrientation(joystick, &w, &x, &y, &z);
  lua_pushnumber(L, w);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 4;
}

int l_lovrJoystickGetPosition(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  float x, y, z;
  lovrJoystickGetPosition(joystick, &x, &y, &z);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, z);
  return 3;
}

int l_lovrJoystickIsDown(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  int button = luaL_checkint(L, 2);
  lua_pushinteger(L, lovrJoystickIsDown(joystick, button));
  return 1;
}

int l_lovrJoystickIsTracked(lua_State* L) {
  Joystick* joystick = luax_checkjoystick(L, 1);
  lua_pushinteger(L, lovrJoystickIsTracked(joystick));
  return 1;
}
