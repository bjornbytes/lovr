#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static int l_lovrPassReset(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassReset(pass);
  return 1;
}

static int l_lovrPassGetStats(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const PassStats* stats = lovrPassGetStats(pass);
  lua_newtable(L);
  lua_pushinteger(L, stats->draws), lua_setfield(L, -2, "draws");
  lua_pushinteger(L, stats->computes), lua_setfield(L, -2, "computes");
  lua_pushinteger(L, stats->drawsCulled), lua_setfield(L, -2, "drawsCulled");
  lua_pushinteger(L, stats->cpuMemoryReserved), lua_setfield(L, -2, "cpuMemoryReserved");
  lua_pushinteger(L, stats->cpuMemoryUsed), lua_setfield(L, -2, "cpuMemoryUsed");
  lua_pushnumber(L, stats->submitTime), lua_setfield(L, -2, "submitTime");
  lua_pushnumber(L, stats->gpuTime), lua_setfield(L, -2, "gpuTime");
  return 1;
}

static int l_lovrPassGetLabel(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const char* label = lovrPassGetLabel(pass);
  lua_pushstring(L, label);
  return 1;
}

static int l_lovrPassGetCanvas(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  CanvasTexture color[4];
  CanvasTexture depth;
  uint32_t depthFormat;
  Texture* foveation;
  uint32_t samples;
  lovrPassGetCanvas(pass, color, &depth, &depthFormat, &foveation, &samples);

  if (!color[0].texture && !depth.texture) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  bool anyResolve = false;
  for (uint32_t i = 0; i < COUNTOF(color) && color[i].texture; i++) {
    luax_pushtype(L, Texture, color[i].texture);
    lua_rawseti(L, -2, i + 1);
    anyResolve |= color[i].resolve != NULL;
  }

  if (anyResolve) {
    lua_newtable(L);
    for (uint32_t i = 0; i < COUNTOF(color); i++) {
      luax_pushtype(L, Texture, color[i].resolve);
      lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "resolve");
  }

  if (depth.texture) {
    luax_pushtype(L, Texture, depth.texture);
    lua_setfield(L, -2, "depth");
  } else if (depthFormat) {
    luax_pushenum(L, TextureFormat, depthFormat);
    lua_setfield(L, -2, "depth");
  }

  lua_pushinteger(L, samples);
  lua_setfield(L, -2, "samples");
  return 1;
}

int l_lovrPassSetCanvas(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CanvasTexture color[4] = { 0 };
  CanvasTexture depth = { 0 };
  uint32_t depthFormat = FORMAT_D32F;
  uint32_t samples = 4;
  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "texture");
    if (lua_isnil(L, -1)) {
      int length = luax_len(L, 2);
      for (int i = 0; i < length && i < 4; i++) {
        lua_rawgeti(L, 2, i + 1);
        color[i].texture = luax_checktype(L, -1, Texture);
        lua_pop(L, 1);
      }
    } else {
      color[0].texture = luax_checktype(L, -1, Texture);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "resolve");
    if (lua_istable(L, -1)) {
      for (int i = 0; i < 4; i++) {
        lua_rawgeti(L, -1, i + 1);
        color[i].resolve = luax_totype(L, -1, Texture);
        lua_pop(L, 1);
      }
    } else if (!lua_isnil(L, -1)) {
      color[0].resolve = luax_checktype(L, -1, Texture);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "depth");
    switch (lua_type(L, -1)) {
      case LUA_TUSERDATA: depth.texture = luax_checktype(L, -1, Texture); break;
      case LUA_TSTRING: depthFormat = luax_checkenum(L, -1, TextureFormat, NULL); break;
      case LUA_TBOOLEAN: depthFormat = lua_toboolean(L, -1) ? FORMAT_D32F : 0; break;
      case LUA_TNIL: depthFormat = FORMAT_D32F; break;
      case LUA_TTABLE:
        lua_getfield(L, -1, "texture");
        depth.texture = luax_totype(L, -1, Texture);
        luax_check(L, depth.texture, "When depth is a table, it must have a 'texture' key with a Texture");
        lua_pop(L, 1);

        lua_getfield(L, -1, "resolve");
        depth.resolve = lua_isnil(L, -1) ? NULL : luax_checktype(L, -1, Texture);
        lua_pop(L, 1);
        break;
      default: luaL_error(L, "Expected Texture, TextureFormat, boolean, table, or nil for canvas depth buffer");
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "samples");
    samples = luax_optu32(L, -1, samples);
    lua_pop(L, 1);
  } else if (lua_isuserdata(L, 2)) {
    int top = lua_gettop(L);
    for (int i = 0; i + 2 <= top && i < 4; i++) {
      color[i].texture = luax_checktype(L, i + 2, Texture);
    }
  } else if (!lua_isnoneornil(L, 2)) {
    luaL_error(L, "Expected Texture, table, or nil for canvas");
  }
  luax_assert(L, lovrPassSetCanvas(pass, color, &depth, depthFormat, NULL, samples));
  return 0;
}

