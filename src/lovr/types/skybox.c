#include "lovr/types/skybox.h"

void luax_pushskybox(lua_State* L, Skybox* skybox) {
  if (skybox == NULL) {
    lua_pushnil(L);
    return;
  }

  Skybox** userdata = (Skybox**) lua_newuserdata(L, sizeof(Skybox*));
  luaL_getmetatable(L, "Skybox");
  lua_setmetatable(L, -2);
  *userdata = skybox;
}

Skybox* luax_checkskybox(lua_State* L, int index) {
  return *(Skybox**) luaL_checkudata(L, index, "Skybox");
}

const luaL_Reg lovrSkybox[] = {
  { "draw", l_lovrSkyboxDraw },
  { NULL, NULL }
};

int l_lovrSkyboxDraw(lua_State* L) {
  Skybox* skybox = luax_checkskybox(L, 1);
  float angle = luaL_optnumber(L, 2, 0.f);
  float ax = luaL_optnumber(L, 3, 0.f);
  float ay = luaL_optnumber(L, 4, 0.f);
  float az = luaL_optnumber(L, 5, 0.f);
  lovrGraphicsSkybox(skybox, angle, ax, ay, az);
  return 0;
}
