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
  { "getBackgroundColor", l_lovrGraphicsGetBackgroundColor },
  { "setBackgroundColor", l_lovrGraphicsSetBackgroundColor },
  { "getColor", l_lovrGraphicsGetColor },
  { "setColor", l_lovrGraphicsSetColor },
  { "getColorMask", l_lovrGraphicsGetColorMask },
  { "setColorMask", l_lovrGraphicsSetColorMask },
  { "getScissor", l_lovrGraphicsGetScissor },
  { "setScissor", l_lovrGraphicsSetScissor },
  { "getShader", l_lovrGraphicsGetShader },
  { "setShader", l_lovrGraphicsSetShader },
  { "setProjection", l_lovrGraphicsSetProjection },
  { "getLineWidth", l_lovrGraphicsGetLineWidth },
  { "setLineWidth", l_lovrGraphicsSetLineWidth },
  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
  { "line", l_lovrGraphicsLine },
  { "cube", l_lovrGraphicsCube },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
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

  map_init(&DrawModes);
  map_set(&DrawModes, "fill", DRAW_MODE_FILL);
  map_set(&DrawModes, "line", DRAW_MODE_LINE);

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

int l_lovrGraphicsGetBackgroundColor(lua_State* L) {
  float r, g, b, a;
  lovrGraphicsGetBackgroundColor(&r, &g, &b, &a);
  lua_pushnumber(L, r);
  lua_pushnumber(L, g);
  lua_pushnumber(L, b);
  lua_pushnumber(L, a);
  return 4;
}

int l_lovrGraphicsSetBackgroundColor(lua_State* L) {
  float r = luaL_checknumber(L, 1);
  float g = luaL_checknumber(L, 2);
  float b = luaL_checknumber(L, 3);
  float a = 255.0;
  if (lua_gettop(L) > 3) {
    a = luaL_checknumber(L, 4);
  }
  lovrGraphicsSetBackgroundColor(r, g, b, a);
  return 0;
}

int l_lovrGraphicsGetColor(lua_State* L) {
  unsigned char r, g, b, a;
  lovrGraphicsGetColor(&r, &g, &b, &a);
  lua_pushinteger(L, r);
  lua_pushinteger(L, g);
  lua_pushinteger(L, b);
  lua_pushinteger(L, a);
  return 4;
}

int l_lovrGraphicsSetColor(lua_State* L) {
  if (lua_gettop(L) <= 1 && lua_isnoneornil(L, 1)) {
    lovrGraphicsSetColor(255, 255, 255, 255);
    return 0;
  }

  unsigned char r = lua_tointeger(L, 1);
  unsigned char g = lua_tointeger(L, 2);
  unsigned char b = lua_tointeger(L, 3);
  unsigned char a = lua_isnoneornil(L, 4) ? 255 : lua_tointeger(L, 4);
  lovrGraphicsSetColor(r, g, b, a);
  return 0;
}

int l_lovrGraphicsGetColorMask(lua_State* L) {
  char r, g, b, a;
  lovrGraphicsGetColorMask(&r, &g, &b, &a);
  lua_pushboolean(L, r);
  lua_pushboolean(L, g);
  lua_pushboolean(L, b);
  lua_pushboolean(L, a);
  return 4;
}

int l_lovrGraphicsSetColorMask(lua_State* L) {
  if (lua_gettop(L) <= 1 && lua_isnoneornil(L, 1)) {
    lovrGraphicsSetColorMask(1, 1, 1, 1);
    return 0;
  }

  char r = lua_toboolean(L, 1);
  char g = lua_toboolean(L, 2);
  char b = lua_toboolean(L, 3);
  char a = lua_toboolean(L, 4);
  lovrGraphicsSetColorMask(r, g, b, a);
  return 0;
}

int l_lovrGraphicsGetScissor(lua_State* L) {
  if (!lovrGraphicsIsScissorEnabled()) {
    lua_pushnil(L);
    return 1;
  }

  int x, y, width, height;
  lovrGraphicsGetScissor(&x, &y, &width, &height);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 4;
}