static int l_lovrPassGetClear(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  bool depth;
  uint32_t count = lovrPassGetAttachmentCount(pass, &depth);

  if (count == 0 && !depth) {
    lua_pushnil(L);
    return 1;
  }

  LoadAction loads[4], depthLoad;
  float clears[4][4], depthClear;
  lovrPassGetClear(pass, loads, clears, &depthLoad, &depthClear);

  lua_createtable(L, (int) count, depth);
  for (int i = 0; i < (int) count; i++) {
    lua_createtable(L, 4, 0);
    if (loads[i] == LOAD_CLEAR) {
      for (int j = 0; j < 4; j++) {
        lua_pushnumber(L, clears[i][j]);
        lua_rawseti(L, -2, j + 1);
      }
    } else {
      lua_pushboolean(L, loads[i] == LOAD_DISCARD);
    }
    lua_rawseti(L, -2, i + 1);
  }

  if (depth) {
    if (depthLoad == LOAD_CLEAR) {
      lua_pushnumber(L, depthClear);
    } else {
      lua_pushboolean(L, depthLoad == LOAD_DISCARD);
    }
    lua_setfield(L, -2, "depth");
  }

  return 1;
}

static int l_lovrPassSetClear(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  LoadAction loads[4] = { 0 };
  float clears[4][4] = { 0 };
  LoadAction depthLoad = LOAD_CLEAR;
  float depthClear = 0.f;

  if (lua_type(L, 2) == LUA_TNUMBER) {
    loads[0] = LOAD_CLEAR;
    luax_readcolor(L, 2, clears[0]);
    for (uint32_t i = 1; i < COUNTOF(clears); i++) {
      memcpy(clears[i], clears[0], 4 * sizeof(float));
      loads[i] = loads[0];
    }
  } else if (lua_type(L, 2) == LUA_TBOOLEAN) {
    loads[0] = loads[1] = loads[2] = loads[3] = lua_toboolean(L, 2) ? LOAD_DISCARD : LOAD_KEEP;
  } else if (lua_type(L, 2) == LUA_TTABLE) {
    lua_rawgeti(L, 2, 1);
    if (lua_type(L, -1) == LUA_TNUMBER) {
      lua_pop(L, 1);
      loads[0] = LOAD_CLEAR;
      luax_optcolor(L, -1, clears[0]);
      for (uint32_t i = 1; i < COUNTOF(clears); i++) {
        memcpy(clears[i], clears[0], 4 * sizeof(float));
        loads[i] = loads[0];
      }
    } else {
      lua_pop(L, 1);
      for (uint32_t i = 0; i < COUNTOF(clears); i++) {
        lua_rawgeti(L, 2, i + 1);
        if (lua_istable(L, -1)) {
          lua_rawgeti(L, -1, 1);
          lua_rawgeti(L, -2, 2);
          lua_rawgeti(L, -3, 3);
          lua_rawgeti(L, -4, 4);
          clears[i][0] = luax_checkfloat(L, -4);
          clears[i][1] = luax_checkfloat(L, -3);
          clears[i][2] = luax_checkfloat(L, -2);
          clears[i][3] = luax_optfloat(L, -1, 1.f);
          lua_pop(L, 4);
        } else {
          loads[i] = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP;
        }
        lua_pop(L, 1);
      }
    }

    lua_getfield(L, 2, "depth");
    switch (lua_type(L, -1)) {
      case LUA_TNIL: break;
      case LUA_TBOOLEAN: depthLoad = lua_toboolean(L, -1) ? LOAD_DISCARD : LOAD_KEEP; break;
      case LUA_TNUMBER: depthClear = lua_tonumber(L, -1); break;
      default: luaL_error(L, "Expected boolean or number for depth clear");
    }
    lua_pop(L, 1);
  } else {
    return luax_typeerror(L, 2, "number, boolean, or table");
  }

  luax_assert(L, lovrPassSetClear(pass, loads, clears, depthLoad, depthClear));
  return 0;
}

static int l_lovrPassGetWidth(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t width = lovrPassGetWidth(pass);
  lua_pushinteger(L, width);
  return 1;
}

static int l_lovrPassGetHeight(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t height = lovrPassGetHeight(pass);
  lua_pushinteger(L, height);
  return 1;
}

static int l_lovrPassGetDimensions(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t width = lovrPassGetWidth(pass);
  uint32_t height = lovrPassGetHeight(pass);
  lua_pushinteger(L, width);
  lua_pushinteger(L, height);
  return 2;
}

static int l_lovrPassGetViewCount(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t views = lovrPassGetViewCount(pass);
  lua_pushinteger(L, views);
  return 1;
}

