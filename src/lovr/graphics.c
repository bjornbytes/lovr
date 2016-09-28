#include "graphics.h"
#include "buffer.h"
#include "model.h"
#include "shader.h"
#include "../graphics/graphics.h"
#include "../util.h"

const luaL_Reg lovrGraphics[] = {
  { "reset", l_lovrGraphicsReset },
  { "clear", l_lovrGraphicsClear },
  { "present", l_lovrGraphicsPresent },
  { "getClearColor", l_lovrGraphicsGetClearColor },
  { "setClearColor", l_lovrGraphicsSetClearColor },
  { "setShader", l_lovrGraphicsSetShader },
  { "getProjection", l_lovrGraphicsGetProjection },
  { "setProjection", l_lovrGraphicsSetProjection },
  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
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

  map_init(&BufferDrawModes);
  map_set(&BufferDrawModes, "points", BUFFER_POINTS);
  map_set(&BufferDrawModes, "strip", BUFFER_TRIANGLE_STRIP);
  map_set(&BufferDrawModes, "triangles", BUFFER_TRIANGLES);
  map_set(&BufferDrawModes, "fan", BUFFER_TRIANGLE_FAN);

  map_init(&BufferUsages);
  map_set(&BufferUsages, "static", BUFFER_STATIC);
  map_set(&BufferUsages, "dynamic", BUFFER_DYNAMIC);
  map_set(&BufferUsages, "stream", BUFFER_STREAM);

  lovrGraphicsInit();
  return 1;
}

int l_lovrGraphicsReset(lua_State* L) {
  lovrGraphicsReset();
  return 0;
}

int l_lovrGraphicsClear(lua_State* L) {
  int color = lua_gettop(L) < 1 || lua_toboolean(L, 1);
  int depth = lua_gettop(L) < 2 || lua_toboolean(L, 2);
  lovrGraphicsClear(color, depth);
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

int l_lovrGraphicsGetProjection(lua_State* L) {
  float near, far, fov;
  lovrGraphicsGetProjection(&near, &far, &fov);
  lua_pushnumber(L, near);
  lua_pushnumber(L, far);
  lua_pushnumber(L, fov);
  return 3;
}

int l_lovrGraphicsSetProjection(lua_State* L) {
  float near = luaL_checknumber(L, 1);
  float far = luaL_checknumber(L, 2);
  float fov = luaL_checknumber(L, 3);
  lovrGraphicsSetProjection(near, far, fov);
  return 0;
}

int l_lovrGraphicsPush(lua_State* L) {
  if (lovrGraphicsPush()) {
    return luaL_error(L, "Unbalanced matrix stack (more pushes than pops?)");
  }

  return 0;
}

int l_lovrGraphicsPop(lua_State* L) {
  if (lovrGraphicsPop()) {
    return luaL_error(L, "Unbalanced matrix stack (more pops than pushes?)");
  }

  return 0;
}

int l_lovrGraphicsOrigin(lua_State* L) {
  lovrGraphicsOrigin();
  return 0;
}

int l_lovrGraphicsTranslate(lua_State* L) {
  float x = luaL_checknumber(L, 1);
  float y = luaL_checknumber(L, 2);
  float z = luaL_checknumber(L, 3);
  lovrGraphicsTranslate(x, y, z);
  return 0;
}

int l_lovrGraphicsRotate(lua_State* L) {
  float w = luaL_checknumber(L, 1);
  float x = luaL_checknumber(L, 2);
  float y = luaL_checknumber(L, 3);
  float z = luaL_checknumber(L, 4);
  lovrGraphicsRotate(w, x, y, z);
  return 0;
}

int l_lovrGraphicsScale(lua_State* L) {
  float x = luaL_checknumber(L, 1);
  float y = luaL_checknumber(L, 2);
  float z = luaL_checknumber(L, 3);
  lovrGraphicsScale(x, y, z);
  return 0;
}

int l_lovrGraphicsNewBuffer(lua_State* L) {
  const char* userDrawMode = luaL_optstring(L, 2, "fan");
  BufferDrawMode* drawMode = (BufferDrawMode*) map_get(&BufferDrawModes, userDrawMode);
  if (!drawMode) {
    return luaL_error(L, "Invalid buffer draw mode: '%s'", userDrawMode);
  }

  const char* userUsage = luaL_optstring(L, 3, "dynamic");
  BufferUsage* usage = (BufferUsage*) map_get(&BufferUsages, userUsage);
  if (!usage) {
    return luaL_error(L, "Invalid buffer usage: '%s'", userUsage);
  }

  int size;

  if (lua_isnumber(L, 1)) {
    size = lua_tonumber(L, 1);
  } else if (lua_istable(L, 1)) {
    size = lua_objlen(L, 1);
  } else {
    return luaL_argerror(L, 1, "table or number expected");
  }

  Buffer* buffer = lovrGraphicsNewBuffer(size, *drawMode, *usage);

  if (lua_istable(L, 1)) {
    float x, y, z;
    for (int i = 0; i < size; i++) {
      lua_rawgeti(L, 1, i + 1);
      lua_rawgeti(L, -1, 1);
      lua_rawgeti(L, -2, 2);
      lua_rawgeti(L, -3, 3);
      x = lua_tonumber(L, -3);
      y = lua_tonumber(L, -2);
      z = lua_tonumber(L, -1);
      lovrBufferSetVertex(buffer, i, x, y, z);
      lua_pop(L, 4);
    }
  }

  luax_pushbuffer(L, buffer);

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
