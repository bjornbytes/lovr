#include "api.h"
#include "api/data.h"
#include "api/graphics.h"
#include "api/math.h"
#include "graphics/graphics.h"
#include "graphics/animator.h"
#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "data/textureData.h"
#include "data/vertexData.h"
#include "filesystem/filesystem.h"
#include "util.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>

const char* ArcModes[] = {
  [ARC_MODE_PIE] = "pie",
  [ARC_MODE_OPEN] = "open",
  [ARC_MODE_CLOSED] = "closed",
  NULL
};

const char* AttributeTypes[] = {
  [ATTR_FLOAT] = "float",
  [ATTR_BYTE] = "byte",
  [ATTR_INT] = "int",
  NULL
};

const char* BlendAlphaModes[] = {
  [BLEND_ALPHA_MULTIPLY] = "alphamultiply",
  [BLEND_PREMULTIPLIED] = "premultiplied",
  NULL
};

const char* BlendModes[] = {
  [BLEND_ALPHA] = "alpha",
  [BLEND_ADD] = "add",
  [BLEND_SUBTRACT] = "subtract",
  [BLEND_MULTIPLY] = "multiply",
  [BLEND_LIGHTEN] = "lighten",
  [BLEND_DARKEN] = "darken",
  [BLEND_SCREEN] = "screen",
  NULL
};

const char* BufferUsages[] = {
  [USAGE_STATIC] = "static",
  [USAGE_DYNAMIC] = "dynamic",
  [USAGE_STREAM] = "stream",
  NULL
};

const char* CompareModes[] = {
  [COMPARE_NONE] = "always",
  [COMPARE_EQUAL] = "equal",
  [COMPARE_NEQUAL] = "notequal",
  [COMPARE_LESS] = "less",
  [COMPARE_LEQUAL] = "lequal",
  [COMPARE_GREATER] = "greater",
  [COMPARE_GEQUAL] = "gequal",
  NULL
};

const char* DrawModes[] = {
  [DRAW_POINTS] = "points",
  [DRAW_LINES] = "lines",
  [DRAW_LINE_STRIP] = "linestrip",
  [DRAW_LINE_LOOP] = "lineloop",
  [DRAW_TRIANGLE_STRIP] = "strip",
  [DRAW_TRIANGLES] = "triangles",
  [DRAW_TRIANGLE_FAN] = "fan",
  NULL
};

const char* DrawStyles[] = {
  [STYLE_FILL] = "fill",
  [STYLE_LINE] = "line",
  NULL
};

const char* FilterModes[] = {
  [FILTER_NEAREST] = "nearest",
  [FILTER_BILINEAR] = "bilinear",
  [FILTER_TRILINEAR] = "trilinear",
  [FILTER_ANISOTROPIC] = "anisotropic",
  NULL
};

const char* HorizontalAligns[] = {
  [ALIGN_LEFT] = "left",
  [ALIGN_CENTER] = "center",
  [ALIGN_RIGHT] = "right",
  NULL
};

const char* MaterialColors[] = {
  [COLOR_DIFFUSE] = "diffuse",
  [COLOR_EMISSIVE] = "emissive",
  NULL
};

const char* MaterialScalars[] = {
  [SCALAR_METALNESS] = "metalness",
  [SCALAR_ROUGHNESS] = "roughness",
  NULL
};

const char* MaterialTextures[] = {
  [TEXTURE_DIFFUSE] = "diffuse",
  [TEXTURE_EMISSIVE] = "emissive",
  [TEXTURE_METALNESS] = "metalness",
  [TEXTURE_ROUGHNESS] = "roughness",
  [TEXTURE_OCCLUSION] = "occlusion",
  [TEXTURE_NORMAL] = "normal",
  [TEXTURE_ENVIRONMENT_MAP] = "environment",
  NULL
};

const char* ShaderTypes[] = {
  [SHADER_GRAPHICS] = "graphics",
  [SHADER_COMPUTE] = "compute",
  NULL
};

const char* StencilActions[] = {
  [STENCIL_REPLACE] = "replace",
  [STENCIL_INCREMENT] = "increment",
  [STENCIL_DECREMENT] = "decrement",
  [STENCIL_INCREMENT_WRAP] = "incrementwrap",
  [STENCIL_DECREMENT_WRAP] = "decrementwrap",
  [STENCIL_INVERT] = "invert",
  NULL
};

const char* TextureFormats[] = {
  [FORMAT_RGB] = "rgb",
  [FORMAT_RGBA] = "rgba",
  [FORMAT_RGBA4] = "rgba4",
  [FORMAT_RGBA16F] = "rgba16f",
  [FORMAT_RGBA32F] = "rgba32f",
  [FORMAT_R16F] = "r16f",
  [FORMAT_R32F] = "r32f",
  [FORMAT_RG16F] = "rg16f",
  [FORMAT_RG32F] = "rg32f",
  [FORMAT_RGB5A1] = "rgb5a1",
  [FORMAT_RGB10A2] = "rgb10a2",
  [FORMAT_RG11B10F] = "rg11b10f",
  [FORMAT_D16] = "d16",
  [FORMAT_D32F] = "d32f",
  [FORMAT_D24S8] = "d24s8",
  [FORMAT_DXT1] = "dxt1",
  [FORMAT_DXT3] = "dxt3",
  [FORMAT_DXT5] = "dxt5",
  NULL
};

const char* TextureTypes[] = {
  [TEXTURE_2D] = "2d",
  [TEXTURE_ARRAY] = "array",
  [TEXTURE_CUBE] = "cube",
  [TEXTURE_VOLUME] = "volume",
  NULL
};

const char* UniformAccesses[] = {
  [ACCESS_READ] = "read",
  [ACCESS_WRITE] = "write",
  [ACCESS_READ_WRITE] = "readwrite"
};

const char* VerticalAligns[] = {
  [ALIGN_TOP] = "top",
  [ALIGN_MIDDLE] = "middle",
  [ALIGN_BOTTOM] = "bottom",
  NULL
};

