#include "api/lovr.h"
#include "graphics/graphics.h"
#include "graphics/animator.h"
#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "loaders/font.h"
#include "loaders/material.h"
#include "loaders/model.h"
#include "loaders/texture.h"
#include "filesystem/filesystem.h"
#include <math.h>
#include <stdbool.h>

map_int_t ArcModes;
map_int_t BlendAlphaModes;
map_int_t BlendModes;
map_int_t CanvasTypes;
map_int_t CompareModes;
map_int_t DrawModes;
map_int_t FilterModes;
map_int_t HorizontalAligns;
map_int_t MaterialColors;
map_int_t MaterialTextures;
map_int_t MatrixTypes;
map_int_t MeshAttributeTypes;
map_int_t MeshDrawModes;
map_int_t MeshUsages;
map_int_t VerticalAligns;
map_int_t Windings;
map_int_t WrapModes;

static int luax_optmatrixtype(lua_State* L, int index, MatrixType* type) {
  if (lua_type(L, index) == LUA_TSTRING) {
    *type = *(MatrixType*) luax_checkenum(L, index++, &MatrixTypes, "matrix type");
  } else {
    *type = MATRIX_MODEL;
  }

  return index;
}

static void luax_readvertices(lua_State* L, int index, vec_float_t* points) {
  bool isTable = lua_istable(L, index);

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

// Base

int l_lovrGraphicsInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrGraphics);
  luax_registertype(L, "Animator", lovrAnimator);
  luax_registertype(L, "Font", lovrFont);
  luax_registertype(L, "Material", lovrMaterial);
  luax_registertype(L, "Mesh", lovrMesh);
  luax_registertype(L, "Model", lovrModel);
  luax_registertype(L, "Shader", lovrShader);
  luax_registertype(L, "Texture", lovrTexture);
  luax_extendtype(L, "Texture", "Canvas", lovrTexture, lovrCanvas);

  map_init(&ArcModes);
  map_set(&ArcModes, "pie", ARC_MODE_PIE);
  map_set(&ArcModes, "open", ARC_MODE_OPEN);
  map_set(&ArcModes, "closed", ARC_MODE_CLOSED);

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

  map_init(&CanvasTypes);
  map_set(&CanvasTypes, "3d", CANVAS_3D);
  map_set(&CanvasTypes, "2d", CANVAS_2D);

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

  map_init(&MaterialColors);
  map_set(&MaterialColors, "diffuse", COLOR_DIFFUSE);

  map_init(&MaterialTextures);
  map_set(&MaterialTextures, "diffuse", TEXTURE_DIFFUSE);
  map_set(&MaterialTextures, "environment", TEXTURE_ENVIRONMENT_MAP);

  map_init(&MatrixTypes);
  map_set(&MatrixTypes, "model", MATRIX_MODEL);
  map_set(&MatrixTypes, "view", MATRIX_VIEW);

  map_init(&MeshAttributeTypes);
  map_set(&MeshAttributeTypes, "float", MESH_FLOAT);
  map_set(&MeshAttributeTypes, "byte", MESH_BYTE);
  map_set(&MeshAttributeTypes, "int", MESH_INT);

  map_init(&MeshDrawModes);
  map_set(&MeshDrawModes, "points", MESH_POINTS);
  map_set(&MeshDrawModes, "lines", MESH_LINES);
  map_set(&MeshDrawModes, "linestrip", MESH_LINE_STRIP);
  map_set(&MeshDrawModes, "strip", MESH_TRIANGLE_STRIP);
  map_set(&MeshDrawModes, "triangles", MESH_TRIANGLES);
  map_set(&MeshDrawModes, "fan", MESH_TRIANGLE_FAN);

  map_init(&MeshUsages);
  map_set(&MeshUsages, "static", MESH_STATIC);
  map_set(&MeshUsages, "dynamic", MESH_DYNAMIC);
  map_set(&MeshUsages, "stream", MESH_STREAM);

  map_init(&VerticalAligns);
  map_set(&VerticalAligns, "top", ALIGN_TOP);
  map_set(&VerticalAligns, "bottom", ALIGN_BOTTOM);
  map_set(&VerticalAligns, "middle", ALIGN_MIDDLE);

  map_init(&Windings);
  map_set(&Windings, "clockwise", WINDING_CLOCKWISE);
  map_set(&Windings, "counterclockwise", WINDING_COUNTERCLOCKWISE);

  map_init(&WrapModes);
  map_set(&WrapModes, "clamp", WRAP_CLAMP);
  map_set(&WrapModes, "repeat", WRAP_REPEAT);
  map_set(&WrapModes, "mirroredrepeat", WRAP_MIRRORED_REPEAT);

  lovrGraphicsInit();

  luax_pushconf(L);

  // Set gamma correct
  lua_getfield(L, -1, "gammacorrect");
  bool gammaCorrect = lua_toboolean(L, -1);
  lovrGraphicsSetGammaCorrect(gammaCorrect);
  lua_pop(L, 1);

  // Create window if needed
  lua_getfield(L, -1, "window");
  if (!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "width");
    int width = luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "height");
    int height = luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "fullscreen");
    bool fullscreen = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "msaa");
    int msaa = luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "title");
    const char* title = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "icon");
    const char* icon = luaL_optstring(L, -1, NULL);
    lua_pop(L, 1);

    lovrGraphicsCreateWindow(width, height, fullscreen, msaa, title, icon);
  }

  lua_pop(L, 2);

  return 1;
}

