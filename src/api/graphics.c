#include "api/lovr.h"
#include "graphics/graphics.h"
#include "loaders/font.h"
#include "loaders/model.h"
#include "loaders/texture.h"
#include "filesystem/filesystem.h"
#include <math.h>

map_int_t BlendAlphaModes;
map_int_t BlendModes;
map_int_t CompareModes;
map_int_t DrawModes;
map_int_t FilterModes;
map_int_t HorizontalAligns;
map_int_t MeshAttributeTypes;
map_int_t MeshDrawModes;
map_int_t MeshUsages;
map_int_t PolygonWindings;
map_int_t TextureProjections;
map_int_t VerticalAligns;
map_int_t WrapModes;

static void luax_readvertices(lua_State* L, int index, vec_float_t* points) {
  int isTable = lua_istable(L, index);

  if (!isTable && !lua_isnumber(L, index)) {
    luaL_error(L, "Expected number or table, got '%s'", lua_typename(L, lua_type(L, 1)));
    return;
  }

  int count = isTable ? lua_objlen(L, index) : lua_gettop(L) - index + 1;
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

static Texture* luax_readtexture(lua_State* L, int index) {
  Blob* blob = luax_readblob(L, index, "Texture");
  TextureData* textureData = lovrTextureDataFromBlob(blob);
  Texture* texture = lovrTextureCreate(textureData);
  lovrRelease(&blob->ref);
  return texture;
}

// Base

int l_lovrGraphicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrGraphics);
  luax_registertype(L, "Font", lovrFont);
  luax_registertype(L, "Mesh", lovrMesh);
  luax_registertype(L, "Model", lovrModel);
  luax_registertype(L, "Shader", lovrShader);
  luax_registertype(L, "Skybox", lovrSkybox);
  luax_registertype(L, "Texture", lovrTexture);

  map_init(&BlendAlphaModes);
  map_set(&BlendAlphaModes, "alphamultiply", BLEND_ALPHA_MULTIPLY);
  map_set(&BlendAlphaModes, "premultiplied", BLEND_PREMULTIPLIED);

  map_init(&BlendModes);
  map_set(&BlendModes, "alpha", BLEND_ALPHA);
  map_set(&BlendModes, "add", BLEND_ADD);
  map_set(&BlendModes, "subtract", BLEND_SUBTRACT);
  map_set(&BlendModes, "multiply", BLEND_MULTIPLY);
  map_set(&BlendModes, "lighten", BLEND_LIGHTEN);
  map_set(&BlendModes, "darken", BLEND_DARKEN);
  map_set(&BlendModes, "screen", BLEND_SCREEN);
  map_set(&BlendModes, "replace", BLEND_REPLACE);

  map_init(&CompareModes);
  map_set(&CompareModes, "equal", COMPARE_EQUAL);
  map_set(&CompareModes, "notequal", COMPARE_NOT_EQUAL);
  map_set(&CompareModes, "less", COMPARE_LESS);
  map_set(&CompareModes, "lequal", COMPARE_LEQUAL);
  map_set(&CompareModes, "gequal", COMPARE_GEQUAL);
  map_set(&CompareModes, "greater", COMPARE_GREATER);

  map_init(&DrawModes);
  map_set(&DrawModes, "fill", DRAW_MODE_FILL);
  map_set(&DrawModes, "line", DRAW_MODE_LINE);

  map_init(&FilterModes);
  map_set(&FilterModes, "nearest", FILTER_NEAREST);
  map_set(&FilterModes, "bilinear", FILTER_BILINEAR);
  map_set(&FilterModes, "trilinear", FILTER_TRILINEAR);
  map_set(&FilterModes, "anisotropic", FILTER_ANISOTROPIC);

  map_init(&HorizontalAligns);
  map_set(&HorizontalAligns, "left", ALIGN_LEFT);
  map_set(&HorizontalAligns, "right", ALIGN_RIGHT);
  map_set(&HorizontalAligns, "center", ALIGN_CENTER);

  map_init(&MeshAttributeTypes);
  map_set(&MeshAttributeTypes, "float", MESH_FLOAT);
  map_set(&MeshAttributeTypes, "byte", MESH_BYTE);
  map_set(&MeshAttributeTypes, "int", MESH_INT);

  map_init(&MeshDrawModes);
  map_set(&MeshDrawModes, "points", MESH_POINTS);
  map_set(&MeshDrawModes, "strip", MESH_TRIANGLE_STRIP);
  map_set(&MeshDrawModes, "triangles", MESH_TRIANGLES);
  map_set(&MeshDrawModes, "fan", MESH_TRIANGLE_FAN);

  map_init(&MeshUsages);
  map_set(&MeshUsages, "static", MESH_STATIC);
  map_set(&MeshUsages, "dynamic", MESH_DYNAMIC);
  map_set(&MeshUsages, "stream", MESH_STREAM);

  map_init(&PolygonWindings);
  map_set(&PolygonWindings, "clockwise", POLYGON_WINDING_CLOCKWISE);
  map_set(&PolygonWindings, "counterclockwise", POLYGON_WINDING_COUNTERCLOCKWISE);

  map_init(&TextureProjections);
  map_set(&TextureProjections, "2d", PROJECTION_ORTHOGRAPHIC);
  map_set(&TextureProjections, "3d", PROJECTION_PERSPECTIVE);

  map_init(&VerticalAligns);
  map_set(&VerticalAligns, "top", ALIGN_TOP);
  map_set(&VerticalAligns, "bottom", ALIGN_BOTTOM);
  map_set(&VerticalAligns, "middle", ALIGN_MIDDLE);

  map_init(&WrapModes);
  map_set(&WrapModes, "clamp", WRAP_CLAMP);
  map_set(&WrapModes, "repeat", WRAP_REPEAT);
  map_set(&WrapModes, "mirroredrepeat", WRAP_MIRRORED_REPEAT);

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