static int l_lovrPassGetViewPose(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    bool invert = lua_toboolean(L, 4);
    luax_assert(L, lovrPassGetViewMatrix(pass, view, matrix));
    if (!invert) mat4_invert(matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], angle, ax, ay, az;
    luax_assert(L, lovrPassGetViewMatrix(pass, view, matrix));
    mat4_invert(matrix);
    mat4_getAngleAxis(matrix, &angle, &ax, &ay, &az);
    lua_pushnumber(L, matrix[12]);
    lua_pushnumber(L, matrix[13]);
    lua_pushnumber(L, matrix[14]);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
    return 7;
  }
}

static int l_lovrPassSetViewPose(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  VectorType type;
  float* p = luax_tovector(L, 3, &type);
  if (p && type == V_MAT4) {
    float matrix[16];
    mat4_init(matrix, p);
    bool inverted = lua_toboolean(L, 4);
    if (!inverted) mat4_invert(matrix);
    luax_assert(L, lovrPassSetViewMatrix(pass, view, matrix));
  } else {
    int index = 3;
    float position[3], orientation[4], matrix[16];
    index = luax_readvec3(L, index, position, "vec3, number, or mat4");
    index = luax_readquat(L, index, orientation, NULL);
    mat4_fromPose(matrix, position, orientation);
    mat4_invert(matrix);
    luax_assert(L, lovrPassSetViewMatrix(pass, view, matrix));
  }
  return 0;
}

static int l_lovrPassGetProjection(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    luax_assert(L, lovrPassGetProjection(pass, view, matrix));
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], left, right, up, down;
    luax_assert(L, lovrPassGetProjection(pass, view, matrix));
    mat4_getFov(matrix, &left, &right, &up, &down);
    lua_pushnumber(L, left);
    lua_pushnumber(L, right);
    lua_pushnumber(L, up);
    lua_pushnumber(L, down);
    return 4;
  }
}

static int l_lovrPassSetProjection(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  if (lua_type(L, 3) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 3);
    float right = luax_checkfloat(L, 4);
    float up = luax_checkfloat(L, 5);
    float down = luax_checkfloat(L, 6);
    float clipNear = luax_optfloat(L, 7, .01f);
    float clipFar = luax_optfloat(L, 8, 0.f);
    float matrix[16];
    mat4_fov(matrix, left, right, up, down, clipNear, clipFar);
    lovrPassSetProjection(pass, view, matrix);
  } else {
    float* matrix = luax_checkvector(L, 3, V_MAT4, "mat4 or number");
    lovrPassSetProjection(pass, view, matrix);
  }
  return 0;
}

static int l_lovrPassPush(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StackType stack = luax_checkenum(L, 2, StackType, "transform");
  luax_assert(L, lovrPassPush(pass, stack));
  return 0;
}

static int l_lovrPassPop(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StackType stack = luax_checkenum(L, 2, StackType, "transform");
  luax_assert(L, lovrPassPop(pass, stack));
  return 0;
}

static int l_lovrPassOrigin(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassOrigin(pass);
  return 0;
}

static int l_lovrPassTranslate(lua_State* L) {
  float translation[3];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readvec3(L, 2, translation, NULL);
  lovrPassTranslate(pass, translation);
  return 0;
}

static int l_lovrPassRotate(lua_State* L) {
  float rotation[4];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readquat(L, 2, rotation, NULL);
  lovrPassRotate(pass, rotation);
  return 0;
}

static int l_lovrPassScale(lua_State* L) {
  float scale[3];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readscale(L, 2, scale, 3, NULL);
  lovrPassScale(pass, scale);
  return 0;
}

static int l_lovrPassTransform(lua_State* L) {
  float transform[16];
  Pass* pass = luax_checktype(L, 1, Pass);
  luax_readmat4(L, 2, transform, 3);
  lovrPassTransform(pass, transform);
  return 0;
}

static int l_lovrPassSetAlphaToCoverage(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassSetAlphaToCoverage(pass, lua_toboolean(L, 2));
  return 1;
}

static int l_lovrPassSetBlendMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  int index = 2;
  uint32_t target = lua_type(L, 2) == LUA_TNUMBER ? luax_checku32(L, index++) - 1 : ~0u;
  BlendMode mode = lua_isnoneornil(L, index) ? BLEND_NONE : luax_checkenum(L, index++, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, index, BlendAlphaMode, "alphamultiply");
  if (target == ~0u) {
    uint32_t count = lovrPassGetAttachmentCount(pass, NULL);
    for (uint32_t i = 0; i < count; i++) {
      lovrPassSetBlendMode(pass, i, mode, alphaMode);
    }
  } else {
    lovrPassSetBlendMode(pass, target, mode, alphaMode);
  }
  return 0;
}

static int l_lovrPassSetColor(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float color[4];
  luax_readcolor(L, 2, color);
  lovrPassSetColor(pass, color);
  return 0;
}

