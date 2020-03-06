#include "api.h"
#include "graphics/graphics.h"
#include "graphics/buffer.h"
#include "graphics/canvas.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "graphics/model.h"
#include "graphics/shader.h"
#include "data/blob.h"
#include "data/modelData.h"
#include "data/rasterizer.h"
#include "data/textureData.h"
#include "core/arr.h"
#include "core/ref.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

StringEntry ArcModes[] = {
  [ARC_MODE_PIE] = ENTRY("pie"),
  [ARC_MODE_OPEN] = ENTRY("open"),
  [ARC_MODE_CLOSED] = ENTRY("closed"),
  { 0 }
};

StringEntry AttributeTypes[] = {
  [I8] = ENTRY("byte"),
  [U8] = ENTRY("ubyte"),
  [I16] = ENTRY("short"),
  [U16] = ENTRY("ushort"),
  [I32] = ENTRY("int"),
  [U32] = ENTRY("uint"),
  [F32] = ENTRY("float"),
  { 0 }
};

StringEntry BlendAlphaModes[] = {
  [BLEND_ALPHA_MULTIPLY] = ENTRY("alphamultiply"),
  [BLEND_PREMULTIPLIED] = ENTRY("premultiplied"),
  { 0 }
};

StringEntry BlendModes[] = {
  [BLEND_ALPHA] = ENTRY("alpha"),
  [BLEND_ADD] = ENTRY("add"),
  [BLEND_SUBTRACT] = ENTRY("subtract"),
  [BLEND_MULTIPLY] = ENTRY("multiply"),
  [BLEND_LIGHTEN] = ENTRY("lighten"),
  [BLEND_DARKEN] = ENTRY("darken"),
  [BLEND_SCREEN] = ENTRY("screen"),
  { 0 }
};

StringEntry BlockTypes[] = {
  [BLOCK_UNIFORM] = ENTRY("uniform"),
  [BLOCK_COMPUTE] = ENTRY("compute"),
  { 0 }
};

StringEntry BufferUsages[] = {
  [USAGE_STATIC] = ENTRY("static"),
  [USAGE_DYNAMIC] = ENTRY("dynamic"),
  [USAGE_STREAM] = ENTRY("stream"),
  { 0 }
};

StringEntry CompareModes[] = {
  [COMPARE_EQUAL] = ENTRY("equal"),
  [COMPARE_NEQUAL] = ENTRY("notequal"),
  [COMPARE_LESS] = ENTRY("less"),
  [COMPARE_LEQUAL] = ENTRY("lequal"),
  [COMPARE_GREATER] = ENTRY("greater"),
  [COMPARE_GEQUAL] = ENTRY("gequal"),
  { 0 }
};

StringEntry CoordinateSpaces[] = {
  [SPACE_LOCAL] = ENTRY("local"),
  [SPACE_GLOBAL] = ENTRY("global"),
  { 0 }
};

StringEntry DefaultShaders[] = {
  [SHADER_UNLIT] = ENTRY("unlit"),
  [SHADER_STANDARD] = ENTRY("standard"),
  [SHADER_CUBE] = ENTRY("cube"),
  [SHADER_PANO] = ENTRY("pano"),
  [SHADER_FONT] = ENTRY("font"),
  [SHADER_FILL] = ENTRY("screenspace"),
  { 0 }
};

StringEntry DrawModes[] = {
  [DRAW_POINTS] = ENTRY("points"),
  [DRAW_LINES] = ENTRY("lines"),
  [DRAW_LINE_STRIP] = ENTRY("linestrip"),
  [DRAW_LINE_LOOP] = ENTRY("lineloop"),
  [DRAW_TRIANGLE_STRIP] = ENTRY("strip"),
  [DRAW_TRIANGLES] = ENTRY("triangles"),
  [DRAW_TRIANGLE_FAN] = ENTRY("fan"),
  { 0 }
};

StringEntry DrawStyles[] = {
  [STYLE_FILL] = ENTRY("fill"),
  [STYLE_LINE] = ENTRY("line"),
  { 0 }
};

StringEntry FilterModes[] = {
  [FILTER_NEAREST] = ENTRY("nearest"),
  [FILTER_BILINEAR] = ENTRY("bilinear"),
  [FILTER_TRILINEAR] = ENTRY("trilinear"),
  { 0 }
};

StringEntry HorizontalAligns[] = {
  [ALIGN_LEFT] = ENTRY("left"),
  [ALIGN_CENTER] = ENTRY("center"),
  [ALIGN_RIGHT] = ENTRY("right"),
  { 0 }
};

StringEntry MaterialColors[] = {
  [COLOR_DIFFUSE] = ENTRY("diffuse"),
  [COLOR_EMISSIVE] = ENTRY("emissive"),
  { 0 }
};

StringEntry MaterialScalars[] = {
  [SCALAR_METALNESS] = ENTRY("metalness"),
  [SCALAR_ROUGHNESS] = ENTRY("roughness"),
  { 0 }
};

StringEntry MaterialTextures[] = {
  [TEXTURE_DIFFUSE] = ENTRY("diffuse"),
  [TEXTURE_EMISSIVE] = ENTRY("emissive"),
  [TEXTURE_METALNESS] = ENTRY("metalness"),
  [TEXTURE_ROUGHNESS] = ENTRY("roughness"),
  [TEXTURE_OCCLUSION] = ENTRY("occlusion"),
  [TEXTURE_NORMAL] = ENTRY("normal"),
  { 0 }
};

StringEntry ShaderTypes[] = {
  [SHADER_GRAPHICS] = ENTRY("graphics"),
  [SHADER_COMPUTE] = ENTRY("compute"),
  { 0 }
};