int l_lovrGraphicsReset(lua_State* L) {
  lovrGraphicsReset();
  return 0;
}

int l_lovrGraphicsClear(lua_State* L) {
  bool color = lua_gettop(L) < 1 || lua_toboolean(L, 1);
  bool depth = lua_gettop(L) < 2 || lua_toboolean(L, 2);
  lovrGraphicsClear(color, depth);
  return 0;
}

int l_lovrGraphicsPresent(lua_State* L) {
  lovrGraphicsPresent();
  return 0;
}

int l_lovrGraphicsCreateWindow(lua_State* L) {
  int width = luaL_optnumber(L, 1, 800);
  int height = luaL_optnumber(L, 2, 600);
  bool fullscreen = !lua_isnoneornil(L, 3) && lua_toboolean(L, 3);
  int msaa = luaL_optnumber(L, 4, 0);
  const char* title = luaL_optstring(L, 5, "LÖVR");
  const char* icon = luaL_optstring(L, 6, NULL);
  lovrGraphicsCreateWindow(width, height, fullscreen, msaa, title, icon);
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

int l_lovrGraphicsGetStats(lua_State* L) {
  if (lua_gettop(L) > 0) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
  } else {
    lua_createtable(L, 0, 2);
  }

  GraphicsStats stats = lovrGraphicsGetStats();

  lua_pushinteger(L, stats.drawCalls);
  lua_setfield(L, 1, "drawcalls");

  lua_pushinteger(L, stats.shaderSwitches);
  lua_setfield(L, 1, "shaderswitches");

  return 1;
}

// State