const char* Windings[] = {
  [WINDING_CLOCKWISE] = "clockwise",
  [WINDING_COUNTERCLOCKWISE] = "counterclockwise",
  NULL
};

const char* WrapModes[] = {
  [WRAP_CLAMP] = "clamp",
  [WRAP_REPEAT] = "repeat",
  [WRAP_MIRRORED_REPEAT] = "mirroredrepeat",
  NULL
};

static uint32_t luax_getvertexcount(lua_State* L, int index) {
  int type = lua_type(L, index);
  if (type == LUA_TTABLE) {
    size_t count = lua_objlen(L, index);
    lua_rawgeti(L, index, 1);
    int tableType = lua_type(L, -1);
    lua_pop(L, 1);
    return tableType == LUA_TNUMBER ? count / 3 : count;
  } else if (type == LUA_TNUMBER) {
    return (lua_gettop(L) - index + 1) / 3;
  } else {
    return lua_gettop(L) - index + 1;
  }
}

static void luax_readvertices(lua_State* L, int index, float* vertices, uint32_t count) {
  switch (lua_type(L, index)) {
    case LUA_TTABLE:
      lua_rawgeti(L, index, 1);
      if (lua_type(L, -1) == LUA_TNUMBER) {
        lua_pop(L, 1);
        for (uint32_t i = 0; i < count; i++) {
          for (int j = 1; j <= 3; j++) {
            lua_rawgeti(L, index, 3 * i + j);
            vertices[j] = lua_tonumber(L, -1);
            lua_pop(L, 1);
          }
          vertices += 8;
        }
      } else {
        lua_pop(L, 1);
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, index, i + 1);
          vec3_init(vertices, luax_checkmathtype(L, -1, MATH_VEC3, NULL));
          lua_pop(L, 1);
          vertices += 8;
        }
      }
      break;

    case LUA_TNUMBER:
      for (uint32_t i = 0; i < count; i++) {
        for (int j = 0; j < 3; j++) {
          vertices[j] = lua_tonumber(L, index + 3 * i + j);
        }
        vertices += 8;
      }
      break;

    default:
      for (uint32_t i = 0; i < count; i++) {
        vec3_init(vertices, luax_checkmathtype(L, index + i, MATH_VEC3, NULL));
        vertices += 8;
      }
      break;
  }
}

static void stencilCallback(void* userdata) {
  lua_State* L = userdata;
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_call(L, 0, 0);
}

static TextureData* luax_checktexturedata(lua_State* L, int index, bool flip) {
  TextureData* textureData = luax_totype(L, index, TextureData);

  if (!textureData) {
    Blob* blob = luax_readblob(L, index, "Texture");
    textureData = lovrTextureDataCreateFromBlob(blob, flip);
    lovrRelease(blob);
  }

  return textureData;
}

// Base

static int l_lovrGraphicsPresent(lua_State* L) {
  lovrGraphicsPresent();
  return 0;
}

static int l_lovrGraphicsSetWindow(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrGraphicsSetWindow(NULL);
    return 0;
  }

  WindowFlags flags = { 0 };
  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "width");
  flags.width = luaL_optinteger(L, -1, 1080);
  lua_pop(L, 1);

  lua_getfield(L, 1, "height");
  flags.height = luaL_optinteger(L, -1, 600);
  lua_pop(L, 1);

  lua_getfield(L, 1, "fullscreen");
  flags.fullscreen = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "msaa");
  flags.msaa = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "title");
  flags.title = luaL_optstring(L, -1, "LÖVR");
  lua_pop(L, 1);

  lua_getfield(L, 1, "icon");
  TextureData* textureData = NULL;
  if (!lua_isnil(L, -1)) {
    textureData = luax_checktexturedata(L, -1, true);
    flags.icon.data = textureData->blob.data;
    flags.icon.width = textureData->width;
    flags.icon.height = textureData->height;
  }
  lua_pop(L, 1);

  lovrGraphicsSetWindow(&flags);
  luax_atexit(L, lovrGraphicsDestroy); // The lua_State that creates the window shall be the one to destroy it
  lovrRelease(textureData);
  return 0;
}

static int l_lovrGraphicsGetWidth(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  return 1;
}

static int l_lovrGraphicsGetHeight(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 1;
}

static int l_lovrGraphicsGetDimensions(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 2;
}

static int l_lovrGraphicsGetSupported(lua_State* L) {
  const GpuFeatures* features = lovrGraphicsGetSupported();
  lua_newtable(L);
  lua_pushboolean(L, features->computeShaders);
  lua_setfield(L, -2, "computeshaders");
  lua_pushboolean(L, features->singlepass);
  lua_setfield(L, -2, "singlepass");
  return 1;
}

static int l_lovrGraphicsGetSystemLimits(lua_State* L) {
  const GpuLimits* limits = lovrGraphicsGetLimits();
  lua_newtable(L);
  lua_pushnumber(L, limits->pointSizes[1]);
  lua_setfield(L, -2, "pointsize");
  lua_pushinteger(L, limits->textureSize);
  lua_setfield(L, -2, "texturesize");
  lua_pushinteger(L, limits->textureMSAA);
  lua_setfield(L, -2, "texturemsaa");
  lua_pushinteger(L, limits->textureAnisotropy);
  lua_setfield(L, -2, "anisotropy");
  lua_pushinteger(L, limits->blockSize);
  lua_setfield(L, -2, "blocksize");
  return 1;
}

static int l_lovrGraphicsGetStats(lua_State* L) {
  if (lua_gettop(L) > 0) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
  } else {
    lua_createtable(L, 0, 2);
  }

  const GpuStats* stats = lovrGraphicsGetStats();
  lua_pushinteger(L, stats->drawCalls);
  lua_setfield(L, 1, "drawcalls");
  lua_pushinteger(L, stats->shaderSwitches);
  lua_setfield(L, 1, "shaderswitches");
  return 1;
}

// State

static int l_lovrGraphicsReset(lua_State* L) {
  lovrGraphicsReset();
  return 0;
}