static int l_lovrPassSetColorWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  int index = 2;
  uint32_t target = lua_type(L, 2) == LUA_TNUMBER ? luax_checku32(L, index++) - 1 : ~0u;
  bool r, g, b, a;
  if (lua_gettop(L) <= index) {
    r = g = b = a = lua_toboolean(L, index);
  } else {
    r = lua_toboolean(L, index++);
    g = lua_toboolean(L, index++);
    b = lua_toboolean(L, index++);
    a = lua_toboolean(L, index++);
  }
  if (target == ~0u) {
    uint32_t count = lovrPassGetAttachmentCount(pass, NULL);
    for (uint32_t i = 0; i < count; i++) {
      lovrPassSetColorWrite(pass, i, r, g, b, a);
    }
  } else {
    lovrPassSetColorWrite(pass, target, r, g, b, a);
  }
  return 0;
}

static int l_lovrPassSetDepthTest(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CompareMode test = luax_checkcomparemode(L, 2);
  lovrPassSetDepthTest(pass, test);
  return 0;
}

static int l_lovrPassSetDepthWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool write = lua_toboolean(L, 2);
  lovrPassSetDepthWrite(pass, write);
  return 0;
}

static int l_lovrPassSetDepthOffset(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float offset = luax_optfloat(L, 2, 0.f);
  float sloped = luax_optfloat(L, 3, 0.f);
  lovrPassSetDepthOffset(pass, offset, sloped);
  return 0;
}

static int l_lovrPassSetDepthClamp(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool clamp = lua_toboolean(L, 2);
  lovrPassSetDepthClamp(pass, clamp);
  return 0;
}

static int l_lovrPassSetFaceCull(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CullMode mode;
  if (lua_type(L, 2) == LUA_TBOOLEAN) {
    mode = lua_toboolean(L, 2) ? CULL_BACK : CULL_NONE;
  } else {
    mode = luax_checkenum(L, 2, CullMode, "none");
  }
  lovrPassSetFaceCull(pass, mode);
  return 0;
}

static int l_lovrPassSetFont(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Font* font = luax_totype(L, 2, Font);
  lovrPassSetFont(pass, font);
  return 0;
}

static int l_lovrPassSetMaterial(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Material* material = luax_optmaterial(L, 2);
  lovrPassSetMaterial(pass, material);
  return 0;
}

static int l_lovrPassSetMeshMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  DrawMode mode = luax_checkenum(L, 2, DrawMode, NULL);
  lovrPassSetMeshMode(pass, mode);
  return 0;
}

static int l_lovrPassSetSampler(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  if (lua_type(L, 2) != LUA_TUSERDATA) {
    FilterMode filter = luax_checkenum(L, 2, FilterMode, "linear");
    Sampler* sampler = lovrGraphicsGetDefaultSampler(filter);
    lovrPassSetSampler(pass, sampler);
  } else {
    Sampler* sampler = luax_checktype(L, 2, Sampler);
    lovrPassSetSampler(pass, sampler);
  }
  return 0;
}

static int l_lovrPassSetScissor(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  if (lua_isnoneornil(L, 2)) {
    lovrPassSetScissor(pass, NULL);
  } else {
    uint32_t scissor[4];
    scissor[0] = luax_checku32(L, 2);
    scissor[1] = luax_checku32(L, 3);
    scissor[2] = luax_checku32(L, 4);
    scissor[3] = luax_checku32(L, 5);
    lovrPassSetScissor(pass, scissor);
  }
  return 0;
}

static int l_lovrPassSetShader(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL:
      lovrPassSetShader(pass, NULL);
      return 0;
    case LUA_TSTRING:
      lovrPassSetShader(pass, lovrGraphicsGetDefaultShader(luax_checkenum(L, 2, DefaultShader, NULL)));
      return 0;
    default:
      lovrPassSetShader(pass, luax_checktype(L, 2, Shader));
      return 0;
  }
}

static int l_lovrPassSetStencilTest(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CompareMode test = luax_checkcomparemode(L, 2);
  uint8_t value = lua_tointeger(L, 3);
  uint8_t mask = luaL_optinteger(L, 4, 0xff);
  luax_assert(L, lovrPassSetStencilTest(pass, test, value, mask));
  return 0;
}

static int l_lovrPassSetStencilWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StencilAction actions[3];
  if (lua_isnoneornil(L, 2)) {
    actions[0] = actions[1] = actions[2] = STENCIL_KEEP;
    luax_assert(L, lovrPassSetStencilWrite(pass, actions, 0, 0xff));
  } else {
    if (lua_istable(L, 2)) {
      lua_rawgeti(L, 2, 1);
      lua_rawgeti(L, 2, 2);
      lua_rawgeti(L, 2, 3);
      actions[0] = luax_checkenum(L, -3, StencilAction, NULL);
      actions[1] = luax_checkenum(L, -2, StencilAction, NULL);
      actions[2] = luax_checkenum(L, -1, StencilAction, NULL);
      lua_pop(L, 3);
    } else {
      actions[0] = STENCIL_KEEP;
      actions[1] = STENCIL_KEEP;
      actions[2] = luax_checkenum(L, 2, StencilAction, NULL);
    }
    uint8_t value = luaL_optinteger(L, 3, 1);
    uint8_t mask = luaL_optinteger(L, 4, 0xff);
    luax_assert(L, lovrPassSetStencilWrite(pass, actions, value, mask));
  }
  return 0;
}