StringEntry StencilActions[] = {
  [STENCIL_REPLACE] = ENTRY("replace"),
  [STENCIL_INCREMENT] = ENTRY("increment"),
  [STENCIL_DECREMENT] = ENTRY("decrement"),
  [STENCIL_INCREMENT_WRAP] = ENTRY("incrementwrap"),
  [STENCIL_DECREMENT_WRAP] = ENTRY("decrementwrap"),
  [STENCIL_INVERT] = ENTRY("invert"),
  { 0 }
};

StringEntry TextureFormats[] = {
  [FORMAT_RGB] = ENTRY("rgb"),
  [FORMAT_RGBA] = ENTRY("rgba"),
  [FORMAT_RGBA4] = ENTRY("rgba4"),
  [FORMAT_RGBA16F] = ENTRY("rgba16f"),
  [FORMAT_RGBA32F] = ENTRY("rgba32f"),
  [FORMAT_R16F] = ENTRY("r16f"),
  [FORMAT_R32F] = ENTRY("r32f"),
  [FORMAT_RG16F] = ENTRY("rg16f"),
  [FORMAT_RG32F] = ENTRY("rg32f"),
  [FORMAT_RGB5A1] = ENTRY("rgb5a1"),
  [FORMAT_RGB10A2] = ENTRY("rgb10a2"),
  [FORMAT_RG11B10F] = ENTRY("rg11b10f"),
  [FORMAT_D16] = ENTRY("d16"),
  [FORMAT_D32F] = ENTRY("d32f"),
  [FORMAT_D24S8] = ENTRY("d24s8"),
  [FORMAT_DXT1] = ENTRY("dxt1"),
  [FORMAT_DXT3] = ENTRY("dxt3"),
  [FORMAT_DXT5] = ENTRY("dxt5"),
  [FORMAT_ASTC_4x4] = ENTRY("astc4x4"),
  [FORMAT_ASTC_5x4] = ENTRY("astc5x4"),
  [FORMAT_ASTC_5x5] = ENTRY("astc5x5"),
  [FORMAT_ASTC_6x5] = ENTRY("astc6x5"),
  [FORMAT_ASTC_6x6] = ENTRY("astc6x6"),
  [FORMAT_ASTC_8x5] = ENTRY("astc8x5"),
  [FORMAT_ASTC_8x6] = ENTRY("astc8x6"),
  [FORMAT_ASTC_8x8] = ENTRY("astc8x8"),
  [FORMAT_ASTC_10x5] = ENTRY("astc10x5"),
  [FORMAT_ASTC_10x6] = ENTRY("astc10x6"),
  [FORMAT_ASTC_10x8] = ENTRY("astc10x8"),
  [FORMAT_ASTC_10x10] = ENTRY("astc10x10"),
  [FORMAT_ASTC_12x10] = ENTRY("astc12x10"),
  [FORMAT_ASTC_12x12] = ENTRY("astc12x12"),
  { 0 }
};

StringEntry TextureTypes[] = {
  [TEXTURE_2D] = ENTRY("2d"),
  [TEXTURE_ARRAY] = ENTRY("array"),
  [TEXTURE_CUBE] = ENTRY("cube"),
  [TEXTURE_VOLUME] = ENTRY("volume"),
  { 0 }
};

StringEntry UniformAccesses[] = {
  [ACCESS_READ] = ENTRY("read"),
  [ACCESS_WRITE] = ENTRY("write"),
  [ACCESS_READ_WRITE] = ENTRY("readwrite"),
  { 0 }
};

StringEntry VerticalAligns[] = {
  [ALIGN_TOP] = ENTRY("top"),
  [ALIGN_MIDDLE] = ENTRY("middle"),
  [ALIGN_BOTTOM] = ENTRY("bottom"),
  { 0 }
};

StringEntry Windings[] = {
  [WINDING_CLOCKWISE] = ENTRY("clockwise"),
  [WINDING_COUNTERCLOCKWISE] = ENTRY("counterclockwise"),
  { 0 }
};

StringEntry WrapModes[] = {
  [WRAP_CLAMP] = ENTRY("clamp"),
  [WRAP_REPEAT] = ENTRY("repeat"),
  [WRAP_MIRRORED_REPEAT] = ENTRY("mirroredrepeat"),
  { 0 }
};

static uint32_t luax_getvertexcount(lua_State* L, int index) {
  int type = lua_type(L, index);
  if (type == LUA_TTABLE) {
    int count = luax_len(L, index);
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

static void luax_readvertices(lua_State* L, int index, float* v, uint32_t count) {
  switch (lua_type(L, index)) {
    case LUA_TTABLE:
      lua_rawgeti(L, index, 1);
      if (lua_type(L, -1) == LUA_TNUMBER) {
        lua_pop(L, 1);
        for (uint32_t i = 0; i < count; i++) {
          for (int j = 0; j < 3; j++) {
            lua_rawgeti(L, index, 3 * i + j + 1);
            v[j] = lua_tonumber(L, -1);
            lua_pop(L, 1);
          }
          v[3] = v[4] = v[5] = v[6] = v[7] = 0.f;
          v += 8;
        }
      } else {
        lua_pop(L, 1);
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, index, i + 1);
          vec3_init(v, luax_checkvector(L, -1, V_VEC3, NULL));
          lua_pop(L, 1);
          v[3] = v[4] = v[5] = v[6] = v[7] = 0.f;
          v += 8;
        }
      }
      break;

    case LUA_TNUMBER:
      for (uint32_t i = 0; i < count; i++) {
        for (int j = 0; j < 3; j++) {
          v[j] = lua_tonumber(L, index + 3 * i + j);
        }
        v[3] = v[4] = v[5] = v[6] = v[7] = 0.f;
        v += 8;
      }
      break;

    default:
      for (uint32_t i = 0; i < count; i++) {
        vec3_init(v, luax_checkvector(L, index + i, V_VEC3, NULL));
        v[3] = v[4] = v[5] = v[6] = v[7] = 0.f;
        v += 8;
      }
      break;
  }
}

