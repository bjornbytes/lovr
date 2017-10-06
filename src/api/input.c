#include "api/lovr.h"
#include "input/input.h"


map_int_t MouseButtons;


int l_lovrInputInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrInput);

  map_init(&MouseButtons);
  map_set(&MouseButtons, "left", MOUSE_BUTTON_LEFT);
  map_set(&MouseButtons, "right", MOUSE_BUTTON_RIGHT);
  map_set(&MouseButtons, "middle", MOUSE_BUTTON_MIDDLE);

  lovrInputInit();

  return 1;
}

int l_lovrInputGetMousePosition(lua_State* L) {
  float x,y;
  lovrInputGetMousePosition(&x,&y);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  return 2;
}

int l_lovrInputIsMousePressed(lua_State* L) {
  MouseButton* button = (MouseButton*) luax_checkenum(L, 1, &MouseButtons, "mouse button");
  lua_pushboolean(L, lovrInputIsMouseDown(*button));
  return 1;
}

const luaL_Reg lovrInput[] = {
  { "getMousePosition", l_lovrInputGetMousePosition },
  { "isMouseDown", l_lovrInputIsMousePressed },
  { NULL, NULL }
};