static int l_lovrGraphicsGetAlphaSampling(lua_State* L) {
  lua_pushboolean(L, lovrGraphicsGetAlphaSampling());
  return 1;
}

static int l_lovrGraphicsSetAlphaSampling(lua_State* L) {
  lovrGraphicsSetAlphaSampling(lua_toboolean(L, 1));
  return 0;
}

static int l_lovrGraphicsGetBackgroundColor(lua_State* L) {
  Color color = lovrGraphicsGetBackgroundColor();
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

static int l_lovrGraphicsSetBackgroundColor(lua_State* L) {
  Color color;
  color.r = luaL_checknumber(L, 1);
  color.g = luaL_checknumber(L, 2);
  color.b = luaL_checknumber(L, 3);
  color.a = luaL_optnumber(L, 4, 1.);
  lovrGraphicsSetBackgroundColor(color);
  return 0;
}

static int l_lovrGraphicsGetBlendMode(lua_State* L) {
  BlendMode mode;
  BlendAlphaMode alphaMode;
  lovrGraphicsGetBlendMode(&mode, &alphaMode);
  lua_pushstring(L, BlendModes[mode]);
  lua_pushstring(L, BlendAlphaModes[alphaMode]);
  return 2;
}

static int l_lovrGraphicsSetBlendMode(lua_State* L) {
  BlendMode mode = lua_isnoneornil(L, 1) ? BLEND_NONE : luaL_checkoption(L, 1, NULL, BlendModes);
  BlendAlphaMode alphaMode = luaL_checkoption(L, 2, "alphamultiply", BlendAlphaModes);
  lovrGraphicsSetBlendMode(mode, alphaMode);
  return 0;
}

static int l_lovrGraphicsGetCanvas(lua_State* L) {
  Canvas* canvas = lovrGraphicsGetCanvas();
  luax_pushobject(L, canvas);
  return 1;
}

static int l_lovrGraphicsSetCanvas(lua_State* L) {
  Canvas* canvas = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Canvas);
  lovrGraphicsSetCanvas(canvas);
  return 0;
}

static int l_lovrGraphicsGetColor(lua_State* L) {
  Color color = lovrGraphicsGetColor();
  lua_pushnumber(L, color.r);
  lua_pushnumber(L, color.g);
  lua_pushnumber(L, color.b);
  lua_pushnumber(L, color.a);
  return 4;
}

static int l_lovrGraphicsSetColor(lua_State* L) {
  Color color = luax_checkcolor(L, 1);
  lovrGraphicsSetColor(color);
  return 0;
}

static int l_lovrGraphicsIsCullingEnabled(lua_State* L) {
  lua_pushboolean(L, lovrGraphicsIsCullingEnabled());
  return 1;
}

static int l_lovrGraphicsSetCullingEnabled(lua_State* L) {
  lovrGraphicsSetCullingEnabled(lua_toboolean(L, 1));
  return 0;
}

static int l_lovrGraphicsGetDefaultFilter(lua_State* L) {
  TextureFilter filter = lovrGraphicsGetDefaultFilter();
  lua_pushstring(L, FilterModes[filter.mode]);
  if (filter.mode == FILTER_ANISOTROPIC) {
    lua_pushnumber(L, filter.anisotropy);
    return 2;
  }
  return 1;
}

static int l_lovrGraphicsSetDefaultFilter(lua_State* L) {
  FilterMode mode = luaL_checkoption(L, 1, NULL, FilterModes);
  float anisotropy = luaL_optnumber(L, 2, 1.);
  lovrGraphicsSetDefaultFilter((TextureFilter) { .mode = mode, .anisotropy = anisotropy });
  return 0;
}

static int l_lovrGraphicsGetDepthTest(lua_State* L) {
  CompareMode mode;
  bool write;
  lovrGraphicsGetDepthTest(&mode, &write);
  lua_pushstring(L, CompareModes[mode]);
  lua_pushboolean(L, write);
  return 2;
}

static int l_lovrGraphicsSetDepthTest(lua_State* L) {
  CompareMode mode = lua_isnoneornil(L, 1) ? COMPARE_NONE : luaL_checkoption(L, 1, NULL, CompareModes);
  bool write = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
  lovrGraphicsSetDepthTest(mode, write);
  return 0;
}

static int l_lovrGraphicsGetFont(lua_State* L) {
  Font* font = lovrGraphicsGetFont();
  luax_pushobject(L, font);
  return 1;
}

static int l_lovrGraphicsSetFont(lua_State* L) {
  Font* font = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Font);
  lovrGraphicsSetFont(font);
  return 0;
}

static int l_lovrGraphicsIsGammaCorrect(lua_State* L) {
  bool gammaCorrect = lovrGraphicsIsGammaCorrect();
  lua_pushboolean(L, gammaCorrect);
  return 1;
}

static int l_lovrGraphicsGetLineWidth(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetLineWidth());
  return 1;
}

static int l_lovrGraphicsSetLineWidth(lua_State* L) {
  uint8_t width = (uint8_t) luaL_optinteger(L, 1, 1);
  lovrGraphicsSetLineWidth(width);
  return 0;
}

static int l_lovrGraphicsGetPointSize(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetPointSize());
  return 1;
}

static int l_lovrGraphicsSetPointSize(lua_State* L) {
  float size = luaL_optnumber(L, 1, 1.f);
  lovrGraphicsSetPointSize(size);
  return 0;
}

static int l_lovrGraphicsGetShader(lua_State* L) {
  Shader* shader = lovrGraphicsGetShader();
  luax_pushobject(L, shader);
  return 1;
}

static int l_lovrGraphicsSetShader(lua_State* L) {
  Shader* shader = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Shader);
  lovrGraphicsSetShader(shader);
  return 0;
}

static int l_lovrGraphicsGetStencilTest(lua_State* L) {
  CompareMode mode;
  int value;
  lovrGraphicsGetStencilTest(&mode, &value);

  if (mode == COMPARE_NONE) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushstring(L, CompareModes[mode]);
  lua_pushinteger(L, value);
  return 2;
}