static int l_lovrPassSetViewCull(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool enable = lua_toboolean(L, 2);
  lovrPassSetViewCull(pass, enable);
  return 0;
}

static int l_lovrPassSetViewport(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  if (lua_isnoneornil(L, 2)) {
    lovrPassSetViewport(pass, NULL);
  } else {
    float viewport[6];
    viewport[0] = luax_checkfloat(L, 2);
    viewport[1] = luax_checkfloat(L, 3);
    viewport[2] = luax_checkfloat(L, 4);
    viewport[3] = luax_checkfloat(L, 5);
    viewport[4] = luax_optfloat(L, 6, 0.f);
    viewport[5] = luax_optfloat(L, 7, 1.f);
    luax_check(L, viewport[2] > 0.f, "Viewport width must be positive");
    luax_check(L, viewport[3] != 0.f, "Viewport height can not be zero");
    luax_check(L, viewport[4] >= 0.f && viewport[4] <= 1.f, "Viewport depth range must be between 0 and 1");
    luax_check(L, viewport[5] >= 0.f && viewport[5] <= 1.f, "Viewport depth range must be between 0 and 1");
    lovrPassSetViewport(pass, viewport);
  }
  return 0;
}

static int l_lovrPassSetWinding(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Winding winding = luax_checkenum(L, 2, Winding, NULL);
  lovrPassSetWinding(pass, winding);
  return 0;
}

static int l_lovrPassSetWireframe(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  bool wireframe = lua_toboolean(L, 2);
  lovrPassSetWireframe(pass, wireframe);
  return 0;
}

static int l_lovrPassSend(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  size_t length;
  const char* name = luaL_checklstring(L, 2, &length);

  if (lua_isnoneornil(L, 3)) {
    return luax_typeerror(L, 3, "Buffer, Texture, Sampler, number, vector, table, or boolean");
  }

  Buffer* buffer = luax_totype(L, 3, Buffer);

  if (buffer) {
    uint32_t offset = lua_tointeger(L, 4);
    uint32_t extent = lua_tointeger(L, 5);
    luax_assert(L, lovrPassSendBuffer(pass, name, length, buffer, offset, extent));
    return 0;
  }

  Texture* texture = luax_totype(L, 3, Texture);

  if (texture) {
    luax_assert(L, lovrPassSendTexture(pass, name, length, texture));
    return 0;
  }

  Sampler* sampler = luax_totype(L, 3, Sampler);

  if (sampler) {
    luax_assert(L, lovrPassSendSampler(pass, name, length, sampler));
    return 0;
  }

  void* pointer;
  DataField* format;
  luax_assert(L, lovrPassSendData(pass, name, length, &pointer, &format));
  char* data = pointer;

  // Coerce booleans since they aren't supported in buffer formats
  if (lua_isboolean(L, 3)) {
    bool value = lua_toboolean(L, 3);
    lua_settop(L, 2);
    lua_pushinteger(L, value);
  }

  luax_checkbufferdata(L, 3, format, data);

  return 0;
}

static uint32_t luax_getvertexcount(lua_State* L, int index) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
      return 0;
    case LUA_TNUMBER:
      return (lua_gettop(L) - index + 1) / 3;
    case LUA_TTABLE:
      lua_rawgeti(L, index, 1);
      int innerType = lua_type(L, -1);
      lua_pop(L, 1);
      return luax_len(L, index) / (innerType == LUA_TNUMBER ? 3 : 1);
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
      return lua_gettop(L) - index + 1;
    default:
      return luax_typeerror(L, index, "number, table, or vector");
  }
}

static void luax_readvertices(lua_State* L, int index, float* vertices, uint32_t count) {
  switch (lua_type(L, index)) {
    case LUA_TNONE:
    case LUA_TNIL:
    default:
      break;
    case LUA_TNUMBER:
      for (uint32_t i = 0; i < 3 * count; i++) {
        *vertices++ = luax_tofloat(L, index + i);
      }
      break;
    case LUA_TTABLE:
      lua_rawgeti(L, index, 1);
      int innerType = lua_type(L, -1);
      lua_pop(L, 1);
      if (innerType == LUA_TNUMBER) {
        for (uint32_t i = 0; i < 3 * count; i++) {
          lua_rawgeti(L, index, i + 1);
          *vertices++ = luax_tofloat(L, -1);
          lua_pop(L, 1);
        }
      } else if (innerType == LUA_TUSERDATA || innerType == LUA_TLIGHTUSERDATA) {
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, index, i + 1);
          float* v = luax_checkvector(L, -1, V_VEC3, NULL);
          memcpy(vertices, v, 3 * sizeof(float));
          vertices += 3;
          lua_pop(L, 1);
        }
      }
      break;
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
      for (uint32_t i = 0; i < count; i++) {
        float *v = luax_checkvector(L, index + i, V_VEC3, NULL);
        memcpy(vertices, v, 3 * sizeof(float));
        vertices += 3;
      }
      break;
  }
}