static void stencilCallback(void* userdata) {
  lua_State* L = userdata;
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_call(L, 0, 0);
}

// Must be released when done
static TextureData* luax_checktexturedata(lua_State* L, int index, bool flip) {
  TextureData* textureData = luax_totype(L, index, TextureData);

  if (textureData) {
    lovrRetain(textureData);
  } else {
    Blob* blob = luax_readblob(L, index, "Texture");
    textureData = lovrTextureDataCreateFromBlob(blob, flip);
    lovrRelease(Blob, blob);
  }

  return textureData;
}

// Base

static int l_lovrGraphicsPresent(lua_State* L) {
  lovrGraphicsPresent();
  return 0;
}

static int l_lovrGraphicsCreateWindow(lua_State* L) {
  WindowFlags flags;
  memset(&flags, 0, sizeof(flags));

  if (!lua_toboolean(L, 1)) {
    return 0;
  }

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

  lua_getfield(L, 1, "resizable");
  flags.resizable = lua_toboolean(L, -1);
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
    flags.icon.data = textureData->blob->data;
    flags.icon.width = textureData->width;
    flags.icon.height = textureData->height;
    lovrRelease(TextureData, textureData);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "vsync");
  flags.vsync = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lovrGraphicsCreateWindow(&flags);
  luax_atexit(L, lovrGraphicsDestroy); // The lua_State that creates the window shall be the one to destroy it
  lovrRelease(TextureData, textureData);
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

static int l_lovrGraphicsGetPixelDensity(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetPixelDensity());
  return 1;
}

static int l_lovrGraphicsHasWindow(lua_State *L) {
  bool window = lovrPlatformHasWindow();
  lua_pushboolean(L, window);
  return 1;
}

static int l_lovrGraphicsTick(lua_State* L) {
  const char* label = luaL_checkstring(L, 1);
  lovrGraphicsTick(label);
  return 0;
}

static int l_lovrGraphicsTock(lua_State* L) {
  lovrGraphicsFlush();
  const char* label = luaL_checkstring(L, 1);
  lua_pushnumber(L, lovrGraphicsTock(label));
  return 1;
}

static int l_lovrGraphicsGetFeatures(lua_State* L) {
  const GpuFeatures* features = lovrGraphicsGetFeatures();
  lua_newtable(L);
  lua_pushboolean(L, features->astc);
  lua_setfield(L, -2, "astc");
  lua_pushboolean(L, features->compute);
  lua_setfield(L, -2, "compute");
  lua_pushboolean(L, features->dxt);
  lua_setfield(L, -2, "dxt");
  lua_pushboolean(L, features->instancedStereo);
  lua_setfield(L, -2, "instancedstereo");
  lua_pushboolean(L, features->multiview);
  lua_setfield(L, -2, "multiview");
  lua_pushboolean(L, features->timers);
  lua_setfield(L, -2, "timers");
  return 1;
}

static int l_lovrGraphicsGetLimits(lua_State* L) {
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
    lua_createtable(L, 0, 7);
  }

  lovrGraphicsFlush();
  const GpuStats* stats = lovrGraphicsGetStats();
  lua_pushinteger(L, stats->shaderSwitches);
  lua_setfield(L, 1, "shaderswitches");
  lua_pushinteger(L, stats->renderPasses);
  lua_setfield(L, 1, "renderpasses");
  lua_pushinteger(L, stats->drawCalls);
  lua_setfield(L, 1, "drawcalls");
  lua_pushinteger(L, stats->bufferCount);
  lua_setfield(L, 1, "buffers");
  lua_pushinteger(L, stats->textureCount);
  lua_setfield(L, 1, "textures");
  lua_pushinteger(L, stats->bufferMemory);
  lua_setfield(L, 1, "buffermemory");
  lua_pushinteger(L, stats->textureMemory);
  lua_setfield(L, 1, "texturememory");
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
  luax_readcolor(L, 1, &color);
  lovrGraphicsSetBackgroundColor(color);
  return 0;
}

static int l_lovrGraphicsGetBlendMode(lua_State* L) {
  BlendMode mode;
  BlendAlphaMode alphaMode;
  lovrGraphicsGetBlendMode(&mode, &alphaMode);
  luax_pushenum(L, BlendModes, mode);
  luax_pushenum(L, BlendAlphaModes, alphaMode);
  return 2;
}

static int l_lovrGraphicsSetBlendMode(lua_State* L) {
  BlendMode mode = lua_isnoneornil(L, 1) ? BLEND_NONE : luax_checkenum(L, 1, BlendModes, NULL, "BlendMode");
  BlendAlphaMode alphaMode = luax_checkenum(L, 2, BlendAlphaModes, "alphamultiply", "BlendAlphaMode");
  lovrGraphicsSetBlendMode(mode, alphaMode);
  return 0;
}

static int l_lovrGraphicsGetCanvas(lua_State* L) {
  Canvas* canvas = lovrGraphicsGetCanvas();
  luax_pushtype(L, Canvas, canvas);
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
  Color color;
  luax_readcolor(L, 1, &color);
  lovrGraphicsSetColor(color);
  return 0;
}

static int l_lovrGraphicsGetColorMask(lua_State* L) {
  bool r, b, g, a;
  lovrGraphicsGetColorMask(&r, &g, &b, &a);
  lua_pushboolean(L, r);
  lua_pushboolean(L, g);
  lua_pushboolean(L, b);
  lua_pushboolean(L, a);
  return 4;
}

static int l_lovrGraphicsSetColorMask(lua_State* L) {
  bool r = lua_toboolean(L, 1);
  bool g = lua_toboolean(L, 2);
  bool b = lua_toboolean(L, 3);
  bool a = lua_toboolean(L, 4);
  lovrGraphicsSetColorMask(r, g, b, a);
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
  luax_pushenum(L, FilterModes, filter.mode);
  lua_pushnumber(L, filter.anisotropy);
  return 2;
}