static int l_lovrGraphicsSetStencilTest(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrGraphicsSetStencilTest(COMPARE_NONE, 0);
  } else {
    CompareMode mode = luaL_checkoption(L, 1, NULL, CompareModes);
    int value = luaL_checkinteger(L, 2);
    lovrGraphicsSetStencilTest(mode, value);
  }
  return 0;
}

static int l_lovrGraphicsGetWinding(lua_State* L) {
  lua_pushstring(L, Windings[lovrGraphicsGetWinding()]);
  return 1;
}

static int l_lovrGraphicsSetWinding(lua_State* L) {
  lovrGraphicsSetWinding(luaL_checkoption(L, 1, NULL, Windings));
  return 0;
}

static int l_lovrGraphicsIsWireframe(lua_State* L) {
  lua_pushboolean(L, lovrGraphicsIsWireframe());
  return 1;
}

static int l_lovrGraphicsSetWireframe(lua_State* L) {
  lovrGraphicsSetWireframe(lua_toboolean(L, 1));
  return 0;
}

// Transforms

static int l_lovrGraphicsPush(lua_State* L) {
  lovrGraphicsPush();
  return 0;
}

static int l_lovrGraphicsPop(lua_State* L) {
  lovrGraphicsPop();
  return 0;
}

static int l_lovrGraphicsOrigin(lua_State* L) {
  lovrGraphicsOrigin();
  return 0;
}

static int l_lovrGraphicsTranslate(lua_State* L) {
  float translation[3];
  luax_readvec3(L, 1, translation, NULL);
  lovrGraphicsTranslate(translation);
  return 0;
}

static int l_lovrGraphicsRotate(lua_State* L) {
  float rotation[4];
  luax_readquat(L, 1, rotation, NULL);
  lovrGraphicsRotate(rotation);
  return 0;
}

static int l_lovrGraphicsScale(lua_State* L) {
  float scale[3];
  luax_readscale(L, 1, scale, 3, NULL);
  lovrGraphicsScale(scale);
  return 0;
}

static int l_lovrGraphicsTransform(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 3, NULL);
  lovrGraphicsMatrixTransform(transform);
  return 0;
}

static int l_lovrGraphicsSetProjection(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 3, NULL);
  lovrGraphicsSetProjection(transform);
  return 0;
}

// Rendering

static int l_lovrGraphicsClear(lua_State* L) {
  int index = 1;
  int top = lua_gettop(L);

  bool clearColor = true;
  bool clearDepth = true;
  bool clearStencil = true;
  Color color = lovrGraphicsGetBackgroundColor();
  float depth = 1.f;
  int stencil = 0;

  if (top >= index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
      color.r = luaL_checknumber(L, index++);
      color.g = luaL_checknumber(L, index++);
      color.b = luaL_checknumber(L, index++);
      color.a = luaL_optnumber(L, index++, 1.);
    } else {
      clearColor = lua_toboolean(L, index++);
    }
  }

  if (top >= index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
      depth = luaL_checknumber(L, index++);
    } else {
      clearDepth = lua_toboolean(L, index++);
    }
  }

  if (top >= index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
      stencil = luaL_checkinteger(L, index++);
    } else {
      clearStencil = lua_toboolean(L, index++);
    }
  }

  lovrGraphicsClear(clearColor ? &color : NULL, clearDepth ? &depth : NULL, clearStencil ? &stencil : NULL);
  return 0;
}

static int l_lovrGraphicsDiscard(lua_State* L) {
  int top = lua_gettop(L);
  bool color = top >= 1 ? lua_toboolean(L, 1) : true;
  bool depth = top >= 2 ? lua_toboolean(L, 2) : true;
  bool stencil = top >= 3 ? lua_toboolean(L, 3) : true;
  lovrGraphicsDiscard(color, depth, stencil);
  return 0;
}

static int l_lovrGraphicsFlush(lua_State* L) {
  lovrGraphicsFlush();
  return 0;
}

static int l_lovrGraphicsPoints(lua_State* L) {
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 1);
  lovrGraphicsPoints(count, &vertices);
  luax_readvertices(L, 1, vertices, count);
  return 0;
}

static int l_lovrGraphicsLine(lua_State* L) {
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 1);
  lovrGraphicsLine(count, &vertices);
  luax_readvertices(L, 1, vertices, count);
  return 0;
}

static int l_lovrGraphicsTriangle(lua_State* L) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luaL_checkoption(L, 1, NULL, DrawStyles);
  }

  float* vertices;
  uint32_t count = luax_getvertexcount(L, 2);
  lovrAssert(count % 3 == 0, "Triangle vertex count must be a multiple of 3");
  lovrGraphicsTriangle(style, material, count, &vertices);
  luax_readvertices(L, 2, vertices, count);
  return 0;
}

static int l_lovrGraphicsPlane(lua_State* L) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luaL_checkoption(L, 1, NULL, DrawStyles);
  }
  float transform[16];
  luax_readmat4(L, 2, transform, 2, NULL);
  lovrGraphicsPlane(style, material, transform);
  return 0;
}

static int luax_rectangularprism(lua_State* L, int scaleComponents) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luaL_checkoption(L, 1, NULL, DrawStyles);
  }
  float transform[16];
  luax_readmat4(L, 2, transform, scaleComponents, NULL);
  lovrGraphicsBox(style, material, transform);
  return 0;
}

static int l_lovrGraphicsCube(lua_State* L) {
  return luax_rectangularprism(L, 1);
}

static int l_lovrGraphicsBox(lua_State* L) {
  return luax_rectangularprism(L, 3);
}

static int l_lovrGraphicsArc(lua_State* L) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luaL_checkoption(L, 1, NULL, DrawStyles);
  }
  ArcMode mode = ARC_MODE_PIE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    mode = luaL_checkoption(L, index++, NULL, ArcModes);
  }
  float transform[16];
  index = luax_readmat4(L, index, transform, 1, NULL);
  float r1 = luaL_optnumber(L, index++, 0);
  float r2 = luaL_optnumber(L, index++, 2 * M_PI);
  int segments = luaL_optinteger(L, index, 64) * (MIN(fabsf(r2 - r1), 2 * M_PI) / (2 * M_PI));
  lovrGraphicsArc(style, mode, material, transform, r1, r2, segments);
  return 0;
}