static int l_lovrPassPoints(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 2);
  luax_assert(L, lovrPassPoints(pass, count, &vertices));
  luax_readvertices(L, 2, vertices, count);
  return 0;
}

static int l_lovrPassLine(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 2);
  luax_assert(L, lovrPassLine(pass, count, &vertices));
  luax_readvertices(L, 2, vertices, count);
  return 0;
}

static int l_lovrPassPolygon(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 2);
  luax_assert(L, lovrPassPolygon(pass, count, &vertices));
  luax_readvertices(L, 2, vertices, count);
  return 0;
}

static int l_lovrPassPlane(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 2);
  DrawStyle style = luax_checkenum(L, index++, DrawStyle, "fill");
  uint32_t cols = luax_optu32(L, index++, 1);
  uint32_t rows = luax_optu32(L, index, cols);
  luax_assert(L, lovrPassPlane(pass, transform, style, cols, rows));
  return 0;
}

static int l_lovrPassRoundrect(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 3);
  float radius = luax_optfloat(L, index++, 0.f);
  uint32_t segments = luax_optu32(L, index++, 8);
  luax_assert(L, lovrPassRoundrect(pass, transform, radius, segments));
  return 0;
}

static int l_lovrPassCube(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  DrawStyle style = luax_checkenum(L, index, DrawStyle, "fill");
  luax_assert(L, lovrPassBox(pass, transform, style));
  return 0;
}

static int l_lovrPassBox(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 3);
  DrawStyle style = luax_checkenum(L, index, DrawStyle, "fill");
  luax_assert(L, lovrPassBox(pass, transform, style));
  return 0;
}

static int l_lovrPassCircle(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  DrawStyle style = luax_checkenum(L, index++, DrawStyle, "fill");
  float angle1 = luax_optfloat(L, index++, 0.f);
  float angle2 = luax_optfloat(L, index++, 2.f * (float) M_PI);
  uint32_t segments = luax_optu32(L, index++, 64);
  luax_assert(L, lovrPassCircle(pass, transform, style, angle1, angle2, segments));
  return 0;
}

static int l_lovrPassSphere(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  uint32_t segmentsH = luax_optu32(L, index++, 48);
  uint32_t segmentsV = luax_optu32(L, index++, segmentsH / 2);
  luax_assert(L, lovrPassSphere(pass, transform, segmentsH, segmentsV));
  return 0;
}

static bool luax_checkendpoints(lua_State* L, int index, float transform[16], bool center) {
  float *v, *u;
  VectorType t1, t2;
  if ((v = luax_tovector(L, index + 0, &t1)) == NULL || t1 != V_VEC3) return false;
  if ((u = luax_tovector(L, index + 1, &t2)) == NULL || t2 != V_VEC3) return false;
  float radius = luax_optfloat(L, index + 2, 1.);
  float orientation[4];
  float forward[3] = { 0.f, 0.f, -1.f };
  float direction[3];
  vec3_sub(vec3_init(direction, u), v);
  float length = vec3_length(direction);
  vec3_normalize(direction);
  quat_between(orientation, forward, direction);
  mat4_identity(transform);
  if (center) {
    mat4_translate(transform, (v[0] + u[0]) / 2.f, (v[1] + u[1]) / 2.f, (v[2] + u[2]) / 2.f);
  } else {
    mat4_translate(transform, v[0], v[1], v[2]);
  }
  mat4_rotateQuat(transform, orientation);
  mat4_scale(transform, radius, radius, length);
  return true;
}

static int l_lovrPassCylinder(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_checkendpoints(L, 2, transform, true) ? 5 : luax_readmat4(L, 2, transform, -2);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  float angle1 = luax_optfloat(L, index++, 0.f);
  float angle2 = luax_optfloat(L, index++, 2.f * (float) M_PI);
  uint32_t segments = luax_optu32(L, index++, 64);
  luax_assert(L, lovrPassCylinder(pass, transform, capped, angle1, angle2, segments));
  return 0;
}

static int l_lovrPassCone(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_checkendpoints(L, 2, transform, false) ? 5 : luax_readmat4(L, 2, transform, -2);
  uint32_t segments = luax_optu32(L, index, 64);
  luax_assert(L, lovrPassCone(pass, transform, segments));
  return 0;
}

