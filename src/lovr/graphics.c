#include "lovr/graphics.h"
#include "lovr/types/buffer.h"
#include "lovr/types/model.h"
#include "lovr/types/shader.h"
#include "lovr/types/skybox.h"
#include "lovr/types/texture.h"
#include "graphics/graphics.h"
#include "filesystem/filesystem.h"
#include "util.h"
#include <math.h>

static void luax_readvertices(lua_State* L, int index, vec_float_t* points) {
  int isTable = lua_istable(L, index);

  if (!isTable && !lua_isnumber(L, index)) {
    luaL_error(L, "Expected number or table, got '%s'", lua_typename(L, lua_type(L, 1)));
    return;
  }

  int count = isTable ? lua_objlen(L, index) : lua_gettop(L);
  if (count % 3 != 0) {
    vec_deinit(points);
    luaL_error(L, "Number of coordinates must be a multiple of 3, got '%d'", count);
    return;
  }

  vec_reserve(points, count);

  if (isTable) {
    for (int i = 1; i <= count; i++) {
      lua_rawgeti(L, index, i);
      vec_push(points, lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
  } else {
    for (int i = 0; i < count; i++) {
      vec_push(points, lua_tonumber(L, index + i));
    }
  }
}

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
  { "getPointSize", l_lovrGraphicsGetPointSize },
  { "setPointSize", l_lovrGraphicsSetPointSize },
  { "isCullingEnabled", l_lovrGraphicsIsCullingEnabled },
  { "setCullingEnabled", l_lovrGraphicsSetCullingEnabled },
  { "getPolygonWinding", l_lovrGraphicsGetPolygonWinding },
  { "setPolygonWinding", l_lovrGraphicsSetPolygonWinding },
  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
  { "points", l_lovrGraphicsPoints },
  { "line", l_lovrGraphicsLine },
  { "plane", l_lovrGraphicsPlane },
  { "cube", l_lovrGraphicsCube },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "newModel", l_lovrGraphicsNewModel },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newShader", l_lovrGraphicsNewShader },
  { "newSkybox", l_lovrGraphicsNewSkybox },
  { "newTexture", l_lovrGraphicsNewTexture },
  { NULL, NULL }
};

int l_lovrGraphicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrGraphics);
  luax_registertype(L, "Buffer", lovrBuffer);
  luax_registertype(L, "Model", lovrModel);
  luax_registertype(L, "Shader", lovrShader);
  luax_registertype(L, "Skybox", lovrSkybox);
  luax_registertype(L, "Texture", lovrTexture);

  map_init(&BufferAttributeTypes);
  map_set(&BufferAttributeTypes, "float", BUFFER_FLOAT);
  map_set(&BufferAttributeTypes, "byte", BUFFER_BYTE);
  map_set(&BufferAttributeTypes, "int", BUFFER_INT);

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

  map_init(&PolygonWindings);
  map_set(&PolygonWindings, "clockwise", POLYGON_WINDING_CLOCKWISE);
  map_set(&PolygonWindings, "counterclockwise", POLYGON_WINDING_COUNTERCLOCKWISE);

  map_init(&FilterModes);
  map_set(&FilterModes, "nearest", FILTER_NEAREST);
  map_set(&FilterModes, "linear", FILTER_LINEAR);

  map_init(&WrapModes);
  map_set(&WrapModes, "clamp", WRAP_CLAMP);
  map_set(&WrapModes, "repeat", WRAP_REPEAT);
  map_set(&WrapModes, "mirroredrepeat", WRAP_MIRRORED_REPEAT);
  map_set(&WrapModes, "clampzero", WRAP_CLAMP_ZERO);

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
  luax_pushtype(L, Shader, lovrGraphicsGetShader());
  return 1;
}

int l_lovrGraphicsSetShader(lua_State* L) {
  Shader* shader = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Shader);
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

int l_lovrGraphicsGetPointSize(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetPointSize());
  return 1;
}

int l_lovrGraphicsSetPointSize(lua_State* L) {
  float size = luaL_optnumber(L, 1, 1.f);
  lovrGraphicsSetPointSize(size);
  return 0;
}

int l_lovrGraphicsIsCullingEnabled(lua_State* L) {
  lua_pushboolean(L, lovrGraphicsIsCullingEnabled());
  return 1;
}