static int l_lovrGraphicsCircle(lua_State* L) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luaL_checkoption(L, 1, NULL, DrawStyles);
  }
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1, NULL);
  int segments = luaL_optnumber(L, index, 32);
  lovrGraphicsCircle(style, material, transform, segments);
  return 0;
}

static int l_lovrGraphicsCylinder(lua_State* L) {
  float transform[16];
  int index = 1;
  Material* material = lua_isuserdata(L, index) ? luax_checktype(L, index++, Material) : NULL;
  index = luax_readmat4(L, index, transform, 1, NULL);
  float r1 = luaL_optnumber(L, index++, 1.);
  float r2 = luaL_optnumber(L, index++, 1.);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  int segments = luaL_optnumber(L, index, floorf(16 + 16 * MAX(r1, r2)));
  lovrGraphicsCylinder(material, transform, r1, r2, capped, segments);
  return 0;
}

static int l_lovrGraphicsSphere(lua_State* L) {
  float transform[16];
  int index = 1;
  Material* material = lua_isuserdata(L, index) ? luax_checktype(L, index++, Material) : NULL;
  index = luax_readmat4(L, index, transform, 1, NULL);
  int segments = luaL_optnumber(L, index, 30);
  lovrGraphicsSphere(material, transform, segments);
  return 0;
}

static int l_lovrGraphicsSkybox(lua_State* L) {
  Texture* texture = luax_checktexture(L, 1);
  float angle = luaL_optnumber(L, 2, 0);
  float ax = luaL_optnumber(L, 3, 0);
  float ay = luaL_optnumber(L, 4, 1);
  float az = luaL_optnumber(L, 5, 0);
  lovrGraphicsSkybox(texture, angle, ax, ay, az);
  return 0;
}

static int l_lovrGraphicsPrint(lua_State* L) {
  size_t length;
  const char* str = luaL_checklstring(L, 1, &length);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1, NULL);
  float wrap = luaL_optnumber(L, index++, 0);
  HorizontalAlign halign = luaL_checkoption(L, index++, "center", HorizontalAligns);
  VerticalAlign valign = luaL_checkoption(L, index++, "middle", VerticalAligns);
  lovrGraphicsPrint(str, length, transform, wrap, halign, valign);
  return 0;
}

static int l_lovrGraphicsStencil(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  StencilAction action = luaL_checkoption(L, 2, "replace", StencilActions);
  int replaceValue = luaL_optinteger(L, 3, 1);
  bool keepValues = lua_toboolean(L, 4);
  if (!keepValues) {
    int clearTo = lua_isnumber(L, 4) ? lua_tonumber(L, 4) : 0;
    lovrGraphicsClear(NULL, NULL, &clearTo);
  }
  lua_settop(L, 1);
  lovrGraphicsStencil(action, replaceValue, stencilCallback, L);
  return 0;
}

static int l_lovrGraphicsFill(lua_State* L) {
  Texture* texture = lua_isnoneornil(L, 1) ? NULL : luax_checktexture(L, 1);
  float u = luaL_optnumber(L, 2, 0.);
  float v = luaL_optnumber(L, 3, 0.);
  float w = luaL_optnumber(L, 4, 1. - u);
  float h = luaL_optnumber(L, 5, 1. - v);
  lovrGraphicsFill(texture, u, v, w, h);
  return 0;
}

static int l_lovrGraphicsCompute(lua_State* L) {
  Shader* shader = luax_checktype(L, 1, Shader);
  int x = luaL_optinteger(L, 2, 1);
  int y = luaL_optinteger(L, 3, 1);
  int z = luaL_optinteger(L, 4, 1);
  lovrGraphicsCompute(shader, x, y, z);
  return 0;
}

// Types

static int l_lovrGraphicsNewAnimator(lua_State* L) {
  Model* model = luax_checktype(L, 1, Model);
  Animator* animator = lovrAnimatorCreate(model->modelData);
  luax_pushobject(L, animator);
  lovrRelease(animator);
  return 1;
}

static int l_lovrGraphicsNewShaderBlock(lua_State* L) {
  vec_uniform_t uniforms;
  vec_init(&uniforms);

  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushnil(L);
  while (lua_next(L, 1) != 0) {
    Uniform uniform;

    // Name
    strncpy(uniform.name, luaL_checkstring(L, -2), LOVR_MAX_UNIFORM_LENGTH - 1);

    if (lua_type(L, -1) == LUA_TSTRING) {
      uniform.count = 1;
      luax_checkuniformtype(L, -1, &uniform.type, &uniform.components);
    } else {
      luaL_checktype(L, -1, LUA_TTABLE);

      lua_rawgeti(L, -1, 1);
      luax_checkuniformtype(L, -1, &uniform.type, &uniform.components);
      lua_pop(L, 1);

      lua_rawgeti(L, -1, 2);
      uniform.count = luaL_optinteger(L, -1, 1);
      lua_pop(L, 1);
    }

    lovrAssert(uniform.count >= 1, "Uniform count must be positive, got %d for '%s'", uniform.count, uniform.name);
    vec_push(&uniforms, uniform);

    // Pop the table, leaving the key for lua_next to nom
    lua_pop(L, 1);
  }

  BlockType type = BLOCK_UNIFORM;
  BufferUsage usage = USAGE_DYNAMIC;

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "usage");
    usage = luaL_checkoption(L, -1, "dynamic", BufferUsages);
    lua_pop(L, 1);

    lua_getfield(L, 2, "writable");
    type = lua_toboolean(L, -1) ? BLOCK_STORAGE : BLOCK_UNIFORM;
    lua_pop(L, 1);
  }

  lovrAssert(type != BLOCK_STORAGE || lovrGraphicsGetSupported()->computeShaders, "Writable ShaderBlocks are not supported on this system");
  size_t size = lovrShaderComputeUniformLayout(&uniforms);
  Buffer* buffer = lovrBufferCreate(size, NULL, type == BLOCK_STORAGE ? BUFFER_SHADER_STORAGE : BUFFER_UNIFORM, usage, false);
  ShaderBlock* block = lovrShaderBlockCreate(type, buffer, &uniforms);
  luax_pushobject(L, block);
  vec_deinit(&uniforms);
  lovrRelease(buffer);
  lovrRelease(block);
  return 1;
}

