#include "skybox.h"

void luax_pushskybox(lua_State* L, Skybox* skybox) {
  if (skybox == NULL) {
    return lua_pushnil(L);
  }

  Skybox** userdata = (Skybox**) lua_newuserdata(L, sizeof(Skybox*));
  luaL_getmetatable(L, "Skybox");
  lua_setmetatable(L, -2);
  *userdata = skybox;
}

Skybox* luax_checkskybox(lua_State* L, int index) {
  return *(Skybox**) luaL_checkudata(L, index, "Skybox");
}

int luax_destroyskybox(lua_State* L) {
  Skybox* skybox = luax_checkskybox(L, 1);
  lovrSkyboxDestroy(skybox);
  return 0;
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
  lovrSkyboxDraw(skybox, angle, ax, ay, az);
  return 0;
}