int l_lovrGraphicsGetBackgroundColor(lua_State* L) {
  Color color = lovrGraphicsGetBackgroundColor();
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

int l_lovrGraphicsSetBackgroundColor(lua_State* L) {
  Color color;
  color.r = luaL_checknumber(L, 1);
  color.g = luaL_checknumber(L, 2);
  color.b = luaL_checknumber(L, 3);
  color.a = luaL_optnumber(L, 4, 1.);
  lovrGraphicsSetBackgroundColor(color);
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
  Color color = lovrGraphicsGetColor();
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

int l_lovrGraphicsSetColor(lua_State* L) {
  Color color = luax_checkcolor(L, 1);
  lovrGraphicsSetColor(color);
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

int l_lovrGraphicsGetDefaultFilter(lua_State* L) {
  TextureFilter filter = lovrGraphicsGetDefaultFilter();
  luax_pushenum(L, &FilterModes, filter.mode);
  if (filter.mode == FILTER_ANISOTROPIC) {
    lua_pushnumber(L, filter.anisotropy);
    return 2;
  }
  return 1;
}

int l_lovrGraphicsSetDefaultFilter(lua_State* L) {
  FilterMode mode = *(FilterMode*) luax_checkenum(L, 1, &FilterModes, "filter mode");
  float anisotropy = luaL_optnumber(L, 2, 1.);
  TextureFilter filter = { .mode = mode, .anisotropy = anisotropy };
  lovrGraphicsSetDefaultFilter(filter);
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

int l_lovrGraphicsGetFont(lua_State* L) {
  Font* font = lovrGraphicsGetFont();
  luax_pushtype(L, Font, font);
  return 1;
}

int l_lovrGraphicsSetFont(lua_State* L) {
  Font* font = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Font);
  lovrGraphicsSetFont(font);
  return 0;
}

int l_lovrGraphicsIsGammaCorrect(lua_State* L) {
  bool gammaCorrect = lovrGraphicsIsGammaCorrect();
  lua_pushboolean(L, gammaCorrect);
  return 1;
}

int l_lovrGraphicsGetSystemLimits(lua_State* L) {
  GraphicsLimits limits = lovrGraphicsGetLimits();
  lua_newtable(L);
  lua_pushnumber(L, limits.pointSizes[1]);
  lua_setfield(L, -2, "pointsize");
  lua_pushinteger(L, limits.textureSize);
  lua_setfield(L, -2, "texturesize");
  lua_pushinteger(L, limits.textureMSAA);
  lua_setfield(L, -2, "texturemsaa");
  lua_pushinteger(L, limits.textureAnisotropy);
  lua_setfield(L, -2, "anisotropy");
  return 1;
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

int l_lovrGraphicsGetWinding(lua_State* L) {
  luax_pushenum(L, &Windings, lovrGraphicsGetWinding());
  return 1;
}

int l_lovrGraphicsSetWinding(lua_State* L) {
  Winding* winding = (Winding*) luax_checkenum(L, 1, &Windings, "winding");
  lovrGraphicsSetWinding(*winding);
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

// Transforms

int l_lovrGraphicsPush(lua_State* L) {
  lovrGraphicsPush();
  return 0;
}

int l_lovrGraphicsPop(lua_State* L) {
  lovrGraphicsPop();
  return 0;
}

int l_lovrGraphicsOrigin(lua_State* L) {
  lovrGraphicsOrigin();
  return 0;
}

int l_lovrGraphicsTranslate(lua_State* L) {
  MatrixType type;
  int i = luax_optmatrixtype(L, 1, &type);
  float x = luaL_checknumber(L, i++);
  float y = luaL_checknumber(L, i++);
  float z = luaL_checknumber(L, i++);
  lovrGraphicsTranslate(type, x, y, z);
  return 0;
}

int l_lovrGraphicsRotate(lua_State* L) {
  MatrixType type;
  int i = luax_optmatrixtype(L, 1, &type);
  float angle = luaL_checknumber(L, i++);
  float axisX = luaL_optnumber(L, i++, 0);
  float axisY = luaL_optnumber(L, i++, 1);
  float axisZ = luaL_optnumber(L, i++, 0);
  lovrGraphicsRotate(type, angle, axisX, axisY, axisZ);
  return 0;
}

int l_lovrGraphicsScale(lua_State* L) {
  MatrixType type;
  int i = luax_optmatrixtype(L, 1, &type);
  float x = luaL_checknumber(L, i++);
  float y = luaL_optnumber(L, i++, x);
  float z = luaL_optnumber(L, i++, x);
  lovrGraphicsScale(type, x, y, z);
  return 0;
}

int l_lovrGraphicsTransform(lua_State* L) {
  MatrixType type;
  int i = luax_optmatrixtype(L, 1, &type);
  float transform[16];
  luax_readtransform(L, i++, transform, 0);
  lovrGraphicsMatrixTransform(type, transform);
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
  DrawMode drawMode = DRAW_MODE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  }

  int top = lua_gettop(L);
  if (top != 10) {
    return luaL_error(L, "Expected 9 coordinates to make a triangle, got %d values", top - 1);
  }
  vec_float_t points;
  vec_init(&points);
  luax_readvertices(L, 2, &points);
  lovrGraphicsTriangle(drawMode, material, points.data);
  vec_deinit(&points);
  return 0;
}

int l_lovrGraphicsPlane(lua_State* L) {
  if (lua_isuserdata(L, 1) && lua_gettop(L) == 1) {
    Texture* texture = luax_checktypeof(L, 1, Texture);
    lovrGraphicsPlaneFullscreen(texture);
    return 0;
  }

  DrawMode drawMode = DRAW_MODE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  }
  float transform[16];
  luax_readtransform(L, 2, transform, 1);
  lovrGraphicsPlane(drawMode, material, transform);
  return 0;
}

static int luax_rectangularprism(lua_State* L, bool uniformScale) {
  DrawMode drawMode = DRAW_MODE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  }
  float transform[16];
  luax_readtransform(L, 2, transform, uniformScale);
  lovrGraphicsBox(drawMode, material, transform);
  return 0;
}

int l_lovrGraphicsCube(lua_State* L) {
  return luax_rectangularprism(L, true);
}

int l_lovrGraphicsBox(lua_State* L) {
  return luax_rectangularprism(L, false);
}

int l_lovrGraphicsArc(lua_State* L) {
  DrawMode drawMode = DRAW_MODE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  }
  ArcMode arcMode = ARC_MODE_PIE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    arcMode = *(ArcMode*) luax_checkenum(L, index++, &ArcModes, "arc mode");
  }
  float transform[16];
  index = luax_readtransform(L, index, transform, true);
  float theta1 = luaL_optnumber(L, index++, 0);
  float theta2 = luaL_optnumber(L, index++, 2 * M_PI);
  int segments = luaL_optinteger(L, index, 32) * fabsf(theta2 - theta1) * 2 * M_PI + .5f;
  lovrGraphicsArc(drawMode, arcMode, material, transform, theta1, theta2, segments);
  return 0;
}