static int l_lovrGraphicsNewCanvas(lua_State* L) {
  Attachment attachments[MAX_CANVAS_ATTACHMENTS];
  int attachmentCount = 0;
  int width = 0;
  int height = 0;
  int index;

  if (luax_totype(L, 1, Texture)) {
    for (index = 1; index <= MAX_CANVAS_ATTACHMENTS; index++) {
      Texture* texture = luax_totype(L, index, Texture);
      if (!texture) break;
      attachments[attachmentCount++] = (Attachment) { texture, 0, 0 };
    }
  } else if (lua_istable(L, 1)) {
    luax_readattachments(L, 1, attachments, &attachmentCount);
    index = 2;
  } else {
    width = luaL_checkinteger(L, 1);
    height = luaL_checkinteger(L, 2);
    index = 3;
  }

  CanvasFlags flags = {
    .depth = { .enabled = true, .readable = false, .format = FORMAT_D16 },
    .stereo = true,
    .msaa = 0,
    .mipmaps = true
  };

  TextureFormat format = FORMAT_RGBA;
  bool anonymous = attachmentCount == 0;

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "depth");
    switch (lua_type(L, -1)) {
      case LUA_TNIL: break;
      case LUA_TBOOLEAN: flags.depth.enabled = lua_toboolean(L, -1); break;
      case LUA_TSTRING: flags.depth.format = luaL_checkoption(L, -1, NULL, TextureFormats); break;
      case LUA_TTABLE:
        lua_getfield(L, -1, "readable");
        flags.depth.readable = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "format");
        flags.depth.format = luaL_checkoption(L, -1, NULL, TextureFormats);
        lua_pop(L, 1);
        break;
      default: lovrThrow("Expected boolean, string, or table for Canvas depth flag");
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "stereo");
    flags.stereo = lua_isnil(L, -1) ? flags.stereo : lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "msaa");
    flags.msaa = lua_isnil(L, -1) ? flags.msaa : luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    flags.mipmaps = lua_isnil(L, -1) ? flags.mipmaps : lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (attachmentCount == 0) {
      lua_getfield(L, index, "format");
      format = luaL_checkoption(L, -1, "rgba", TextureFormats);
      anonymous = lua_isnil(L, -1) || lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
  }

  if (anonymous) {
    Texture* texture = lovrTextureCreate(TEXTURE_2D, NULL, 0, true, flags.mipmaps, flags.msaa);
    lovrTextureAllocate(texture, width, height, 1, format);
    lovrTextureSetWrap(texture, (TextureWrap) { .s = WRAP_CLAMP, .t = WRAP_CLAMP, .r = WRAP_CLAMP });
    attachments[0] = (Attachment) { texture, 0, 0 };
    attachmentCount++;
  }

  if (width == 0 && height == 0 && attachmentCount > 0) {
    width = lovrTextureGetWidth(attachments[0].texture, attachments[0].level);
    height = lovrTextureGetHeight(attachments[0].texture, attachments[0].level);
  }

  Canvas* canvas = lovrCanvasCreate(width, height, flags);

  if (attachmentCount > 0) {
    lovrCanvasSetAttachments(canvas, attachments, attachmentCount);
    if (anonymous) {
      lovrRelease(attachments[0].texture);
    }
  }

  luax_pushobject(L, canvas);
  lovrRelease(canvas);
  return 1;
}

static int l_lovrGraphicsNewFont(lua_State* L) {
  Rasterizer* rasterizer = luax_totype(L, 1, Rasterizer);

  if (!rasterizer) {
    Blob* blob = NULL;
    float size;

    if (lua_type(L, 1) == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
      size = luaL_optnumber(L, 1, 32);
    } else {
      blob = luax_readblob(L, 1, "Font");
      size = luaL_optnumber(L, 2, 32);
    }

    rasterizer = lovrRasterizerCreate(blob, size);
    lovrRelease(blob);
  }

  Font* font = lovrFontCreate(rasterizer);
  luax_pushobject(L, font);
  lovrRelease(rasterizer);
  lovrRelease(font);
  return 1;
}

static int l_lovrGraphicsNewMaterial(lua_State* L) {
  Material* material = lovrMaterialCreate();

  int index = 1;

  if (lua_type(L, index) == LUA_TSTRING) {
    Blob* blob = luax_readblob(L, index++, "Texture");
    TextureData* textureData = lovrTextureDataCreateFromBlob(blob, true);
    Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, true, 0);
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
    lovrRelease(blob);
    lovrRelease(textureData);
    lovrRelease(texture);
  } else if (lua_isuserdata(L, index)) {
    Texture* texture = luax_checktexture(L, index);
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
    index++;
  }

  if (lua_isnumber(L, index)) {
    Color color = luax_checkcolor(L, index);
    lovrMaterialSetColor(material, COLOR_DIFFUSE, color);
  }

  luax_pushobject(L, material);
  lovrRelease(material);
  return 1;
}