static int l_lovrGraphicsSetDefaultFilter(lua_State* L) {
  FilterMode mode = luax_checkenum(L, 1, FilterModes, NULL, "FilterMode");
  float anisotropy = luax_optfloat(L, 2, 1.f);
  lovrGraphicsSetDefaultFilter((TextureFilter) { .mode = mode, .anisotropy = anisotropy });
  return 0;
}

static int l_lovrGraphicsGetDepthTest(lua_State* L) {
  CompareMode mode;
  bool write;
  lovrGraphicsGetDepthTest(&mode, &write);
  luax_pushenum(L, CompareModes, mode);
  lua_pushboolean(L, write);
  return 2;
}

static int l_lovrGraphicsSetDepthTest(lua_State* L) {
  CompareMode mode = lua_isnoneornil(L, 1) ? COMPARE_NONE : luax_checkenum(L, 1, CompareModes, NULL, "CompareMode");
  bool write = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
  lovrGraphicsSetDepthTest(mode, write);
  return 0;
}

static int l_lovrGraphicsGetFont(lua_State* L) {
  Font* font = lovrGraphicsGetFont();
  luax_pushtype(L, Font, font);
  return 1;
}

static int l_lovrGraphicsSetFont(lua_State* L) {
  Font* font = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Font);
  lovrGraphicsSetFont(font);
  return 0;
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
  float size = luax_optfloat(L, 1, 1.f);
  lovrGraphicsSetPointSize(size);
  return 0;
}

static int l_lovrGraphicsGetShader(lua_State* L) {
  Shader* shader = lovrGraphicsGetShader();
  luax_pushtype(L, Shader, shader);
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
  luax_pushenum(L, CompareModes, mode);
  lua_pushinteger(L, value);
  return 2;
}

static int l_lovrGraphicsSetStencilTest(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lovrGraphicsSetStencilTest(COMPARE_NONE, 0);
  } else {
    CompareMode mode = luax_checkenum(L, 1, CompareModes, NULL, "CompareMode");
    int value = luaL_checkinteger(L, 2);
    lovrGraphicsSetStencilTest(mode, value);
  }
  return 0;
}

static int l_lovrGraphicsGetWinding(lua_State* L) {
  luax_pushenum(L, Windings, lovrGraphicsGetWinding());
  return 1;
}

static int l_lovrGraphicsSetWinding(lua_State* L) {
  Winding winding = luax_checkenum(L, 1, Windings, NULL, "Winding");
  lovrGraphicsSetWinding(winding);
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
  float translation[4];
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
  float scale[4];
  luax_readscale(L, 1, scale, 3, NULL);
  lovrGraphicsScale(scale);
  return 0;
}

static int l_lovrGraphicsTransform(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 3);
  lovrGraphicsMatrixTransform(transform);
  return 0;
}

static int l_lovrGraphicsSetProjection(lua_State* L) {
  float transform[16];
  luax_readmat4(L, 1, transform, 3);
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

  if (top == 1) {
    luax_readcolor(L, index, &color);
    lovrGraphicsClear(&color, NULL, NULL);
    return 0;
  }

  if (top >= index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
      color.r = luax_checkfloat(L, index++);
      color.g = luax_checkfloat(L, index++);
      color.b = luax_checkfloat(L, index++);
      color.a = luax_optfloat(L, index++, 1.f);
    } else {
      clearColor = lua_toboolean(L, index++);
    }
  }

  if (top >= index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
      depth = luax_checkfloat(L, index++);
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
    style = luax_checkenum(L, 1, DrawStyles, NULL, "DrawStyle");
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
    style = luax_checkenum(L, 1, DrawStyles, NULL, "DrawStyle");
  }
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 2);
  float u = luax_optfloat(L, index++, 0.f);
  float v = luax_optfloat(L, index++, 0.f);
  float w = luax_optfloat(L, index++, 1.f - u);
  float h = luax_optfloat(L, index++, 1.f - v);
  lovrGraphicsPlane(style, material, transform, u, v, w, h);
  return 0;
}

static int luax_rectangularprism(lua_State* L, int scaleComponents) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luax_checkenum(L, 1, DrawStyles, NULL, "DrawStyle");
  }
  float transform[16];
  luax_readmat4(L, 2, transform, scaleComponents);
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
    style = luax_checkenum(L, 1, DrawStyles, NULL, "DrawStyle");
  }
  ArcMode mode = ARC_MODE_PIE;
  int index = 2;
  if (lua_type(L, index) == LUA_TSTRING) {
    mode = luax_checkenum(L, index++, ArcModes, NULL, "ArcMode");
  }
  float transform[16];
  index = luax_readmat4(L, index, transform, 1);
  float r1 = luax_optfloat(L, index++, 0.f);
  float r2 = luax_optfloat(L, index++, 2.f * (float) M_PI);
  int segments = luaL_optinteger(L, index, 64) * (MIN(fabsf(r2 - r1), 2.f * (float) M_PI) / (2.f * (float) M_PI));
  lovrGraphicsArc(style, mode, material, transform, r1, r2, segments);
  return 0;
}

static int l_lovrGraphicsCircle(lua_State* L) {
  DrawStyle style = STYLE_FILL;
  Material* material = NULL;
  if (lua_isuserdata(L, 1)) {
    material = luax_checktype(L, 1, Material);
  } else {
    style = luax_checkenum(L, 1, DrawStyles, NULL, "DrawStyle");
  }
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  int segments = luaL_optinteger(L, index, 32);
  lovrGraphicsCircle(style, material, transform, segments);
  return 0;
}