int l_lovrGraphicsCircle(lua_State* L) {
  DrawMode drawMode = DRAW_MODE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    drawMode = *(DrawMode*) luax_checkenum(L, 1, &DrawModes, "draw mode");
  }
  float transform[16];
  int index = luax_readtransform(L, 2, transform, true);
  int segments = luaL_optnumber(L, index, 32);
  lovrGraphicsCircle(drawMode, material, transform, segments);
  return 0;
}

int l_lovrGraphicsCylinder(lua_State* L) {
  int index = 1;
  Material* material = lua_isuserdata(L, index) ? luax_checktype(L, index++, Material) : NULL;
  float x1 = luaL_checknumber(L, index++);
  float y1 = luaL_checknumber(L, index++);
  float z1 = luaL_checknumber(L, index++);
  float x2 = luaL_checknumber(L, index++);
  float y2 = luaL_checknumber(L, index++);
  float z2 = luaL_checknumber(L, index++);
  float r1 = luaL_optnumber(L, index++, 1);
  float r2 = luaL_optnumber(L, index++, 1);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  int segments = luaL_optnumber(L, index, floorf(16 + 16 * MAX(r1, r2)));
  lovrGraphicsCylinder(material, x1, y1, z1, x2, y2, z2, r1, r2, capped, segments);
  return 0;
}

int l_lovrGraphicsSphere(lua_State* L) {
  float transform[16];
  int index = 1;
  Material* material = lua_isuserdata(L, index) ? luax_checktype(L, index++, Material) : NULL;
  index = luax_readtransform(L, index, transform, 1);
  int segments = luaL_optnumber(L, index, 30);
  lovrGraphicsSphere(material, transform, segments);
  return 0;
}