static int l_lovrGraphicsNewMesh(lua_State* L) {
  uint32_t count;
  int dataIndex = 0;
  int drawModeIndex = 2;
  VertexData* vertexData = NULL;
  bool hasFormat = false;
  VertexFormat format;
  vertexFormatInit(&format);

  if (lua_isnumber(L, 1)) {
    count = lua_tointeger(L, 1);
  } else if (lua_istable(L, 1)) {
    if (lua_isnumber(L, 2)) {
      drawModeIndex++;
      hasFormat = luax_checkvertexformat(L, 1, &format);
      count = lua_tointeger(L, 2);
      dataIndex = 0;
    } else if (lua_istable(L, 2)) {
      drawModeIndex++;
      hasFormat = luax_checkvertexformat(L, 1, &format);
      count = lua_objlen(L, 2);
      dataIndex = 2;
    } else {
      count = lua_objlen(L, 1);
      dataIndex = 1;
    }
  } else if (lua_isuserdata(L, 1)) {
    vertexData = luax_checktype(L, 1, VertexData);
    format = vertexData->format;
    count = vertexData->count;
    hasFormat = true;
  } else {
    luaL_argerror(L, 1, "table or number expected");
    return 0;
  }

  if (!hasFormat) {
    vertexFormatAppend(&format, "lovrPosition", ATTR_FLOAT, 3);
    vertexFormatAppend(&format, "lovrNormal", ATTR_FLOAT, 3);
    vertexFormatAppend(&format, "lovrTexCoord", ATTR_FLOAT, 2);
  }

  DrawMode mode = luaL_checkoption(L, drawModeIndex, "fan", DrawModes);
  BufferUsage usage = luaL_checkoption(L, drawModeIndex + 1, "dynamic", BufferUsages);
  bool readable = lua_toboolean(L, drawModeIndex + 2);
  size_t bufferSize = count * format.stride;
  Buffer* vertexBuffer = lovrBufferCreate(bufferSize, NULL, BUFFER_VERTEX, usage, readable);
  Mesh* mesh = lovrMeshCreate(mode, format, vertexBuffer, count);

  lovrMeshAttachAttribute(mesh, "lovrDrawID", &(MeshAttribute) {
    .buffer = lovrGraphicsGetIdentityBuffer(),
    .type = ATTR_BYTE,
    .components = 1,
    .divisor = 1,
    .integer = true,
    .enabled = true
  });

  if (dataIndex) {
    VertexPointer vertices = { .raw = lovrBufferMap(vertexBuffer, 0) };
    luax_loadvertices(L, dataIndex, &format, vertices);
  } else if (vertexData) {
    void* vertices = lovrBufferMap(vertexBuffer, 0);
    memcpy(vertices, vertexData->blob.data, vertexData->count * vertexData->format.stride);
  }

  lovrBufferFlush(vertexBuffer, 0, count * format.stride);
  lovrRelease(vertexBuffer);

  luax_pushobject(L, mesh);
  lovrRelease(mesh);
  return 1;
}

static int l_lovrGraphicsNewModel(lua_State* L) {
  ModelData* modelData = luax_totype(L, 1, ModelData);

  if (!modelData) {
    Blob* blob = luax_readblob(L, 1, "Model");
    modelData = lovrModelDataCreate(blob);
    lovrRelease(blob);
  }

  Model* model = lovrModelCreate(modelData);

  if (lua_gettop(L) >= 2) {
    if (lua_type(L, 2) == LUA_TSTRING) {
      Blob* blob = luax_readblob(L, 2, "Texture");
      TextureData* textureData = lovrTextureDataCreateFromBlob(blob, true);
      Texture* texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, true, true, 0);
      Material* material = lovrMaterialCreate();
      lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
      lovrModelSetMaterial(model, material);
      lovrRelease(blob);
      lovrRelease(texture);
      lovrRelease(material);
    } else {
      lovrModelSetMaterial(model, luax_checktype(L, 2, Material));
    }
  }

  luax_pushobject(L, model);
  lovrRelease(modelData);
  lovrRelease(model);
  return 1;
}

static void luax_readshadersource(lua_State* L, int index) {
  if (lua_isnoneornil(L, index)) {
    return;
  }

  Blob* blob = luax_totype(L, index, Blob);
  if (blob) {
    lua_pushlstring(L, blob->data, blob->size);
    lua_replace(L, index);
    return;
  }

  const char* source = luaL_checkstring(L, index);
  if (!lovrFilesystemIsFile(source)) {
    return;
  }

  size_t bytesRead;
  char* contents = lovrFilesystemRead(source, &bytesRead);
  lovrAssert(bytesRead > 0, "Could not read shader from file '%s'", source);
  lua_pushlstring(L, contents, bytesRead);
  lua_replace(L, index);
  free(contents);
}

static int l_lovrGraphicsNewShader(lua_State* L) {
  luax_readshadersource(L, 1);
  luax_readshadersource(L, 2);
  const char* vertexSource = lua_tostring(L, 1);
  const char* fragmentSource = lua_tostring(L, 2);
  Shader* shader = lovrShaderCreateGraphics(vertexSource, fragmentSource);
  luax_pushobject(L, shader);
  lovrRelease(shader);
  return 1;
}

static int l_lovrGraphicsNewComputeShader(lua_State* L) {
  luax_readshadersource(L, 1);
  const char* source = lua_tostring(L, 1);
  Shader* shader = lovrShaderCreateCompute(source);
  luax_pushobject(L, shader);
  lovrRelease(shader);
  return 1;
}

