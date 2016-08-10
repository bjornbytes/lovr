#include "joysticks.h"
#include "joystick.h"
#include "../util.h"
#include "../joystick/joysticks.h"

const luaL_Reg lovrJoysticks[] = {
  { "getJoystickCount", l_lovrJoysticksGetJoystickCount },
  { "getJoysticks", l_lovrJoysticksGetJoysticks },
  { NULL, NULL }
};

int l_lovrJoysticksInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrJoysticks);
  luaRegisterType(L, "Joystick", lovrJoystick, luax_destroyjoystick);
  lovrJoysticksInit();
  return 1;
}

int l_lovrJoysticksGetJoystickCount(lua_State* L) {
  lua_pushnumber(L, lovrJoysticksGetJoystickCount());
  return 1;
}

int l_lovrJoysticksGetJoysticks(lua_State* L) {
  joystick_vec_t joysticks;
  vec_init(&joysticks);
  lovrJoysticksGetJoysticks(&joysticks);
  lua_newtable(L);
  Joystick* joystick;
  int i;
  vec_foreach(&joysticks, joystick, i) {
    luax_pushjoystick(L, joystick);
    lua_rawseti(L, -2, i + 1);
  }
  vec_deinit(&joysticks);
  return 1;
}