static int l_lovrGraphicsCylinder(lua_State* L) {
  float transform[16];
  Material* material = luax_totype(L, 1, Material);
  int index = material ? 2 : 1;
  index = luax_readmat4(L, index, transform, 1);
  float r1 = luax_optfloat(L, index++, 1.f);
  float r2 = luax_optfloat(L, index++, 1.f);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  int segments = luaL_optinteger(L, index, (lua_Integer) floorf(16 + 16 * MAX(r1, r2)));
  lovrGraphicsCylinder(material, transform, r1, r2, capped, segments);
  return 0;
}

static int l_lovrGraphicsSphere(lua_State* L) {
  float transform[16];
  Material* material = luax_totype(L, 1, Material);
  int index = material ? 2 : 1;
  index = luax_readmat4(L, index, transform, 1);
  int segments = luaL_optinteger(L, index, 30);
  lovrGraphicsSphere(material, transform, segments);
  return 0;
}

static int l_lovrGraphicsSkybox(lua_State* L) {
  Texture* texture = luax_checktype(L, 1, Texture);
  lovrGraphicsSkybox(texture);
  return 0;
}

static int l_lovrGraphicsPrint(lua_State* L) {
  size_t length;
  const char* str = luaL_checklstring(L, 1, &length);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  float wrap = luax_optfloat(L, index++, 0.f);
  HorizontalAlign halign = luax_checkenum(L, index++, HorizontalAligns, "center", "HorizontalAlign");
  VerticalAlign valign = luax_checkenum(L, index++, VerticalAligns, "middle", "VerticalAlign");
  lovrGraphicsPrint(str, length, transform, wrap, halign, valign);
  return 0;
}

static int l_lovrGraphicsStencil(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  StencilAction action = luax_checkenum(L, 2, StencilActions, "replace", "StencilAction");
  int replaceValue = luaL_optinteger(L, 3, 1);
  bool keepValues = lua_toboolean(L, 4);
  if (!keepValues) {
    int clearTo = lua_isnumber(L, 4) ? lua_tonumber(L, 4) : 0;
    lovrGraphicsClear(NULL, NULL, &clearTo);
  }
  lua_settop(L, 1);
  bool r, g, b, a;
  lovrGraphicsGetColorMask(&r, &g, &b, &a);
  lovrGraphicsSetColorMask(false, false, false, false);
  lovrGraphicsStencil(action, replaceValue, stencilCallback, L);
  lovrGraphicsSetColorMask(r, g, b, a);
  return 0;
}

static int l_lovrGraphicsFill(lua_State* L) {
  Texture* texture = lua_isnoneornil(L, 1) ? NULL : luax_checktype(L, 1, Texture);
  float u = luax_optfloat(L, 2, 0.f);
  float v = luax_optfloat(L, 3, 0.f);
  float w = luax_optfloat(L, 4, 1.f - u);
  float h = luax_optfloat(L, 5, 1.f - v);
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

static void luax_checkuniformtype(lua_State* L, int index, UniformType* baseType, int* components) {
  size_t length;
  lovrAssert(lua_type(L, index) == LUA_TSTRING, "Uniform types must be strings, got %s", lua_typename(L, index));
  const char* type = lua_tolstring(L, index, &length);

  if (!strcmp(type, "float")) {
    *baseType = UNIFORM_FLOAT;
    *components = 1;
  } else if (!strcmp(type, "int")) {
    *baseType = UNIFORM_INT;
    *components = 1;
  } else {
    int n = type[length - 1] - '0';
    lovrAssert(n >= 2 && n <= 4, "Unknown uniform type '%s'", type);
    if (type[0] == 'v' && type[1] == 'e' && type[2] == 'c' && length == 4) {
      *baseType = UNIFORM_FLOAT;
      *components = n;
    } else if (type[0] == 'i' && type[1] == 'v' && type[2] == 'e' && type[3] == 'c' && length == 5) {
      *baseType = UNIFORM_INT;
      *components = n;
    } else if (type[0] == 'm' && type[1] == 'a' && type[2] == 't' && length == 4) {
      *baseType = UNIFORM_MATRIX;
      *components = n;
    } else {
      lovrThrow("Unknown uniform type '%s'", type);
    }
  }
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

  TextureFormat format = FORMAT_RGBA;
  bool anonymous = attachmentCount == 0;

  CanvasFlags flags = {
    .depth = { .enabled = true, .readable = false, .format = FORMAT_D16 },
    .stereo = anonymous,
    .msaa = 0,
    .mipmaps = true
  };

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "depth");
    switch (lua_type(L, -1)) {
      case LUA_TNIL: break;
      case LUA_TBOOLEAN: flags.depth.enabled = lua_toboolean(L, -1); break;
      case LUA_TSTRING: flags.depth.format = luax_checkenum(L, -1, TextureFormats, NULL, "TextureFormat"); break;
      case LUA_TTABLE:
        lua_getfield(L, -1, "readable");
        flags.depth.readable = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "format");
        flags.depth.format = luax_checkenum(L, -1, TextureFormats, NULL, "TextureFormat");
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
      format = luax_checkenum(L, -1, TextureFormats, "rgba", "TextureFormat");
      anonymous = lua_isnil(L, -1) || lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
  }

  if (width == 0 && height == 0 && attachmentCount > 0) {
    width = lovrTextureGetWidth(attachments[0].texture, attachments[0].level);
    height = lovrTextureGetHeight(attachments[0].texture, attachments[0].level);
  }

  Canvas* canvas = lovrCanvasCreate(width, height, flags);

  if (anonymous) {
    bool multiview = flags.stereo && lovrGraphicsGetFeatures()->multiview;
    TextureType textureType = multiview ? TEXTURE_ARRAY : TEXTURE_2D;
    uint32_t depth = multiview ? 2 : 1;
    Texture* texture = lovrTextureCreate(textureType, NULL, 0, true, flags.mipmaps, flags.msaa);
    lovrTextureAllocate(texture, lovrCanvasGetWidth(canvas), lovrCanvasGetHeight(canvas), depth, format);
    lovrTextureSetWrap(texture, (TextureWrap) { .s = WRAP_CLAMP, .t = WRAP_CLAMP, .r = WRAP_CLAMP });
    attachments[0] = (Attachment) { texture, 0, 0 };
    attachmentCount++;
  }

  if (attachmentCount > 0) {
    lovrCanvasSetAttachments(canvas, attachments, attachmentCount);
    if (anonymous) {
      lovrRelease(Texture, attachments[0].texture);
    }
  }

  luax_pushtype(L, Canvas, canvas);
  lovrRelease(Canvas, canvas);
  return 1;
}