static int l_lovrPassCapsule(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_checkendpoints(L, 2, transform, true) ? 5 : luax_readmat4(L, 2, transform, -2);
  uint32_t segments = luax_optu32(L, index, 32);
  luax_assert(L, lovrPassCapsule(pass, transform, segments));
  return 0;
}

static int l_lovrPassTorus(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, -2);
  uint32_t segmentsT = luax_optu32(L, index++, 64);
  uint32_t segmentsP = luax_optu32(L, index++, segmentsT / 2);
  luax_assert(L, lovrPassTorus(pass, transform, segmentsT, segmentsP));
  return 0;
}

static int l_lovrPassText(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t count;
  ColoredString stack;
  ColoredString* strings = luax_checkcoloredstrings(L, 2, &count, &stack);
  float transform[16];
  int index = luax_readmat4(L, 3, transform, 1);
  float wrap = luax_optfloat(L, index++, 0.);
  HorizontalAlign halign = luax_checkenum(L, index++, HorizontalAlign, "center");
  VerticalAlign valign = luax_checkenum(L, index++, VerticalAlign, "middle");
  luax_assert(L, lovrPassText(pass, strings, count, transform, wrap, halign, valign));
  if (strings != &stack) lovrFree(strings);
  return 0;
}

static int l_lovrPassSkybox(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* texture = luax_totype(L, 2, Texture);
  luax_assert(L, lovrPassSkybox(pass, texture));
  return 0;
}

static int l_lovrPassFill(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* texture = luax_totype(L, 2, Texture);
  luax_assert(L, lovrPassFill(pass, texture));
  return 0;
}

static int l_lovrPassMonkey(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  luax_readmat4(L, 2, transform, 1);
  luax_assert(L, lovrPassMonkey(pass, transform));
  return 0;
}

static int l_lovrPassDraw(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];

  Model* model = luax_totype(L, 2, Model);

  if (model) {
    int index = luax_readmat4(L, 3, transform, 1);
    uint32_t instances = luax_optu32(L, index, 1);
    luax_assert(L, lovrPassDrawModel(pass, model, transform, instances));
    return 0;
  }

  Mesh* mesh = luax_totype(L, 2, Mesh);

  if (mesh) {
    int index = luax_readmat4(L, 3, transform, 1);
    uint32_t instances = luax_optu32(L, index, 1);
    luax_assert(L, lovrPassDrawMesh(pass, mesh, transform, instances));
    return 0;
  }

  Texture* texture = luax_totype(L, 2, Texture);

  if (texture) {
    float transform[16];
    luax_readmat4(L, 3, transform, 1);
    luax_assert(L, lovrPassDrawTexture(pass, texture, transform));
    return 0;
  }

  return luax_typeerror(L, 2, "Mesh, Model, or Texture");
}

static int l_lovrPassMesh(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Buffer* vertices = (!lua_toboolean(L, 2) || lua_type(L, 2) == LUA_TNUMBER) ? NULL : luax_checktype(L, 2, Buffer);
  Buffer* indices = luax_totype(L, 3, Buffer);
  Buffer* indirect = luax_totype(L, 4, Buffer);
  if (indirect) {
    uint32_t count = luax_optu32(L, 5, 1);
    uint32_t offset = luax_optu32(L, 6, 0);
    uint32_t stride = luax_optu32(L, 7, 0);
    luax_assert(L, lovrPassMeshIndirect(pass, vertices, indices, indirect, count, offset, stride));
  } else {
    float transform[16];
    int index = luax_readmat4(L, indices ? 4 : 3, transform, 1);
    uint32_t start = luax_optu32(L, index++, 1) - 1;
    uint32_t count = luax_optu32(L, index++, ~0u);
    uint32_t instances = luax_optu32(L, index++, 1);
    uint32_t baseVertex = luax_optu32(L, index++, 0);
    if (count == ~0u && lua_type(L, 2) == LUA_TNUMBER) {
      count = lua_tointeger(L, 2);
    }
    luax_assert(L, lovrPassMesh(pass, vertices, indices, transform, start, count, instances, baseVertex));
  }
  return 0;
}

static int l_lovrPassBeginTally(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t index;
  luax_assert(L, lovrPassBeginTally(pass, &index));
  lua_pushinteger(L, index);
  return 1;
}

static int l_lovrPassFinishTally(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t index;
  luax_assert(L, lovrPassFinishTally(pass, &index));
  lua_pushinteger(L, index);
  return 1;
}

static int l_lovrPassGetTallyBuffer(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t offset;
  Buffer* buffer = lovrPassGetTallyBuffer(pass, &offset);
  if (buffer) {
    luax_pushtype(L, Buffer, buffer);
    lua_pushinteger(L, offset);
    return 2;
  } else {
    lua_pushnil(L);
    return 1;
  }
}

static int l_lovrPassSetTallyBuffer(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Buffer* buffer = luax_totype(L, 2, Buffer);
  uint32_t offset = luax_optu32(L, 3, 0);
  luax_assert(L, lovrPassSetTallyBuffer(pass, buffer, offset));
  return 0;
}