// State

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

int l_lovrGraphicsGetBlendMode(lua_State* L) {
  BlendMode mode;
  BlendAlphaMode alphaMode;
  lovrGraphicsGetBlendMode(&mode, &alphaMode);
  luax_pushenum(L, &BlendModes, mode);
  luax_pushenum(L, &BlendAlphaModes, alphaMode);
  return 2;
}

int l_lovrGraphicsSetBlendMode(lua_State* L) {
  BlendMode mode = *(BlendMode*) luax_checkenum(L, 1, &BlendModes, "blend mode");
  BlendAlphaMode alphaMode = *(BlendAlphaMode*) luax_optenum(L, 2, "alphamultiply", &BlendAlphaModes, "alpha blend mode");
  lovrGraphicsSetBlendMode(mode, alphaMode);
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
  unsigned char r, g, b, a;
  r = g = b = a = 0xff;

  if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
    unsigned int x = lua_tointeger(L, 1);
    r = LOVR_COLOR_R(x);
    g = LOVR_COLOR_G(x);
    b = LOVR_COLOR_B(x);
    a = LOVR_COLOR_A(x);
  } else if (lua_istable(L, 1)) {
    for (int i = 1; i <= 4; i++) {
      lua_rawgeti(L, 1, i);
    }
    r = luaL_checknumber(L, -4);
    g = luaL_checknumber(L, -3);
    b = luaL_checknumber(L, -2);
    a = lua_gettop(L) > 1 ? luaL_checknumber(L, 2) : luaL_optnumber(L, -1, 255);
    lua_pop(L, 4);
  } else if (lua_gettop(L) >= 3) {
    r = lua_tointeger(L, 1);
    g = lua_tointeger(L, 2);
    b = lua_tointeger(L, 3);
    a = lua_isnoneornil(L, 4) ? 255 : lua_tointeger(L, 4);
  }

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
  Shader* shader = lovrGraphicsGetShader();
  luax_pushtype(L, Shader, shader);
  return 1;
}

int l_lovrGraphicsSetShader(lua_State* L) {
  Shader* shader = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Shader);
  lovrGraphicsSetShader(shader);
  return 0;
}

int l_lovrGraphicsGetFont(lua_State* L) {
  Font* font = lovrGraphicsGetFont();
  luax_pushtype(L, Font, font);
  return 1;
}