static int l_lovrGraphicsNewFont(lua_State* L) {
  Rasterizer* rasterizer = luax_totype(L, 1, Rasterizer);

  if (!rasterizer) {
    Blob* blob = NULL;
    float size;

    if (lua_type(L, 1) == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
      size = luaL_optinteger(L, 1, 32);
    } else {
      blob = luax_readblob(L, 1, "Font");
      size = luaL_optinteger(L, 2, 32);
    }

    rasterizer = lovrRasterizerCreate(blob, size);
    lovrRelease(Blob, blob);
  }

  Font* font = lovrFontCreate(rasterizer);
  luax_pushtype(L, Font, font);
  lovrRelease(Rasterizer, rasterizer);
  lovrRelease(Font, font);
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
    lovrRelease(Blob, blob);
    lovrRelease(TextureData, textureData);
    lovrRelease(Texture, texture);
  } else if (lua_isuserdata(L, index)) {
    Texture* texture = luax_checktype(L, index, Texture);
    lovrMaterialSetTexture(material, TEXTURE_DIFFUSE, texture);
    index++;
  }

  if (lua_isnumber(L, index)) {
    Color color;
    luax_readcolor(L, index, &color);
    lovrMaterialSetColor(material, COLOR_DIFFUSE, color);
  }

  luax_pushtype(L, Material, material);
  lovrRelease(Material, material);
  return 1;
}

static int l_lovrGraphicsNewMesh(lua_State* L) {
  uint32_t count;
  int dataIndex = 0;
  int formatIndex = 0;
  int drawModeIndex = 2;
  Blob* blob = NULL;

  if (lua_isnumber(L, 1)) {
    count = lua_tointeger(L, 1);
  } else if (lua_istable(L, 1)) {
    if (lua_isnumber(L, 2)) {
      drawModeIndex++;
      formatIndex = 1;
      count = lua_tointeger(L, 2);
      dataIndex = 0;
    } else if (lua_istable(L, 2)) {
      drawModeIndex++;
      formatIndex = 1;
      count = luax_len(L, 2);
      dataIndex = 2;
    } else if ((blob = luax_totype(L, 2, Blob)) != NULL) {
      drawModeIndex++;
      formatIndex = 1;
      dataIndex = 2;
    } else {
      count = luax_len(L, 1);
      dataIndex = 1;
    }
  } else {
    luaL_argerror(L, 1, "table or number expected");
    return 0;
  }

  MeshAttribute attributes[MAX_ATTRIBUTES];
  const char* attributeNames[MAX_ATTRIBUTES];
  int attributeCount = 0;
  size_t stride = 0;

  if (formatIndex == 0) {
    stride = 32;
    attributeCount = 3;
    attributes[0] = (MeshAttribute) { .offset = 0, .stride = stride, .type = F32, .components = 3 };
    attributes[1] = (MeshAttribute) { .offset = 12, .stride = stride, .type = F32, .components = 3 };
    attributes[2] = (MeshAttribute) { .offset = 24, .stride = stride, .type = F32, .components = 2 };
    attributeNames[0] = "lovrPosition";
    attributeNames[1] = "lovrNormal";
    attributeNames[2] = "lovrTexCoord";
  } else {
    attributeCount = luax_len(L, formatIndex);
    lovrAssert(attributeCount >= 0 && attributeCount <= MAX_ATTRIBUTES, "Attribute count must be between 0 and %d", MAX_ATTRIBUTES);
    for (int i = 0; i < attributeCount; i++) {
      lua_rawgeti(L, formatIndex, i + 1);
      lovrAssert(lua_type(L, -1) == LUA_TTABLE, "Attribute definitions must be tables containing name, type, and component count");
      lua_rawgeti(L, -1, 3);
      lua_rawgeti(L, -2, 2);
      lua_rawgeti(L, -3, 1);

      attributeNames[i] = lua_tostring(L, -1);
      attributes[i].offset = (uint32_t) stride;
      attributes[i].type = luax_checkenum(L, -2, AttributeTypes, "float", "AttributeType");
      attributes[i].components = luaL_optinteger(L, -3, 1);

      switch (attributes[i].type) {
        case I8: case U8: stride += 1 * attributes[i].components; break;
        case I16: case U16: stride += 2 * attributes[i].components; break;
        case I32: case U32: case F32: stride += 4 * attributes[i].components; break;
      }
      lua_pop(L, 4);
    }
  }

  if (blob) {
    lovrAssert(blob->size / stride < UINT32_MAX, "Too many vertices in Blob");
    count = (uint32_t) (blob->size / stride);
  }

  DrawMode mode = luax_checkenum(L, drawModeIndex, DrawModes, "fan", "DrawMode");
  BufferUsage usage = luax_checkenum(L, drawModeIndex + 1, BufferUsages, "dynamic", "BufferUsage");
  bool readable = lua_toboolean(L, drawModeIndex + 2);
  Buffer* vertexBuffer = lovrBufferCreate(count * stride, NULL, BUFFER_VERTEX, usage, readable);
  Mesh* mesh = lovrMeshCreate(mode, vertexBuffer, count);

  for (int i = 0; i < attributeCount; i++) {
    lovrMeshAttachAttribute(mesh, attributeNames[i], &(MeshAttribute) {
      .buffer = vertexBuffer,
      .offset = attributes[i].offset,
      .stride = stride,
      .type = attributes[i].type,
      .components = attributes[i].components,
      .normalized = attributes[i].type == I8 || attributes[i].type == U8
    });
  }

  lovrMeshAttachAttribute(mesh, "lovrDrawID", &(MeshAttribute) {
    .buffer = lovrGraphicsGetIdentityBuffer(),
    .type = U8,
    .components = 1,
    .divisor = 1,
    .integer = true
  });

  if (dataIndex) {
    AttributeData data = { .raw = lovrBufferMap(vertexBuffer, 0) };

    if (blob) {
      memcpy(data.raw, blob->data, count * stride);
    } else {
      for (uint32_t i = 0; i < count; i++) {
        lua_rawgeti(L, dataIndex, i + 1);
        lovrAssert(lua_istable(L, -1), "Vertices should be specified as a table of tables");

        int component = 0;
        for (int j = 0; j < attributeCount; j++) {
          MeshAttribute* attribute = &attributes[j];
          for (unsigned k = 0; k < attribute->components; k++) {
            lua_rawgeti(L, -1, ++component);
            switch (attribute->type) {
              case I8: *data.i8++ = luaL_optinteger(L, -1, 0); break;
              case U8: *data.u8++ = luaL_optinteger(L, -1, 0); break;
              case I16: *data.i16++ = luaL_optinteger(L, -1, 0); break;
              case U16: *data.u16++ = luaL_optinteger(L, -1, 0); break;
              case I32: *data.i32++ = luaL_optinteger(L, -1, 0); break;
              case U32: *data.u32++ = luaL_optinteger(L, -1, 0); break;
              case F32: *data.f32++ = luaL_optnumber(L, -1, 0.); break;
            }
            lua_pop(L, 1);
          }
        }

        lua_pop(L, 1);
      }
    }
  }

  lovrBufferFlush(vertexBuffer, 0, count * stride);
  lovrBufferUnmap(vertexBuffer);
  lovrRelease(Buffer, vertexBuffer);

  luax_pushtype(L, Mesh, mesh);
  lovrRelease(Mesh, mesh);
  return 1;
}