int l_lovrGraphicsSetScissor(lua_State* L) {
  if (lua_gettop(L) <= 1 && lua_isnoneornil(L, 1)) {
    lovrGraphicsSetScissorEnabled(0);
    return 0;
  }

  int x = luaL_checkint(L, 1);
  int y = luaL_checkint(L, 2);
  int width = luaL_checkint(L, 3);
  int height = luaL_checkint(L, 4);
  lovrGraphicsSetScissor(x, y, width, height);
  lovrGraphicsSetScissorEnabled(1);
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

int l_lovrGraphicsSetProjection(lua_State* L) {
  float near = luaL_checknumber(L, 1);
  float far = luaL_checknumber(L, 2);
  float fov = luaL_checknumber(L, 3);
  lovrGraphicsSetProjection(near, far, fov);
  return 0;
}

int l_lovrGraphicsGetLineWidth(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetLineWidth());
  return 1;
}

int l_lovrGraphicsSetLineWidth(lua_State* L) {
  float width = luaL_optnumber(L, 1, 1.f);
  lovrGraphicsSetLineWidth(width);
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
  float angle = luaL_checknumber(L, 1);
  float axisX = luaL_checknumber(L, 2);
  float axisY = luaL_checknumber(L, 3);
  float axisZ = luaL_checknumber(L, 4);
  float cos2 = cos(angle / 2);
  float sin2 = sin(angle / 2);
  float w = cos2;
  float x = sin2 * axisX;
  float y = sin2 * axisY;
  float z = sin2 * axisZ;
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

int l_lovrGraphicsLine(lua_State* L) {
  vec_float_t points;
  vec_init(&points);
  int isTable = lua_istable(L, 1);

  if (!isTable && !lua_isnumber(L, 1)) {
    return luaL_error(L, "Expected number or table, got '%s'", lua_typename(L, lua_type(L, 1)));
  }

  int count = isTable ? lua_objlen(L, 1) : lua_gettop(L);
  if (count % 3 != 0) {
    vec_deinit(&points);
    return luaL_error(L, "Number of coordinates must be a multiple of 3, got '%d'", count);
  }

  vec_reserve(&points, count);

  if (isTable) {
    for (int i = 1; i <= count; i++) {
      lua_rawgeti(L, 1, i);
      vec_push(&points, lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
  } else {
    for (int i = 1; i <= count; i++) {
      vec_push(&points, lua_tonumber(L, i));
    }
  }

  lovrGraphicsSetShapeData(points.data, count, NULL, 0);
  lovrGraphicsDrawShape(DRAW_MODE_LINE);
  vec_deinit(&points);
  return 0;
}

int l_lovrGraphicsCube(lua_State* L) {
  const char* userDrawMode = luaL_checkstring(L, 1);
  DrawMode* drawMode = (DrawMode*) map_get(&DrawModes, userDrawMode);
  if (!drawMode) {
    return luaL_error(L, "Invalid draw mode: '%s'", userDrawMode);
  }

  float x = luaL_optnumber(L, 2, 0.f);
  float y = luaL_optnumber(L, 3, 0.f);
  float z = luaL_optnumber(L, 4, 0.f);
  float s = luaL_optnumber(L, 5, 1.f);
  float angle = luaL_optnumber(L, 6, 0.f);
  float axisX = luaL_optnumber(L, 7, 0.f);
  float axisY = luaL_optnumber(L, 8, 0.f);
  float axisZ = luaL_optnumber(L, 9, 0.f);
  lovrGraphicsCube(*drawMode, x, y, z, s, angle, axisX, axisY, axisZ);
  return 0;
}

int l_lovrGraphicsGetWidth(lua_State* L) {
  int width;
  lovrGraphicsGetDimensions(&width, NULL);
  lua_pushnumber(L, width);
  return 1;
}

int l_lovrGraphicsGetHeight(lua_State* L) {
  int height;
  lovrGraphicsGetDimensions(NULL, &height);
  lua_pushnumber(L, height);
  return 1;
}

int l_lovrGraphicsGetDimensions(lua_State* L) {
  int width, height;
  lovrGraphicsGetDimensions(&width, &height);
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
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