int l_lovrGraphicsSetFont(lua_State* L) {
  Font* font = luax_checktype(L, 1, Font);
  lovrGraphicsSetFont(font);
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
  luax_pushenum(L, &PolygonWindings, lovrGraphicsGetPolygonWinding());
  return 1;
}

int l_lovrGraphicsSetPolygonWinding(lua_State* L) {
  PolygonWinding* winding = (PolygonWinding*) luax_checkenum(L, 1, &PolygonWindings, "winding direction");
  lovrGraphicsSetPolygonWinding(*winding);
  return 0;
}

int l_lovrGraphicsGetDepthTest(lua_State* L) {
  luax_pushenum(L, &CompareModes, lovrGraphicsGetDepthTest());
  return 1;
}

int l_lovrGraphicsSetDepthTest(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrGraphicsSetDepthTest(COMPARE_NONE);
  } else {
    CompareMode* depthTest = (CompareMode*) luax_checkenum(L, 1, &CompareModes, "compare mode");
    lovrGraphicsSetDepthTest(*depthTest);
  }
  return 0;
}

int l_lovrGraphicsIsWireframe(lua_State* L) {
  lua_pushboolean(L, lovrGraphicsIsWireframe());
  return 1;
}

int l_lovrGraphicsSetWireframe(lua_State* L) {
  lovrGraphicsSetWireframe(lua_toboolean(L, 1));
  return 0;
}

int l_lovrGraphicsGetDefaultFilter(lua_State* L) {
  FilterMode filter;
  float anisotropy;
  lovrGraphicsGetDefaultFilter(&filter, &anisotropy);
  luax_pushenum(L, &FilterModes, filter);
  if (filter == FILTER_ANISOTROPIC) {
    lua_pushnumber(L, anisotropy);
    return 2;
  }
  return 1;
}

int l_lovrGraphicsSetDefaultFilter(lua_State* L) {
  FilterMode filter = *(FilterMode*) luax_checkenum(L, 1, &FilterModes, "filter mode");
  float anisotropy = luaL_optnumber(L, 2, 1.);
  lovrGraphicsSetDefaultFilter(filter, anisotropy);
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

int l_lovrGraphicsGetSystemLimits(lua_State* L) {
  lua_newtable(L);
  lua_pushnumber(L, lovrGraphicsGetSystemLimit(LIMIT_POINT_SIZE));
  lua_setfield(L, -2, "pointsize");
  lua_pushinteger(L, lovrGraphicsGetSystemLimit(LIMIT_TEXTURE_SIZE));
  lua_setfield(L, -2, "texturesize");
  lua_pushinteger(L, lovrGraphicsGetSystemLimit(LIMIT_TEXTURE_MSAA));
  lua_setfield(L, -2, "texturemsaa");
  return 1;
}

// Transforms

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
  float axisX = luaL_optnumber(L, 2, 0);
  float axisY = luaL_optnumber(L, 3, 1);
  float axisZ = luaL_optnumber(L, 4, 0);
  lovrGraphicsRotate(angle, axisX, axisY, axisZ);
  return 0;
}

int l_lovrGraphicsScale(lua_State* L) {
  float x = luaL_checknumber(L, 1);
  float y = luaL_optnumber(L, 2, x);
  float z = luaL_optnumber(L, 3, x);
  lovrGraphicsScale(x, y, z);
  return 0;
}

int l_lovrGraphicsTransform(lua_State* L) {
  float transform[16];
  luax_readtransform(L, 1, transform, 0);
  lovrGraphicsMatrixTransform(transform);
  return 0;
}

// Primitives

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