int l_lovrGraphicsSkybox(lua_State* L) {
  Texture* texture = luax_checktypeof(L, 1, Texture);
  float angle = luaL_optnumber(L, 2, 0);
  float ax = luaL_optnumber(L, 3, 0);
  float ay = luaL_optnumber(L, 4, 1);
  float az = luaL_optnumber(L, 5, 0);
  lovrGraphicsSkybox(texture, angle, ax, ay, az);
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

int l_lovrGraphicsNewAnimator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Animator* animator = lovrAnimatorCreate(model->modelData->animationData);
  luax_pushtype(L, Animator, animator);
  return 1;
}

int l_lovrGraphicsNewCanvas(lua_State* L) {
  CanvasType type = CANVAS_3D;
  int index = 1;
  if (lua_type(L, index) == LUA_TSTRING) {
    type = *(CanvasType*) luax_checkenum(L, index++, &CanvasTypes, "canvas type");
  }
  int width = luaL_checkinteger(L, index++);
  int height = luaL_checkinteger(L, index++);
  int msaa = luaL_optinteger(L, index++, 0);
  Canvas* canvas = lovrCanvasCreate(type, width, height, msaa);
  luax_pushtype(L, Canvas, canvas);
  return 1;
}

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

int l_lovrGraphicsNewMaterial(lua_State* L) {
  MaterialData* materialData = lovrMaterialDataCreateEmpty();
  Material* material = lovrMaterialCreate(materialData, false);

  int index = 1;

  if (lua_type(L, index) == LUA_TSTRING) {
    Blob* blob = luax_readblob(L, index++, "Texture");
    TextureData* textureData = lovrTextureDataFromBlob(blob);
    lovrRelease(&blob->ref);
    Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true);
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
    lovrRelease(&texture->ref);
  } else if (lua_isuserdata(L, index)) {
    Texture* texture = luax_checktype(L, index++, Texture);
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
  }

  if (lua_isnumber(L, index)) {
    Color color = luax_checkcolor(L, index);
    lovrMaterialSetColor(material, COLOR_DIFFUSE, color);
  }

  luax_pushtype(L, Material, material);
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
    char* vertex = lovrMeshMap(mesh, 0, count, false, true);

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
            case MESH_FLOAT:
              *((float*) vertex) = luaL_optnumber(L, -1, 0.f);
              vertex += sizeof(float);
              break;

            case MESH_BYTE:
              *((uint8_t*) vertex) = luaL_optint(L, -1, 255);
              vertex += sizeof(uint8_t);
              break;

            case MESH_INT:
              *((int*) vertex) = luaL_optint(L, -1, 0);
              vertex += sizeof(int);
              break;
          }
          lua_pop(L, 1);
        }
      }

      lua_pop(L, 1);
    }
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
    if (lua_type(L, 2) == LUA_TSTRING) {
      Blob* blob = luax_readblob(L, 2, "Texture");
      TextureData* textureData = lovrTextureDataFromBlob(blob);
      Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true);
      MaterialData* materialData = lovrMaterialDataCreateEmpty();
      Material* material = lovrMaterialCreate(materialData, false);
      lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
      lovrModelSetMaterial(model, material);
      lovrRelease(&blob->ref);
      lovrRelease(&texture->ref);
      lovrRelease(&material->ref);
    } else {
      lovrModelSetMaterial(model, luax_checktype(L, 2, Material));
    }
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