static int l_lovrGraphicsNewTexture(lua_State* L) {
  int index = 1;
  int width, height, depth;
  int argType = lua_type(L, index);
  bool blank = argType == LUA_TNUMBER;
  TextureType type = TEXTURE_2D;

  if (blank) {
    width = lua_tointeger(L, index++);
    height = luaL_checkinteger(L, index++);
    depth = lua_type(L, index) == LUA_TNUMBER ? lua_tonumber(L, index++) : 0;
    lovrAssert(width > 0 && height > 0, "A Texture must have a positive width, height, and depth");
  } else if (argType != LUA_TTABLE) {
    lua_createtable(L, 1, 0);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);
    lua_replace(L, 1);
    depth = 1;
    index++;
  } else {
    depth = lua_objlen(L, index++);
    type = depth > 0 ? TEXTURE_ARRAY : TEXTURE_CUBE;
  }

  bool hasFlags = lua_istable(L, index);
  bool srgb = !blank;
  bool mipmaps = true;
  TextureFormat format = FORMAT_RGBA;
  int msaa = 0;

  if (hasFlags) {
    lua_getfield(L, index, "linear");
    srgb = lua_isnil(L, -1) ? srgb : !lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    mipmaps = lua_isnil(L, -1) ? mipmaps : lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "type");
    type = lua_isnil(L, -1) ? type : luaL_checkoption(L, -1, NULL, TextureTypes);
    lua_pop(L, 1);

    lua_getfield(L, index, "format");
    format = lua_isnil(L, -1) ? format : luaL_checkoption(L, -1, NULL, TextureFormats);
    lua_pop(L, 1);

    lua_getfield(L, index, "msaa");
    msaa = lua_isnil(L, -1) ? msaa : luaL_checkinteger(L, -1);
    lua_pop(L, 1);
  }

  Texture* texture = lovrTextureCreate(type, NULL, 0, srgb, mipmaps, msaa);
  lovrTextureSetFilter(texture, lovrGraphicsGetDefaultFilter());

  if (blank) {
    depth = depth ? depth : (type == TEXTURE_CUBE ? 6 : 1);
    lovrTextureAllocate(texture, width, height, depth, format);
  } else {
    if (type == TEXTURE_CUBE && depth == 0) {
      depth = 6;
      const char* faces[6] = { "right", "left", "top", "bottom", "back", "front" };
      for (int i = 0; i < 6; i++) {
        lua_pushstring(L, faces[i]);
        lua_rawget(L, 1);
        lua_rawseti(L, 1, i + 1);
      }
    }

    for (int i = 0; i < depth; i++) {
      lua_rawgeti(L, 1, i + 1);
      TextureData* textureData = luax_checktexturedata(L, -1, type != TEXTURE_CUBE);
      if (i == 0) {
        lovrTextureAllocate(texture, textureData->width, textureData->height, depth, textureData->format);
      }
      lovrTextureReplacePixels(texture, textureData, 0, 0, i, 0);
      lovrRelease(textureData);
      lua_pop(L, 1);
    }
  }

  luax_pushobject(L, texture);
  lovrRelease(texture);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {

  // Base
  { "present", l_lovrGraphicsPresent },
  { "setWindow", l_lovrGraphicsSetWindow },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "getSupported", l_lovrGraphicsGetSupported },
  { "getSystemLimits", l_lovrGraphicsGetSystemLimits },
  { "getStats", l_lovrGraphicsGetStats },

  // State
  { "reset", l_lovrGraphicsReset },
  { "getAlphaSampling", l_lovrGraphicsGetAlphaSampling },
  { "setAlphaSampling", l_lovrGraphicsSetAlphaSampling },
  { "getBackgroundColor", l_lovrGraphicsGetBackgroundColor },
  { "setBackgroundColor", l_lovrGraphicsSetBackgroundColor },
  { "getBlendMode", l_lovrGraphicsGetBlendMode },
  { "setBlendMode", l_lovrGraphicsSetBlendMode },
  { "getCanvas", l_lovrGraphicsGetCanvas },
  { "setCanvas", l_lovrGraphicsSetCanvas },
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
  { "getLineWidth", l_lovrGraphicsGetLineWidth },
  { "setLineWidth", l_lovrGraphicsSetLineWidth },
  { "getPointSize", l_lovrGraphicsGetPointSize },
  { "setPointSize", l_lovrGraphicsSetPointSize },
  { "getShader", l_lovrGraphicsGetShader },
  { "setShader", l_lovrGraphicsSetShader },
  { "getStencilTest", l_lovrGraphicsGetStencilTest },
  { "setStencilTest", l_lovrGraphicsSetStencilTest },
  { "getWinding", l_lovrGraphicsGetWinding },
  { "setWinding", l_lovrGraphicsSetWinding },
  { "isWireframe", l_lovrGraphicsIsWireframe },
  { "setWireframe", l_lovrGraphicsSetWireframe },

  // Transforms
  { "push", l_lovrGraphicsPush },
  { "pop", l_lovrGraphicsPop },
  { "origin", l_lovrGraphicsOrigin },
  { "translate", l_lovrGraphicsTranslate },
  { "rotate", l_lovrGraphicsRotate },
  { "scale", l_lovrGraphicsScale },
  { "transform", l_lovrGraphicsTransform },
  { "setProjection", l_lovrGraphicsSetProjection },

  // Rendering
  { "clear", l_lovrGraphicsClear },
  { "discard", l_lovrGraphicsDiscard },
  { "flush", l_lovrGraphicsFlush },
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
  { "stencil", l_lovrGraphicsStencil },
  { "fill", l_lovrGraphicsFill },
  { "compute", l_lovrGraphicsCompute },

  // Types
  { "newAnimator", l_lovrGraphicsNewAnimator },
  { "newCanvas", l_lovrGraphicsNewCanvas },
  { "newFont", l_lovrGraphicsNewFont },
  { "newMaterial", l_lovrGraphicsNewMaterial },
  { "newMesh", l_lovrGraphicsNewMesh },
  { "newModel", l_lovrGraphicsNewModel },
  { "newShader", l_lovrGraphicsNewShader },
  { "newComputeShader", l_lovrGraphicsNewComputeShader },
  { "newShaderBlock", l_lovrGraphicsNewShaderBlock },
  { "newTexture", l_lovrGraphicsNewTexture },

  { NULL, NULL }
};

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrGraphics);
  luax_registertype(L, "Animator", lovrAnimator);
  luax_registertype(L, "Font", lovrFont);
  luax_registertype(L, "Material", lovrMaterial);
  luax_registertype(L, "Mesh", lovrMesh);
  luax_registertype(L, "Model", lovrModel);
  luax_registertype(L, "Shader", lovrShader);
  luax_registertype(L, "ShaderBlock", lovrShaderBlock);
  luax_registertype(L, "Texture", lovrTexture);
  luax_registertype(L, "Canvas", lovrCanvas);

  luax_pushconf(L);

  // Gamma correct
  lua_getfield(L, -1, "gammacorrect");
  bool gammaCorrect = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lovrGraphicsInit(gammaCorrect);

  lua_pushcfunction(L, l_lovrGraphicsSetWindow);
  lua_getfield(L, -2, "window");
  lua_call(L, 1, 0);

  lua_pop(L, 1); // conf
  return 1;
}