int l_lovrGraphicsTriangle(lua_State* L) {
  DrawMode* drawMode = (DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  int top = lua_gettop(L);
  if (top != 10) {
    return luaL_error(L, "Expected 9 coordinates to make a triangle, got %d values", top - 1);
  }
  vec_float_t points;
  vec_init(&points);
  luax_readvertices(L, 2, &points);
  lovrGraphicsTriangle(*drawMode, points.data);
  vec_deinit(&points);
  return 0;
}

int l_lovrGraphicsPlane(lua_State* L) {
  Texture* texture = NULL;
  DrawMode drawMode;
  if (lua_isstring(L, 1)) {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  } else {
    drawMode = DRAW_MODE_FILL;
    texture = luax_checktype(L, 1, Texture);
    if (lua_gettop(L) == 1) {
      lovrGraphicsPlaneFullscreen(texture);
      return 0;
    }
  }
  float transform[16];
  luax_readtransform(L, 2, transform, 1);
  lovrGraphicsPlane(drawMode, texture, transform);
  return 0;
}

static int luax_rectangularprism(lua_State* L, int uniformScale) {
  Texture* texture = NULL;
  DrawMode drawMode;
  if (lua_isstring(L, 1)) {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  } else {
    drawMode = DRAW_MODE_FILL;
    texture = luax_checktype(L, 1, Texture);
  }
  float transform[16];
  luax_readtransform(L, 2, transform, uniformScale);
  lovrGraphicsBox(drawMode, texture, transform);
  return 0;
}

int l_lovrGraphicsCube(lua_State* L) {
  return luax_rectangularprism(L, 1);
}

int l_lovrGraphicsBox(lua_State* L) {
  return luax_rectangularprism(L, 0);
}

int l_lovrGraphicsCylinder(lua_State* L) {
  float x1 = luaL_checknumber(L, 1);
  float y1 = luaL_checknumber(L, 2);
  float z1 = luaL_checknumber(L, 3);
  float x2 = luaL_checknumber(L, 4);
  float y2 = luaL_checknumber(L, 5);
  float z2 = luaL_checknumber(L, 6);
  float r1 = luaL_optnumber(L, 7, 1);
  float r2 = luaL_optnumber(L, 8, 1);
  int capped = lua_isnoneornil(L, 9) ? 1 : lua_toboolean(L, 9);
  int segments = luaL_optnumber(L, 10, floorf(16 + 16 * MAX(r1, r2)));
  lovrGraphicsCylinder(x1, y1, z1, x2, y2, z2, r1, r2, capped, segments);
  return 0;
}

int l_lovrGraphicsSphere(lua_State* L) {
  Texture* texture = NULL;
  float transform[16];
  int index = 1;
  if (lua_isuserdata(L, 1) && (lua_isuserdata(L, 2) || lua_isnumber(L, 2))) {
    texture = luax_checktype(L, index++, Texture);
  }
  index = luax_readtransform(L, index, transform, 1);
  int segments = luaL_optnumber(L, index, 30);
  lovrGraphicsSphere(texture, transform, segments);
  return 0;
}

int l_lovrGraphicsPrint(lua_State* L) {
  const char* str = luaL_checkstring(L, 1);
  float transform[16];
  int index = luax_readtransform(L, 2, transform, 1);
  float wrap = luaL_optnumber(L, index++, 0);
  HorizontalAlign halign = *(HorizontalAlign*) luax_optenum(L, index++, "center", &HorizontalAligns, "alignment");
  VerticalAlign valign = *(VerticalAlign*) luax_optenum(L, index++, "middle", &VerticalAligns, "alignment");
  lovrGraphicsPrint(str, transform, wrap, halign, valign);
  return 0;
}

// Types

int l_lovrGraphicsNewFont(lua_State* L) {
  Blob* blob = NULL;
  float size;

  if (lua_type(L, 1) == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
    size = luaL_optnumber(L, 1, 32);
  } else {
    blob = luax_readblob(L, 1, "Font");
    size = luaL_optnumber(L, 2, 32);
  }

  FontData* fontData = lovrFontDataCreate(blob, size);
  Font* font = lovrFontCreate(fontData);
  luax_pushtype(L, Font, font);
  lovrRelease(&font->ref);

  if (blob) {
    lovrRelease(&blob->ref);
  }

  return 1;
}

int l_lovrGraphicsNewMesh(lua_State* L) {
  int size;
  int dataIndex = 0;
  int drawModeIndex = 2;
  MeshFormat format;
  vec_init(&format);

  if (lua_isnumber(L, 1)) {
    size = lua_tointeger(L, 1);
  } else if (lua_istable(L, 1)) {
    if (lua_isnumber(L, 2)) {
      drawModeIndex++;
      luax_checkmeshformat(L, 1, &format);
      size = lua_tointeger(L, 2);
      dataIndex = 0;
    } else if (lua_istable(L, 2)) {
      drawModeIndex++;
      luax_checkmeshformat(L, 1, &format);
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

  MeshDrawMode* drawMode = (MeshDrawMode*) luax_optenum(L, drawModeIndex, "fan", &MeshDrawModes, "mesh draw mode");
  MeshUsage* usage = (MeshUsage*) luax_optenum(L, drawModeIndex + 1, "dynamic", &MeshUsages, "mesh usage");
  Mesh* mesh = lovrMeshCreate(size, format.length ? &format : NULL, *drawMode, *usage);

  if (dataIndex) {
    int count = lua_objlen(L, dataIndex);
    MeshFormat format = lovrMeshGetVertexFormat(mesh);
    char* vertex = lovrMeshMap(mesh, 0, count);

    for (int i = 0; i < count; i++) {
      lua_rawgeti(L, dataIndex, i + 1);
      if (!lua_istable(L, -1)) {
        return luaL_error(L, "Vertex information should be specified as a table");
      }

      int component = 0;
      for (int j = 0; j < format.length; j++) {
        MeshAttribute attribute = format.data[j];
        for (int k = 0; k < attribute.count; k++) {
          lua_rawgeti(L, -1, ++component);
          switch (attribute.type) {
            case MESH_FLOAT: *((float*) vertex) = luaL_optnumber(L, -1, 0.f); break;
            case MESH_BYTE: *((unsigned char*) vertex) = luaL_optint(L, -1, 255); break;
            case MESH_INT: *((int*) vertex) = luaL_optint(L, -1, 0); break;
          }
          vertex += sizeof(attribute.type);
          lua_pop(L, 1);
        }
      }

      lua_pop(L, 1);
    }

    lovrMeshUnmap(mesh);
  }

  vec_deinit(&format);
  luax_pushtype(L, Mesh, mesh);
  lovrRelease(&mesh->ref);
  return 1;
}

int l_lovrGraphicsNewModel(lua_State* L) {
  Blob* blob = luax_readblob(L, 1, "Model");
  ModelData* modelData = lovrModelDataCreate(blob);
  Model* model = lovrModelCreate(modelData);

  if (lua_gettop(L) >= 2) {
    Texture* texture = luax_readtexture(L, 2);
    lovrModelSetTexture(model, texture);
    lovrRelease(&texture->ref);
  }

  luax_pushtype(L, Model, model);
  lovrRelease(&model->ref);
  lovrRelease(&blob->ref);
  return 1;
}

int l_lovrGraphicsNewShader(lua_State* L) {
  for (int i = 0; i < 2; i++) {
    if (lua_isnoneornil(L, i + 1)) continue;
    const char* source = luaL_checkstring(L, i + 1);
    if (!lovrFilesystemIsFile(source)) continue;
    size_t bytesRead;
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
  Shader* shader = lovrShaderCreate(vertexSource, fragmentSource);
  luax_pushtype(L, Shader, shader);
  lovrRelease(&shader->ref);
  return 1;
}

int l_lovrGraphicsNewSkybox(lua_State* L) {
  Blob* blobs[6] = { NULL };
  SkyboxType type;

  if (lua_gettop(L) == 1 && lua_type(L, 1) == LUA_TSTRING) {
    type = SKYBOX_PANORAMA;
    blobs[0] = luax_readblob(L, 1, "Skybox");
  } else if (lua_istable(L, 1)) {
    if (lua_objlen(L, 1) != 6) {
      return luaL_argerror(L, 1, "Expected 6 strings or a table containing 6 strings");
    }

    for (int i = 0; i < 6; i++) {
      lua_rawgeti(L, 1, i + 1);

      if (!lua_isstring(L, -1)) {
        return luaL_argerror(L, 1, "Expected 6 strings or a table containing 6 strings");
      }

      blobs[i] = luax_readblob(L, -1, "Skybox");
      lua_pop(L, 1);
    }

    type = SKYBOX_CUBE;
  } else {
    for (int i = 0; i < 6; i++) {
      blobs[i] = luax_readblob(L, i + 1, "Skybox");
    }

    type = SKYBOX_CUBE;
  }

  Skybox* skybox = lovrSkyboxCreate(blobs, type);
  luax_pushtype(L, Skybox, skybox);
  lovrRelease(&skybox->ref);

  for (int i = 0; i < 6; i++) {
    if (blobs[i]) {
      lovrRelease(&blobs[i]->ref);
    }
  }

  return 1;
}

int l_lovrGraphicsNewTexture(lua_State* L) {
  Texture* texture;

  if (lua_type(L, 1) == LUA_TNUMBER) {
    int width = luaL_checknumber(L, 1);
    int height = luaL_checknumber(L, 2);
    TextureProjection* projection = luax_optenum(L, 3, "2d", &TextureProjections, "projection");
    int msaa = luaL_optnumber(L, 4, 0);
    TextureData* textureData = lovrTextureDataGetEmpty(width, height, FORMAT_RGBA);
    texture = lovrTextureCreateWithFramebuffer(textureData, *projection, msaa);
  } else {
    texture = luax_readtexture(L, 1);
  }

  luax_pushtype(L, Texture, texture);
  lovrRelease(&texture->ref);
  return 1;
}

const luaL_Reg lovrGraphics[] = {
  { "reset", l_lovrGraphicsReset },
  { "clear", l_lovrGraphicsClear },
  { "present", l_lovrGraphicsPresent },
  { "getBackgroundColor", l_lovrGraphicsGetBackgroundColor },
  { "setBackgroundColor", l_lovrGraphicsSetBackgroundColor },
  { "getBlendMode", l_lovrGraphicsGetBlendMode },
  { "setBlendMode", l_lovrGraphicsSetBlendMode },
  { "getColor", l_lovrGraphicsGetColor },
  { "setColor", l_lovrGraphicsSetColor },
  { "getColorMask", l_lovrGraphicsGetColorMask },
  { "setColorMask", l_lovrGraphicsSetColorMask },
  { "getScissor", l_lovrGraphicsGetScissor },
  { "setScissor", l_lovrGraphicsSetScissor },
  { "getShader", l_lovrGraphicsGetShader },
  { "setShader", l_lovrGraphicsSetShader },
  { "getFont", l_lovrGraphicsGetFont },
  { "setFont", l_lovrGraphicsSetFont },
  { "getLineWidth", l_lovrGraphicsGetLineWidth },
  { "setLineWidth", l_lovrGraphicsSetLineWidth },
  { "getPointSize", l_lovrGraphicsGetPointSize },
  { "setPointSize", l_lovrGraphicsSetPointSize },
  { "isCullingEnabled", l_lovrGraphicsIsCullingEnabled },
  { "setCullingEnabled", l_lovrGraphicsSetCullingEnabled },
  { "getPolygonWinding", l_lovrGraphicsGetPolygonWinding },
  { "setPolygonWinding", l_lovrGraphicsSetPolygonWinding },
  { "getDepthTest", l_lovrGraphicsGetDepthTest },
  { "setDepthTest", l_lovrGraphicsSetDepthTest },
  { "isWireframe", l_lovrGraphicsIsWireframe },
  { "setWireframe", l_lovrGraphicsSetWireframe },
  { "getDefaultFilter", l_lovrGraphicsGetDefaultFilter },
  { "setDefaultFilter", l_lovrGraphicsSetDefaultFilter },
  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
  { "transform", l_lovrGraphicsTransform },
  { "points", l_lovrGraphicsPoints },
  { "line", l_lovrGraphicsLine },
  { "triangle", l_lovrGraphicsTriangle },
  { "plane", l_lovrGraphicsPlane },
  { "cube", l_lovrGraphicsCube },
  { "box", l_lovrGraphicsBox },
  { "cylinder", l_lovrGraphicsCylinder },
  { "sphere", l_lovrGraphicsSphere },
  { "print", l_lovrGraphicsPrint },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "getSystemLimits", l_lovrGraphicsGetSystemLimits },
  { "newFont", l_lovrGraphicsNewFont },
  { "newMesh", l_lovrGraphicsNewMesh },
  { "newModel", l_lovrGraphicsNewModel },
  { "newShader", l_lovrGraphicsNewShader },
  { "newSkybox", l_lovrGraphicsNewSkybox },
  { "newTexture", l_lovrGraphicsNewTexture },
  { NULL, NULL }
};