int l_lovrGraphicsNewTexture(lua_State* L) {
  Blob* blobs[6];
  bool isTable = lua_istable(L, 1);
  int count = isTable ? lua_objlen(L, 1) : lua_gettop(L);

  if (count != 1 && count != 6) {
    return luaL_error(L, "Expected 1 image for a 2D texture or 6 images for a cube texture, got %d", count);
  }

  if (isTable) {
    for (int i = 0; i < count; i++) {
      lua_rawgeti(L, -1, i + 1);
      blobs[i] = luax_readblob(L, -1, "Texture");
      lua_pop(L, 1);
    }
  } else {
    for (int i = 0; i < count; i++) {
      blobs[i] = luax_readblob(L, i + 1, "Texture");
    }
  }

  bool srgb = true;
  if (lua_istable(L, count + 1)) {
    lua_getfield(L, count + 1, "linear");
    srgb = !lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  TextureData* slices[6];
  for (int i = 0; i < count; i++) {
    slices[i] = lovrTextureDataFromBlob(blobs[i]);
    lovrRelease(&blobs[i]->ref);
  }

  TextureType type = (count == 1) ? TEXTURE_2D : TEXTURE_CUBE;
  Texture* texture = lovrTextureCreate(type, slices, count, srgb);

  luax_pushtype(L, Texture, texture);
  lovrRelease(&texture->ref);
  return 1;
}

const luaL_Reg lovrGraphics[] = {
  { "reset", l_lovrGraphicsReset },
  { "clear", l_lovrGraphicsClear },
  { "present", l_lovrGraphicsPresent },
  { "createWindow", l_lovrGraphicsCreateWindow },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "getStats", l_lovrGraphicsGetStats },
  { "getBackgroundColor", l_lovrGraphicsGetBackgroundColor },
  { "setBackgroundColor", l_lovrGraphicsSetBackgroundColor },
  { "getBlendMode", l_lovrGraphicsGetBlendMode },
  { "setBlendMode", l_lovrGraphicsSetBlendMode },
  { "getColor", l_lovrGraphicsGetColor },
  { "setColor", l_lovrGraphicsSetColor },
  { "isCullingEnabled", l_lovrGraphicsIsCullingEnabled },
  { "setCullingEnabled", l_lovrGraphicsSetCullingEnabled },
  { "getDefaultFilter", l_lovrGraphicsGetDefaultFilter },
  { "setDefaultFilter", l_lovrGraphicsSetDefaultFilter },
  { "getDepthTest", l_lovrGraphicsGetDepthTest },
  { "setDepthTest", l_lovrGraphicsSetDepthTest },
  { "getFont", l_lovrGraphicsGetFont },
  { "setFont", l_lovrGraphicsSetFont },
  { "isGammaCorrect", l_lovrGraphicsIsGammaCorrect },
  { "getSystemLimits", l_lovrGraphicsGetSystemLimits },
  { "getLineWidth", l_lovrGraphicsGetLineWidth },
  { "setLineWidth", l_lovrGraphicsSetLineWidth },
  { "getPointSize", l_lovrGraphicsGetPointSize },
  { "setPointSize", l_lovrGraphicsSetPointSize },
  { "getShader", l_lovrGraphicsGetShader },
  { "setShader", l_lovrGraphicsSetShader },
  { "getWinding", l_lovrGraphicsGetWinding },
  { "setWinding", l_lovrGraphicsSetWinding },
  { "isWireframe", l_lovrGraphicsIsWireframe },
  { "setWireframe", l_lovrGraphicsSetWireframe },
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
  { "arc", l_lovrGraphicsArc },
  { "circle", l_lovrGraphicsCircle },
  { "cylinder", l_lovrGraphicsCylinder },
  { "sphere", l_lovrGraphicsSphere },
  { "skybox", l_lovrGraphicsSkybox },
  { "print", l_lovrGraphicsPrint },
  { "newAnimator", l_lovrGraphicsNewAnimator },
  { "newCanvas", l_lovrGraphicsNewCanvas },
  { "newFont", l_lovrGraphicsNewFont },
  { "newMaterial", l_lovrGraphicsNewMaterial },
  { "newMesh", l_lovrGraphicsNewMesh },
  { "newModel", l_lovrGraphicsNewModel },
  { "newShader", l_lovrGraphicsNewShader },
  { "newTexture", l_lovrGraphicsNewTexture },
  { NULL, NULL }
};
