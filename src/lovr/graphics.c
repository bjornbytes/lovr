#include "graphics.h"
#include "buffer.h"
#include "model.h"
#include "shader.h"
#include "../graphics/graphics.h"
#include "../util.h"

const luaL_Reg lovrGraphics[] = {
  { "clear", l_lovrGraphicsClear },
  { "present", l_lovrGraphicsPresent },
  { "getClearColor", l_lovrGraphicsGetClearColor },
  { "setClearColor", l_lovrGraphicsSetClearColor },
  { "setShader", l_lovrGraphicsSetShader },
  { "newModel", l_lovrGraphicsNewModel },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newShader", l_lovrGraphicsNewShader },
  { NULL, NULL }
};

int l_lovrGraphicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrGraphics);
  luaRegisterType(L, "Buffer", lovrBuffer, luax_destroybuffer);
  luaRegisterType(L, "Model", lovrModel, NULL);
  luaRegisterType(L, "Shader", lovrShader, luax_destroyshader);
  lovrGraphicsInit();
  return 1;
}

int l_lovrGraphicsClear(lua_State* L) {
  lovrGraphicsClear();
  return 0;
}

int l_lovrGraphicsPresent(lua_State* L) {
  lovrGraphicsPresent();
  return 0;
}

int l_lovrGraphicsGetClearColor(lua_State* L) {
  float r, g, b, a;
  lovrGraphicsGetClearColor(&r, &g, &b, &a);
  lua_pushnumber(L, r);
  lua_pushnumber(L, g);
  lua_pushnumber(L, b);
  lua_pushnumber(L, a);
  return 4;
}

int l_lovrGraphicsSetClearColor(lua_State* L) {
  float r = luaL_checknumber(L, 1);
  float g = luaL_checknumber(L, 2);
  float b = luaL_checknumber(L, 3);
  float a = 255.0;
  if (lua_gettop(L) > 3) {
    a = luaL_checknumber(L, 4);
  }
  lovrGraphicsSetClearColor(r, g, b, a);
  return 0;
}

int l_lovrGraphicsGetShader(lua_State* L) {
  luax_pushshader(L, lovrGraphicsGetShader());
  return 1;
}

int l_lovrGraphicsSetShader(lua_State* L) {
  Shader* shader = luax_checkshader(L, 1);
  lovrGraphicsSetShader(shader);
  return 0;
}

int l_lovrGraphicsNewBuffer(lua_State* L) {
  int size = luaL_checkint(L, 1);
  luax_pushbuffer(L, lovrGraphicsNewBuffer(size));
  return 1;
}

int l_lovrGraphicsNewModel(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  luax_pushmodel(L, lovrGraphicsNewModel(path));
  return 1;
}

int l_lovrGraphicsNewShader(lua_State* L) {
  const char* vertexSource = luaL_checkstring(L, 1);
  const char* fragmentSource = luaL_checkstring(L, 2);
  luax_pushshader(L, lovrGraphicsNewShader(vertexSource, fragmentSource));
  return 1;
}