static int l_lovrGraphicsNewModel(lua_State* L) {
  ModelData* modelData = luax_totype(L, 1, ModelData);

  if (!modelData) {
    Blob* blob = luax_readblob(L, 1, "Model");
    modelData = lovrModelDataCreate(blob, luax_readfile);
    lovrRelease(Blob, blob);
  } else {
    lovrRetain(modelData);
  }

  Model* model = lovrModelCreate(modelData);
  luax_pushtype(L, Model, model);
  lovrRelease(ModelData, modelData);
  lovrRelease(Model, model);
  return 1;
}

static const char* luax_checkshadersource(lua_State* L, int index, int *outLength) {
  if (lua_isnoneornil(L, index)) {
    return NULL;
  }

  Blob* blob = luax_totype(L, index, Blob);
  if (blob) {
    *outLength = blob->size;
    return blob->data;
  }

  size_t length;
  const char* source = luaL_checklstring(L, index, &length);
  if (memchr(source, '\n', MIN(1024, length))) {
    *outLength = length;
    return source;
  } else {
    void* contents = luax_readfile(source, &length);
    lovrAssert(contents, "Could not read shader from file '%s'", source);
    *outLength = length;
    return contents;
  }
}

#define MAX_SHADER_FLAGS 32
static void luax_parseshaderflags(lua_State* L, int index, ShaderFlag flags[MAX_SHADER_FLAGS], uint32_t* count) {
  if (lua_isnil(L, -1)) {
    *count = 0;
    return;
  }

  lovrAssert(lua_istable(L, -1), "Shader flags must be a table");
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    ShaderFlag* flag = &flags[(*count)++];

    lovrAssert(*count <= MAX_SHADER_FLAGS, "Too many shader flags (max is %d)", MAX_SHADER_FLAGS);

    if (lua_type(L, -2) == LUA_TSTRING) {
      flag->name = lua_tostring(L, -2);
    } else if (lua_isnumber(L, -2)) {
      flag->index = lua_tointeger(L, -2);
    } else {
      lovrThrow("Shader flag names must be strings or numbers");
    }

    switch (lua_type(L, -1)) {
      case LUA_TBOOLEAN:
        flag->type = FLAG_BOOL;
        flag->value.b32 = lua_toboolean(L, -1);
        break;
      case LUA_TNUMBER:
        flag->type = FLAG_INT;
        flag->value.i32 = lua_tointeger(L, -1);
        break;
      default:
        lovrThrow("Shader flag values must be booleans or integers");
    }

    lua_pop(L, 1);
  }
}

