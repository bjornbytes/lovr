#include "api/lovr.h"
#include "graphics/graphics.h"

int l_lovrSkyboxDraw(lua_State* L) {
  Skybox* skybox = luax_checktype(L, 1, Skybox);
  float angle = luaL_optnumber(L, 2, 0.f);
  float ax = luaL_optnumber(L, 3, 0.f);
  float ay = luaL_optnumber(L, 4, 0.f);
  float az = luaL_optnumber(L, 5, 0.f);
  lovrGraphicsSkybox(skybox, angle, ax, ay, az);
  return 0;
}

const luaL_Reg lovrSkybox[] = {
  { "draw", l_lovrSkyboxDraw },
  { NULL, NULL }
};