int l_lovrGraphicsSetCullingEnabled(lua_State* L) {
  lovrGraphicsSetCullingEnabled(lua_toboolean(L, 1));
  return 0;
}

int l_lovrGraphicsGetPolygonWinding(lua_State* L) {
  lua_pushstring(L, map_int_find(&PolygonWindings, lovrGraphicsGetPolygonWinding()));
  return 1;
}

int l_lovrGraphicsSetPolygonWinding(lua_State* L) {
  const char* userWinding = luaL_checkstring(L, 1);
  PolygonWinding* winding = (PolygonWinding*) map_get(&PolygonWindings, userWinding);
  if (!winding) {
    return luaL_error(L, "Invalid winding: '%s'", userWinding);
  }

  lovrGraphicsSetPolygonWinding(*winding);
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

int l_lovrGraphicsPoints(lua_State* L) {
  vec_float_t points;
  vec_init(&points);
  luax_readvertices(L, 1, &points);
  lovrGraphicsPoints(points.data, points.length);
  vec_deinit(&points);
  return 0;
}

int l_lovrGraphicsLine(lua_State* L) {
  vec_float_t points;
  vec_init(&points);
  luax_readvertices(L, 1, &points);
  lovrGraphicsLine(points.data, points.length);
  vec_deinit(&points);
  return 0;
}

int l_lovrGraphicsPlane(lua_State* L) {
  const char* userDrawMode = luaL_checkstring(L, 1);
  DrawMode* drawMode = (DrawMode*) map_get(&DrawModes, userDrawMode);
  if (!drawMode) {
    return luaL_error(L, "Invalid draw mode: '%s'", userDrawMode);
  }

  float x = luaL_optnumber(L, 2, 0.f);
  float y = luaL_optnumber(L, 3, 0.f);
  float z = luaL_optnumber(L, 4, 0.f);
  float s = luaL_optnumber(L, 5, 1.f);
  float nx = luaL_optnumber(L, 6, 0.f);
  float ny = luaL_optnumber(L, 7, 1.f);
  float nz = luaL_optnumber(L, 8, 0.f);
  lovrGraphicsPlane(*drawMode, x, y, z, s, nx, ny, nz);
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
  lua_pushnumber(L, lovrGraphicsGetWidth());
  return 1;
}

int l_lovrGraphicsGetHeight(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 1;
}

int l_lovrGraphicsGetDimensions(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 2;
}

int l_lovrGraphicsNewBuffer(lua_State* L) {
  int size;
  int dataIndex = 0;
  int drawModeIndex = 2;
  BufferFormat format;
  vec_init(&format);

  if (lua_isnumber(L, 1)) {
    size = lua_tointeger(L, 1);
  } else if (lua_istable(L, 1)) {
    if (lua_isnumber(L, 2)) {
      drawModeIndex++;
      luax_checkbufferformat(L, 1, &format);
      size = lua_tointeger(L, 2);
      dataIndex = 0;
    } else if (lua_istable(L, 2)) {
      drawModeIndex++;
      luax_checkbufferformat(L, 1, &format);
      size = lua_objlen(L, 2);
      dataIndex = 2;
    } else {
      size = lua_objlen(L, 1);
      dataIndex = 1;
    }
  } else {
    luaL_argerror(L, 1, "table or number expected");
    return 0;
  }

  BufferDrawMode* drawMode = (BufferDrawMode*) luax_optenum(L, drawModeIndex, "fan", &BufferDrawModes, "buffer draw mode");
  BufferUsage* usage = (BufferUsage*) luax_optenum(L, drawModeIndex + 1, "dynamic", &BufferUsages, "buffer usage");
  Buffer* buffer = lovrBufferCreate(size, format.length ? &format : NULL, *drawMode, *usage);

  if (dataIndex) {
    void* vertex = lovrBufferGetScratchVertex(buffer);
    int length = lua_objlen(L, dataIndex);
    BufferFormat format = lovrBufferGetVertexFormat(buffer);

    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, dataIndex, i + 1);

      if (!lua_istable(L, -1)) {
        return luaL_error(L, "Vertex information should be specified as a table");
      }

      int tableCount = lua_objlen(L, -1);
      int tableIndex = 1;
      void* v = vertex;
      int j;
      BufferAttribute attribute;

      vec_foreach(&format, attribute, j) {
        for (int k = 0; k < attribute.count; k++) {
          if (attribute.type == BUFFER_FLOAT) {
            float value = 0.f;
            if (tableIndex <= tableCount) {
              lua_rawgeti(L, -1, tableIndex++);
              value = lua_tonumber(L, -1);
              lua_pop(L, 1);
            }

            *((float*) v) = value;
            v = (char*) v + sizeof(float);
          } else if (attribute.type == BUFFER_BYTE) {
            unsigned char value = 255;
            if (tableIndex <= tableCount) {
              lua_rawgeti(L, -1, tableIndex++);
              value = lua_tointeger(L, -1);
              lua_pop(L, 1);
            }

            *((unsigned char*) v) = value;
            v = (char*) v + sizeof(unsigned char);
          } else if (attribute.type == BUFFER_INT) {
            int value = 0;
            if (tableIndex <= tableCount) {
              lua_rawgeti(L, -1, tableIndex++);
              value = lua_tointeger(L, -1);
              lua_pop(L, 1);
            }

            *((int*) v) = value;
            v = (char*) v + sizeof(int);
          }
        }
      }

      lovrBufferSetVertex(buffer, i, vertex);
      lua_pop(L, 1);
    }
  }

  vec_deinit(&format);
  luax_pushtype(L, Buffer, buffer);
  return 1;
}