static int l_lovrGraphicsNewShader(lua_State* L) {
  ShaderFlag flags[MAX_SHADER_FLAGS];
  uint32_t flagCount = 0;
  bool multiview = true;
  Shader* shader;

  if (lua_isstring(L, 1) && (lua_istable(L, 2) || lua_gettop(L) == 1)) {
    DefaultShader shaderType = luax_checkenum(L, 1, DefaultShaders, NULL, "DefaultShader");

    if (lua_istable(L, 2)) {
      lua_getfield(L, 2, "flags");
      luax_parseshaderflags(L, -1, flags, &flagCount);
      lua_pop(L, 1);

      lua_getfield(L, 2, "stereo");
      multiview = lua_isnil(L, -1) ? multiview : lua_toboolean(L, -1);
      lua_pop(L, 1);
    }

    shader = lovrShaderCreateDefault(shaderType, flags, flagCount, multiview);

    // Builtin uniforms
    if (shaderType == SHADER_STANDARD) {
      lovrShaderSetFloats(shader, "lovrExposure", (float[1]) { 1.f }, 0, 1);
      lovrShaderSetFloats(shader, "lovrLightDirection", (float[3]) { -1.f, -1.f, -1.f }, 0, 3);
      lovrShaderSetFloats(shader, "lovrLightColor", (float[4]) { 1.f, 1.f, 1.f, 1.f }, 0, 4);
    }
  } else {
    int vertexSourceLength;
    const char* vertexSource = luax_checkshadersource(L, 1, &vertexSourceLength);
    int fragmentSourceLength;
    const char* fragmentSource = luax_checkshadersource(L, 2, &fragmentSourceLength);

    if (lua_istable(L, 3)) {
      lua_getfield(L, 3, "flags");
      luax_parseshaderflags(L, -1, flags, &flagCount);
      lua_pop(L, 1);

      lua_getfield(L, 3, "stereo");
      multiview = lua_isnil(L, -1) ? multiview : lua_toboolean(L, -1);
      lua_pop(L, 1);
    }

    shader = lovrShaderCreateGraphics(
      vertexSource, vertexSourceLength, fragmentSource, fragmentSourceLength,
      flags, flagCount, multiview);
  }

  luax_pushtype(L, Shader, shader);
  lovrRelease(Shader, shader);
  return 1;
}

static int l_lovrGraphicsNewComputeShader(lua_State* L) {
  int sourceLength;
  const char* source = luax_checkshadersource(L, 1, &sourceLength);
  ShaderFlag flags[MAX_SHADER_FLAGS];
  uint32_t flagCount = 0;

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "flags");
    luax_parseshaderflags(L, -1, flags, &flagCount);
    lua_pop(L, 1);
  }

  Shader* shader = lovrShaderCreateCompute(source, sourceLength, flags, flagCount);
  luax_pushtype(L, Shader, shader);
  lovrRelease(Shader, shader);
  return 1;
}

static int l_lovrGraphicsNewShaderBlock(lua_State* L) {
  arr_uniform_t uniforms;
  arr_init(&uniforms);

  BlockType type = luax_checkenum(L, 1, BlockTypes, NULL, "Blocktype");

  luaL_checktype(L, 2, LUA_TTABLE);
  lua_pushnil(L);
  while (lua_next(L, 2) != 0) {
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
    arr_push(&uniforms, uniform);

    // Pop the table, leaving the key for lua_next to nom
    lua_pop(L, 1);
  }

  BufferUsage usage = USAGE_DYNAMIC;
  bool readable = false;

  if (lua_istable(L, 3)) {
    lua_getfield(L, 3, "usage");
    usage = luax_checkenum(L, -1, BufferUsages, "dynamic", "BufferUsage");
    lua_pop(L, 1);

    lua_getfield(L, 3, "readable");
    readable = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  lovrAssert(type == BLOCK_UNIFORM || lovrGraphicsGetFeatures()->compute, "Compute blocks are not supported on this system");
  size_t size = lovrShaderComputeUniformLayout(&uniforms);
  Buffer* buffer = lovrBufferCreate(size, NULL, type == BLOCK_COMPUTE ? BUFFER_SHADER_STORAGE : BUFFER_UNIFORM, usage, readable);
  ShaderBlock* block = lovrShaderBlockCreate(type, buffer, &uniforms);
  luax_pushtype(L, ShaderBlock, block);
  arr_free(&uniforms);
  lovrRelease(Buffer, buffer);
  lovrRelease(ShaderBlock, block);
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
    depth = luax_len(L, index++);
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
    type = lua_isnil(L, -1) ? type : (TextureType) luax_checkenum(L, -1, TextureTypes, NULL, "TextureType");
    lua_pop(L, 1);

    lua_getfield(L, index, "format");
    format = lua_isnil(L, -1) ? format : (TextureFormat) luax_checkenum(L, -1, TextureFormats, NULL, "TextureFormat");
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
        lovrAssert(!lua_isnil(L, -1), "Could not load cubemap texture: missing '%s' face", faces[i]);
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
      lovrRelease(TextureData, textureData);
      lua_pop(L, 1);
    }
  }

  luax_pushtype(L, Texture, texture);
  lovrRelease(Texture, texture);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {

  // Base
  { "present", l_lovrGraphicsPresent },
  { "createWindow", l_lovrGraphicsCreateWindow },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "getPixelDensity", l_lovrGraphicsGetPixelDensity },
  { "hasWindow", l_lovrGraphicsHasWindow },
  { "tick", l_lovrGraphicsTick },
  { "tock", l_lovrGraphicsTock },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
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
  { "getColorMask", l_lovrGraphicsGetColorMask },
  { "setColorMask", l_lovrGraphicsSetColorMask },
  { "isCullingEnabled", l_lovrGraphicsIsCullingEnabled },
  { "setCullingEnabled", l_lovrGraphicsSetCullingEnabled },
  { "getDefaultFilter", l_lovrGraphicsGetDefaultFilter },
  { "setDefaultFilter", l_lovrGraphicsSetDefaultFilter },
  { "getDepthTest", l_lovrGraphicsGetDepthTest },
  { "setDepthTest", l_lovrGraphicsSetDepthTest },
  { "getFont", l_lovrGraphicsGetFont },
  { "setFont", l_lovrGraphicsSetFont },
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
  luax_registertype(L, Canvas);
  luax_registertype(L, Font);
  luax_registertype(L, Material);
  luax_registertype(L, Mesh);
  luax_registertype(L, Model);
  luax_registertype(L, Shader);
  luax_registertype(L, ShaderBlock);
  luax_registertype(L, Texture);
  lovrGraphicsInit();

  luax_pushconf(L);
  lua_pushcfunction(L, l_lovrGraphicsCreateWindow);
  lua_getfield(L, -2, "window");
  lua_call(L, 1, 0);
  lua_pop(L, 1);
  return 1;
}