static int l_lovrPassCompute(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Buffer* buffer = luax_totype(L, 2, Buffer);
  if (buffer) {
    uint32_t offset = lua_tointeger(L, 3);
    lovrPassCompute(pass, 0, 0, 0, buffer, offset);
  } else {
    uint32_t x = luax_optu32(L, 2, 1);
    uint32_t y = luax_optu32(L, 3, 1);
    uint32_t z = luax_optu32(L, 4, 1);
    lovrPassCompute(pass, x, y, z, NULL, 0);
  }
  return 0;
}

static int l_lovrPassBarrier(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassBarrier(pass);
  return 0;
}

static int l_lovrPassToString(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const char* label = lovrPassGetLabel(pass);
  if (label) {
    lua_pushfstring(L, "Pass(%s): %p", label, pass);
  } else {
    lua_pushfstring(L, "Pass: %p", pass);
  }
  return 1;
}

const luaL_Reg lovrPass[] = {
  { "reset", l_lovrPassReset },
  { "getStats", l_lovrPassGetStats },
  { "getLabel", l_lovrPassGetLabel },

  { "getCanvas", l_lovrPassGetCanvas },
  { "setCanvas", l_lovrPassSetCanvas },
  { "getClear", l_lovrPassGetClear },
  { "setClear", l_lovrPassSetClear },
  { "getWidth", l_lovrPassGetWidth },
  { "getHeight", l_lovrPassGetHeight },
  { "getDimensions", l_lovrPassGetDimensions },

  { "getViewCount", l_lovrPassGetViewCount },
  { "getViewPose", l_lovrPassGetViewPose },
  { "setViewPose", l_lovrPassSetViewPose },
  { "getProjection", l_lovrPassGetProjection },
  { "setProjection", l_lovrPassSetProjection },

  { "push", l_lovrPassPush },
  { "pop", l_lovrPassPop },
  { "origin", l_lovrPassOrigin },
  { "translate", l_lovrPassTranslate },
  { "rotate", l_lovrPassRotate },
  { "scale", l_lovrPassScale },
  { "transform", l_lovrPassTransform },

  { "setAlphaToCoverage", l_lovrPassSetAlphaToCoverage },
  { "setBlendMode", l_lovrPassSetBlendMode },
  { "setColor", l_lovrPassSetColor },
  { "setColorWrite", l_lovrPassSetColorWrite },
  { "setDepthTest", l_lovrPassSetDepthTest },
  { "setDepthWrite", l_lovrPassSetDepthWrite },
  { "setDepthOffset", l_lovrPassSetDepthOffset },
  { "setDepthClamp", l_lovrPassSetDepthClamp },
  { "setFaceCull", l_lovrPassSetFaceCull },
  { "setFont", l_lovrPassSetFont },
  { "setMaterial", l_lovrPassSetMaterial },
  { "setMeshMode", l_lovrPassSetMeshMode },
  { "setSampler", l_lovrPassSetSampler },
  { "setScissor", l_lovrPassSetScissor },
  { "setShader", l_lovrPassSetShader },
  { "setStencilTest", l_lovrPassSetStencilTest },
  { "setStencilWrite", l_lovrPassSetStencilWrite },
  { "setViewCull", l_lovrPassSetViewCull },
  { "setViewport", l_lovrPassSetViewport },
  { "setWinding", l_lovrPassSetWinding },
  { "setWireframe", l_lovrPassSetWireframe },

  { "send", l_lovrPassSend },

  { "points", l_lovrPassPoints },
  { "line", l_lovrPassLine },
  { "polygon", l_lovrPassPolygon },
  { "plane", l_lovrPassPlane },
  { "roundrect", l_lovrPassRoundrect },
  { "cube", l_lovrPassCube },
  { "box", l_lovrPassBox },
  { "circle", l_lovrPassCircle },
  { "sphere", l_lovrPassSphere },
  { "cylinder", l_lovrPassCylinder },
  { "cone", l_lovrPassCone },
  { "capsule", l_lovrPassCapsule },
  { "torus", l_lovrPassTorus },
  { "text", l_lovrPassText },
  { "skybox", l_lovrPassSkybox },
  { "fill", l_lovrPassFill },
  { "monkey", l_lovrPassMonkey },
  { "draw", l_lovrPassDraw },
  { "mesh", l_lovrPassMesh },

  { "beginTally", l_lovrPassBeginTally },
  { "finishTally", l_lovrPassFinishTally },
  { "getTallyBuffer", l_lovrPassGetTallyBuffer },
  { "setTallyBuffer", l_lovrPassSetTallyBuffer },

  { "compute", l_lovrPassCompute },
  { "barrier", l_lovrPassBarrier },

  { "__tostring", l_lovrPassToString },

  // Deprecated
  { "setCullMode", l_lovrPassSetFaceCull },

  { NULL, NULL }
};