int l_lovrGraphicsNewModel(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  int size;
  void* data = lovrFilesystemRead(path, &size);
  if (!data) {
    return luaL_error(L, "Could not load model file '%s'", path);
  }

  luax_pushtype(L, Model, lovrModelCreate(data, size));
  free(data);
  return 1;
}

int l_lovrGraphicsNewShader(lua_State* L) {
  for (int i = 0; i < 2; i++) {
    if (lua_isnoneornil(L, i + 1)) continue;
    const char* source = luaL_checkstring(L, i + 1);
    if (!lovrFilesystemIsFile(source)) continue;
    int bytesRead;
    char* contents = lovrFilesystemRead(source, &bytesRead);
    if (bytesRead <= 0) {
      return luaL_error(L, "Could not read shader from file '%s'", source);
    }
    lua_pushlstring(L, contents, bytesRead);
    lua_replace(L, i + 1);
    free(contents);
  }

  const char* vertexSource = lua_tostring(L, 1);
  const char* fragmentSource = lua_tostring(L, 2);
  luax_pushtype(L, Shader, lovrShaderCreate(vertexSource, fragmentSource));
  return 1;
}

int l_lovrGraphicsNewSkybox(lua_State* L) {
  void* data[6];
  int size[6];

  if (lua_istable(L, 1)) {
    if (lua_objlen(L, 1) != 6) {
      return luaL_argerror(L, 1, "Expected 6 strings or a table containing 6 strings");
    }

    for (int i = 0; i < 6; i++) {
      lua_rawgeti(L, 1, i + 1);

      if (!lua_isstring(L, -1)) {
        return luaL_argerror(L, 1, "Expected 6 strings or a table containing 6 strings");
      }

      const char* filename = lua_tostring(L, -1);
      data[i] = lovrFilesystemRead(filename, size + i);
      lua_pop(L, 1);
    }
  } else {
    for (int i = 0; i < 6; i++) {
      const char* filename = luaL_checkstring(L, i + 1);
      data[i] = lovrFilesystemRead(filename, size + i);
    }
  }

  luax_pushtype(L, Skybox, lovrSkyboxCreate(data, size));

  for (int i = 0; i < 6; i++) {
    free(data[i]);
  }

  return 1;
}

int l_lovrGraphicsNewTexture(lua_State* L) {
  Texture* texture;

  if (lua_isstring(L, 1)) {
    const char* path = luaL_checkstring(L, 1);
    int size;
    void* data = lovrFilesystemRead(path, &size);
    if (!data) {
      return luaL_error(L, "Could not load texture file '%s'", path);
    }
    texture = lovrTextureCreate(data, size);
    free(data);
  } else {
    Buffer* buffer = luax_checktype(L, 1, Buffer); // TODO don't error if it's not a buffer
    texture = lovrTextureCreateFromBuffer(buffer);
  }

  luax_pushtype(L, Texture, texture);
  return 1;
}

