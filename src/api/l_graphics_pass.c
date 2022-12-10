#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static int l_lovrPassGetType(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const PassInfo* info = lovrPassGetInfo(pass);
  luax_pushenum(L, PassType, info->type);
  return 1;
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

static int l_lovrPassGetSampleCount(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t samples = lovrPassGetSampleCount(pass);
  lua_pushinteger(L, samples);
  return 1;
}

static int l_lovrPassGetTarget(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  uint32_t count = 0;
  Texture *color[4], *depth;
  lovrPassGetTarget(pass, color, &depth, &count);

  lua_createtable(L, (int) count, !!depth);
  for (int i = 0; i < (int) count; i++) {
    luax_pushtype(L, Texture, color[i]);
    lua_rawseti(L, -2, i + 1);
  }

  if (depth) {
    luax_pushtype(L, Texture, depth);
    lua_setfield(L, -2, "depth");
  }

  return 1;
}

static int l_lovrPassGetClear(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const PassInfo* info = lovrPassGetInfo(pass);

  uint32_t count = 0;
  float color[4][4];
  float depth;
  uint8_t stencil;
  lovrPassGetClear(pass, color, &depth, &stencil, &count);

  lua_createtable(L, (int) count, 2);

  for (int i = 0; i < (int) count; i++) {
    if (info->canvas.loads[i] == LOAD_CLEAR) {
      lua_createtable(L, 4, 0);
      for (int j = 0; j < 4; j++) {
        lua_pushnumber(L, color[i][j]);
        lua_rawseti(L, -2, j + 1);
      }
    } else {
      lua_pushboolean(L, info->canvas.loads[i] == LOAD_DISCARD);
    }

    lua_rawseti(L, -2, i + 1);
  }

  if (info->canvas.depth.format || info->canvas.depth.texture) {
    lua_pushnumber(L, depth);
    lua_setfield(L, -2, "depth");

    lua_pushinteger(L, stencil);
    lua_setfield(L, -2, "stencil");
  }

  return 1;
}

static int l_lovrPassGetViewPose(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    bool invert = lua_toboolean(L, 4);
    lovrPassGetViewMatrix(pass, view, matrix);
    if (!invert) mat4_invert(matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], angle, ax, ay, az;
    lovrPassGetViewMatrix(pass, view, matrix);
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
    lovrPassSetViewMatrix(pass, view, matrix);
  } else {
    int index = 3;
    float position[4], orientation[4], matrix[16];
    index = luax_readvec3(L, index, position, "vec3, number, or mat4");
    index = luax_readquat(L, index, orientation, NULL);
    mat4_fromQuat(matrix, orientation);
    memcpy(matrix + 12, position, 3 * sizeof(float));
    mat4_invert(matrix);
    lovrPassSetViewMatrix(pass, view, matrix);
  }
  return 0;
}

static int l_lovrPassGetProjection(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    lovrPassGetProjection(pass, view, matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], left, right, up, down;
    lovrPassGetProjection(pass, view, matrix);
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
  lovrPassPush(pass, stack);
  return 0;
}

static int l_lovrPassPop(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StackType stack = luax_checkenum(L, 2, StackType, "transform");
  lovrPassPop(pass, stack);
  return 0;
}

static int l_lovrPassOrigin(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  lovrPassOrigin(pass);
  return 0;
}

static int l_lovrPassTranslate(lua_State* L) {
  float translation[4];
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
  float scale[4];
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
  BlendMode mode = lua_isnoneornil(L, 2) ? BLEND_NONE : luax_checkenum(L, 2, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 3, BlendAlphaMode, "alphamultiply");
  lovrPassSetBlendMode(pass, mode, alphaMode);
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
  bool r, g, b, a;
  if (lua_gettop(L) <= 2) {
    r = g = b = a = lua_toboolean(L, 2);
  } else {
    r = lua_toboolean(L, 2);
    g = lua_toboolean(L, 3);
    b = lua_toboolean(L, 4);
    a = lua_toboolean(L, 5);
  }
  lovrPassSetColorWrite(pass, r, g, b, a);
  return 0;
}

static int l_lovrPassSetCullMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  CullMode mode = luax_checkenum(L, 2, CullMode, "none");
  lovrPassSetCullMode(pass, mode);
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

static int l_lovrPassSetFont(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Font* font = luax_totype(L, 2, Font);
  lovrPassSetFont(pass, font);
  return 0;
}

static int l_lovrPassSetMaterial(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Material* material = luax_totype(L, 2, Material);
  Texture* texture = luax_totype(L, 2, Texture);
  lovrPassSetMaterial(pass, material, texture);
  return 0;
}

static int l_lovrPassSetMeshMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  MeshMode mode = luax_checkenum(L, 2, MeshMode, NULL);
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
  uint32_t scissor[4];
  scissor[0] = luax_checku32(L, 2);
  scissor[1] = luax_checku32(L, 3);
  scissor[2] = luax_checku32(L, 4);
  scissor[3] = luax_checku32(L, 5);
  lovrPassSetScissor(pass, scissor);
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
  lovrPassSetStencilTest(pass, test, value, mask);
  return 0;
}

static int l_lovrPassSetStencilWrite(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  StencilAction actions[3];
  if (lua_isnoneornil(L, 2)) {
    actions[0] = actions[1] = actions[2] = STENCIL_KEEP;
    lovrPassSetStencilWrite(pass, actions, 0, 0xff);
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
      actions[0] = actions[1] = actions[2] = luax_checkenum(L, 2, StencilAction, NULL);
    }
    uint8_t value = luaL_optinteger(L, 3, 1);
    uint8_t mask = luaL_optinteger(L, 4, 0xff);
    lovrPassSetStencilWrite(pass, actions, value, mask);
  }
  return 0;
}

static int l_lovrPassSetViewport(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float viewport[4];
  float depthRange[2];
  viewport[0] = luax_checkfloat(L, 2);
  viewport[1] = luax_checkfloat(L, 3);
  viewport[2] = luax_checkfloat(L, 4);
  viewport[3] = luax_checkfloat(L, 5);
  depthRange[0] = luax_optfloat(L, 6, 0.f);
  depthRange[1] = luax_optfloat(L, 7, 1.f);
  lovrPassSetViewport(pass, viewport, depthRange);
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
  const char* name = NULL;
  size_t length = 0;
  uint32_t slot = ~0u;

  switch (lua_type(L, 2)) {
    case LUA_TSTRING: name = lua_tolstring(L, 2, &length); break;
    case LUA_TNUMBER: slot = lua_tointeger(L, 2) - 1; break;
    default: return luax_typeerror(L, 2, "string or number");
  }

  if (lua_isnoneornil(L, 3)) {
    return luax_typeerror(L, 3, "Buffer, Texture, Sampler, number, vector, table, or boolean");
  }

  Buffer* buffer = luax_totype(L, 3, Buffer);

  if (buffer) {
    uint32_t offset = lua_tointeger(L, 4);
    uint32_t extent = lua_tointeger(L, 5);
    lovrPassSendBuffer(pass, name, length, slot, buffer, offset, extent);
    return 0;
  }

  Texture* texture = luax_totype(L, 3, Texture);

  if (texture) {
    lovrPassSendTexture(pass, name, length, slot, texture);
    return 0;
  }

  Sampler* sampler = luax_totype(L, 3, Sampler);

  if (sampler) {
    lovrPassSendSampler(pass, name, length, slot, sampler);
    return 0;
  }

  if (!name) {
    return luax_typeerror(L, 3, "Buffer, Texture, or Sampler");
  }

  void* data;
  FieldType type;
  lovrPassSendValue(pass, name, length, &data, &type);

  int index = 3;

  // readbufferfield doesn't handle booleans or tables; coerce/unpack
  if (lua_isboolean(L, 3)) {
    bool value = lua_toboolean(L, 3);
    lua_settop(L, 2);
    lua_pushinteger(L, value);
  } else if (lua_istable(L, 3)) {
    int length = luax_len(L, 3);
    for (int i = 0; i < length && i < 16; i++) {
      lua_rawgeti(L, 3, i + 1);
    }
    index = 4;
  }

  luax_readbufferfield(L, index, type, data);
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
  lovrPassPoints(pass, count, &vertices);
  luax_readvertices(L, 2, vertices, count);
  return 0;
}

static int l_lovrPassLine(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float* vertices;
  uint32_t count = luax_getvertexcount(L, 2);
  lovrPassLine(pass, count, &vertices);
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
  lovrPassPlane(pass, transform, style, cols, rows);
  return 0;
}

static int l_lovrPassCube(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  DrawStyle style = luax_checkenum(L, index, DrawStyle, "fill");
  lovrPassBox(pass, transform, style);
  return 0;
}

static int l_lovrPassBox(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 3);
  DrawStyle style = luax_checkenum(L, index, DrawStyle, "fill");
  lovrPassBox(pass, transform, style);
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
  lovrPassCircle(pass, transform, style, angle1, angle2, segments);
  return 0;
}

static int l_lovrPassSphere(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  uint32_t segmentsH = luax_optu32(L, index++, 48);
  uint32_t segmentsV = luax_optu32(L, index++, segmentsH / 2);
  lovrPassSphere(pass, transform, segmentsH, segmentsV);
  return 0;
}

static bool luax_checkendpoints(lua_State* L, int index, float transform[16], bool center) {
  float *v, *u;
  VectorType t1, t2;
  if ((v = luax_tovector(L, index + 0, &t1)) == NULL || t1 != V_VEC3) return false;
  if ((u = luax_tovector(L, index + 1, &t2)) == NULL || t2 != V_VEC3) return false;
  float radius = luax_optfloat(L, index + 2, 1.);
  float orientation[4];
  float forward[4] = { 0.f, 0.f, -1.f, 0.f };
  float direction[4];
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
  lovrPassCylinder(pass, transform, capped, angle1, angle2, segments);
  return 0;
}

static int l_lovrPassCone(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_checkendpoints(L, 2, transform, false) ? 5 : luax_readmat4(L, 2, transform, -2);
  uint32_t segments = luax_optu32(L, index, 64);
  lovrPassCone(pass, transform, segments);
  return 0;
}

static int l_lovrPassCapsule(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_checkendpoints(L, 2, transform, true) ? 5 : luax_readmat4(L, 2, transform, -2);
  uint32_t segments = luax_optu32(L, index, 32);
  lovrPassCapsule(pass, transform, segments);
  return 0;
}

static int l_lovrPassTorus(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, -2);
  uint32_t segmentsT = luax_optu32(L, index++, 64);
  uint32_t segmentsP = luax_optu32(L, index++, segmentsT / 2);
  lovrPassTorus(pass, transform, segmentsT, segmentsP);
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
  lovrPassText(pass, strings, count, transform, wrap, halign, valign);
  if (strings != &stack) free(strings);
  return 0;
}

static int l_lovrPassSkybox(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* texture = luax_totype(L, 2, Texture);
  lovrPassSkybox(pass, texture);
  return 0;
}

static int l_lovrPassFill(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* texture = luax_totype(L, 2, Texture);
  lovrPassFill(pass, texture);
  return 0;
}

static int l_lovrPassMonkey(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  luax_readmat4(L, 2, transform, 1);
  lovrPassMonkey(pass, transform);
  return 0;
}

static int l_lovrPassDraw(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];

  Model* model = luax_totype(L, 2, Model);

  if (model) {
    int index = luax_readmat4(L, 3, transform, 1);
    uint32_t node = lua_isnoneornil(L, index) ? ~0u : luax_checknodeindex(L, index, model);
    bool recurse = lua_isnoneornil(L, index + 1) ? true : lua_toboolean(L, index + 1);
    uint32_t instances = lua_isnoneornil(L, index + 2) ? 1 : luax_checku32(L, index + 2);
    lovrPassDrawModel(pass, model, transform, node, recurse, instances);
    return 0;
  }

  return luax_typeerror(L, 2, "Model");
}

static int l_lovrPassMesh(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Buffer* vertices = (!lua_toboolean(L, 2) || lua_type(L, 2) == LUA_TNUMBER) ? NULL : luax_checkbuffer(L, 2);
  Buffer* indices = luax_totype(L, 3, Buffer);
  Buffer* indirect = luax_totype(L, 4, Buffer);
  if (indirect) {
    uint32_t count = luax_optu32(L, 5, 1);
    uint32_t offset = luax_optu32(L, 6, 0);
    uint32_t stride = luax_optu32(L, 7, 0);
    lovrPassMeshIndirect(pass, vertices, indices, indirect, count, offset, stride);
  } else {
    float transform[16];
    int index = luax_readmat4(L, indices ? 4 : 3, transform, 1);
    uint32_t start = luax_optu32(L, index++, 1) - 1;
    uint32_t count = luax_optu32(L, index++, ~0u);
    uint32_t instances = luax_optu32(L, index++, 1);
    uint32_t base = luax_optu32(L, index++, 0);
    if (count == ~0u && lua_type(L, 2) == LUA_TNUMBER) {
      count = lua_tointeger(L, 2);
    }
    lovrPassMesh(pass, vertices, indices, transform, start, count, instances, base);
  }
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

static int l_lovrPassClear(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  Buffer* buffer = luax_totype(L, 2, Buffer);

  if (buffer) {
    const BufferInfo* info = lovrBufferGetInfo(buffer);
    uint32_t index = luax_optu32(L, 3, 1);
    uint32_t count = luax_optu32(L, 4, info->length - index + 1);
    lovrBufferClear(buffer, (index - 1) * info->stride, count * info->stride);
    return 0;
  }

  Texture* texture = luax_totype(L, 2, Texture);

  if (texture) {
    float value[4];
    luax_optcolor(L, 3, value);
    uint32_t layer = luax_optu32(L, 4, 1) - 1;
    uint32_t layerCount = luax_optu32(L, 5, ~0u);
    uint32_t level = luax_optu32(L, 6, 1) - 1;
    uint32_t levelCount = luax_optu32(L, 7, ~0u);
    lovrPassClearTexture(pass, texture, value, layer, layerCount, level, levelCount);
    return 0;
  }

  return luax_typeerror(L, 2, "Buffer or Texture");
}

static int l_lovrPassCopy(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  if (lua_istable(L, 2)) {
    Buffer* buffer = luax_checkbuffer(L, 3);
    uint32_t srcIndex = luax_optu32(L, 4, 1) - 1;
    uint32_t dstIndex = luax_optu32(L, 5, 1) - 1;

    lua_rawgeti(L, 2, 1);
    bool nested = lua_istable(L, -1);
    lua_pop(L, 1);

    uint32_t length = luax_len(L, 2);
    const BufferInfo* info = lovrBufferGetInfo(buffer);
    uint32_t limit = nested ? MIN(length - srcIndex, info->length - dstIndex) : info->length - dstIndex;
    uint32_t count = luax_optu32(L, 6, limit);

    void* data = lovrPassCopyDataToBuffer(pass, buffer, dstIndex * info->stride, count * info->stride);
    lua_remove(L, 3); // table, srcIndex, dstIndex, count
    luax_readbufferdata(L, 2, buffer, data);
    return 0;
  }

  Blob* blob = luax_totype(L, 2, Blob);

  if (blob) {
    Buffer* buffer = luax_checkbuffer(L, 3);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    uint32_t dstOffset = luax_optu32(L, 5, 0);
    const BufferInfo* info = lovrBufferGetInfo(buffer);
    uint32_t limit = MIN(blob->size - srcOffset, info->length * info->stride - dstOffset);
    uint32_t extent = luax_optu32(L, 6, limit);
    lovrCheck(extent <= blob->size - srcOffset, "Buffer copy range exceeds Blob size");
    lovrCheck(extent <= info->length * info->stride - dstOffset, "Buffer copy offset exceeds Buffer size");
    void* data = lovrPassCopyDataToBuffer(pass, buffer, dstOffset, extent);
    memcpy(data, (char*) blob->data + srcOffset, extent);
    return 0;
  }

  Buffer* buffer = luax_totype(L, 2, Buffer);

  if (buffer) {
    Buffer* dst = luax_checkbuffer(L, 3);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    uint32_t dstOffset = luax_optu32(L, 5, 0);
    const BufferInfo* srcInfo = lovrBufferGetInfo(buffer);
    const BufferInfo* dstInfo = lovrBufferGetInfo(dst);
    uint32_t limit = MIN(srcInfo->length * srcInfo->stride - srcOffset, dstInfo->length * dstInfo->stride - dstOffset);
    uint32_t extent = luax_optu32(L, 6, limit);
    lovrPassCopyBufferToBuffer(pass, buffer, dst, srcOffset, dstOffset, extent);
    return 0;
  }

  Image* image = luax_totype(L, 2, Image);

  if (image) {
    Texture* texture = luax_checktype(L, 3, Texture);
    uint32_t srcOffset[4];
    uint32_t dstOffset[4];
    uint32_t extent[3];
    srcOffset[0] = luax_optu32(L, 4, 0);
    srcOffset[1] = luax_optu32(L, 5, 0);
    dstOffset[0] = luax_optu32(L, 6, 0);
    dstOffset[1] = luax_optu32(L, 7, 0);
    extent[0] = luax_optu32(L, 8, ~0u);
    extent[1] = luax_optu32(L, 9, ~0u);
    srcOffset[2] = luax_optu32(L, 10, 1) - 1;
    dstOffset[2] = luax_optu32(L, 11, 1) - 1;
    extent[2] = luax_optu32(L, 12, ~0u);
    srcOffset[3] = luax_optu32(L, 13, 1) - 1;
    dstOffset[3] = luax_optu32(L, 14, 1) - 1;
    lovrPassCopyImageToTexture(pass, image, texture, srcOffset, dstOffset, extent);
    return 0;
  }

  Texture* texture = luax_totype(L, 2, Texture);

  if (texture) {
    Texture* dst = luax_checktype(L, 3, Texture);
    uint32_t srcOffset[4];
    uint32_t dstOffset[4];
    uint32_t extent[3];
    srcOffset[0] = luax_optu32(L, 4, 0);
    srcOffset[1] = luax_optu32(L, 5, 0);
    dstOffset[0] = luax_optu32(L, 6, 0);
    srcOffset[1] = luax_optu32(L, 7, 0);
    extent[0] = luax_optu32(L, 8, ~0u);
    extent[1] = luax_optu32(L, 9, ~0u);
    srcOffset[2] = luax_optu32(L, 10, 1) - 1;
    dstOffset[2] = luax_optu32(L, 11, 1) - 1;
    extent[2] = luax_optu32(L, 12, ~0u);
    srcOffset[3] = luax_optu32(L, 13, 1) - 1;
    dstOffset[3] = luax_optu32(L, 14, 1) - 1;
    lovrPassCopyTextureToTexture(pass, texture, dst, srcOffset, dstOffset, extent);
    return 0;
  }

  Tally* tally = luax_totype(L, 2, Tally);

  if (tally) {
    Buffer* buffer = luax_checkbuffer(L, 3);
    uint32_t srcIndex = luax_optu32(L, 4, 0);
    uint32_t dstOffset = luax_optu32(L, 5, 0);
    uint32_t count = luax_optu32(L, 5, ~0u);
    lovrPassCopyTallyToBuffer(pass, tally, buffer, srcIndex, dstOffset, count);
    return 0;
  }

  return luax_typeerror(L, 2, "table, Blob, Buffer, Image, Texture, or Tally");
}

static int l_lovrPassBlit(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* src = luax_checktype(L, 2, Texture);
  Texture* dst = luax_checktype(L, 3, Texture);
  uint32_t srcOffset[4], dstOffset[4], srcExtent[3], dstExtent[3];
  srcOffset[0] = luax_optu32(L, 4, 0);
  srcOffset[1] = luax_optu32(L, 5, 0);
  srcOffset[2] = luax_optu32(L, 6, 1) - 1;
  dstOffset[0] = luax_optu32(L, 7, 0);
  dstOffset[1] = luax_optu32(L, 8, 0);
  dstOffset[2] = luax_optu32(L, 9, 1) - 1;
  srcExtent[0] = luax_optu32(L, 10, ~0u);
  srcExtent[1] = luax_optu32(L, 11, ~0u);
  srcExtent[2] = luax_optu32(L, 12, ~0u);
  dstExtent[0] = luax_optu32(L, 13, ~0u);
  dstExtent[1] = luax_optu32(L, 14, ~0u);
  dstExtent[2] = luax_optu32(L, 15, ~0u);
  srcOffset[3] = luax_optu32(L, 16, 1) - 1;
  dstOffset[3] = luax_optu32(L, 17, 1) - 1;
  FilterMode filter = luax_checkenum(L, 18, FilterMode, "linear");
  lovrPassBlit(pass, src, dst, srcOffset, dstOffset, srcExtent, dstExtent, filter);
  return 0;
}

static int l_lovrPassMipmap(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* texture = luax_checktype(L, 2, Texture);
  uint32_t base = luax_optu32(L, 3, 1) - 1;
  uint32_t count = luax_optu32(L, 4, ~0u);
  lovrPassMipmap(pass, texture, base, count);
  return 0;
}

static int l_lovrPassRead(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  Buffer* buffer = luax_totype(L, 2, Buffer);

  if (buffer) {
    const BufferInfo* info = lovrBufferGetInfo(buffer);
    uint32_t index = luax_optu32(L, 3, 1) - 1;
    uint32_t offset = index * info->stride;
    uint32_t extent = luax_optu32(L, 4, info->length - index) * info->stride;
    Readback* readback = lovrPassReadBuffer(pass, buffer, offset, extent);
    luax_pushtype(L, Readback, readback);
    lovrRelease(readback, lovrReadbackDestroy);
    return 1;
  }

  Texture* texture = luax_totype(L, 2, Texture);

  if (texture) {
    uint32_t offset[4], extent[3];
    offset[0] = luax_optu32(L, 3, 0);
    offset[1] = luax_optu32(L, 4, 0);
    offset[2] = luax_optu32(L, 5, 1) - 1;
    offset[3] = luax_optu32(L, 6, 1) - 1;
    extent[0] = luax_optu32(L, 7, ~0u);
    extent[1] = luax_optu32(L, 8, ~0u);
    extent[2] = 1;
    Readback* readback = lovrPassReadTexture(pass, texture, offset, extent);
    luax_pushtype(L, Readback, readback);
    lovrRelease(readback, lovrReadbackDestroy);
    return 1;
  }

  Tally* tally = luax_totype(L, 2, Tally);

  if (tally) {
    uint32_t index = luax_optu32(L, 3, 1) - 1;
    uint32_t count = luax_optu32(L, 4, lovrTallyGetInfo(tally)->count);
    Readback* readback = lovrPassReadTally(pass, tally, index, count);
    luax_pushtype(L, Readback, readback);
    lovrRelease(readback, lovrReadbackDestroy);
    return 1;
  }

  return luax_typeerror(L, 2, "Buffer, Texture, or Tally");
}

static int l_lovrPassTick(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Tally* tally = luax_checktype(L, 2, Tally);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrPassTick(pass, tally, index);
  return 0;
}

static int l_lovrPassTock(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Tally* tally = luax_checktype(L, 2, Tally);
  uint32_t index = luax_checku32(L, 3) - 1;
  lovrPassTock(pass, tally, index);
  return 0;
}

const luaL_Reg lovrPass[] = {
  { "getType", l_lovrPassGetType },
  { "getWidth", l_lovrPassGetWidth },
  { "getHeight", l_lovrPassGetHeight },
  { "getDimensions", l_lovrPassGetDimensions },
  { "getViewCount", l_lovrPassGetViewCount },
  { "getSampleCount", l_lovrPassGetSampleCount },
  { "getTarget", l_lovrPassGetTarget },
  { "getClear", l_lovrPassGetClear },

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
  { "setCullMode", l_lovrPassSetCullMode },
  { "setDepthTest", l_lovrPassSetDepthTest },
  { "setDepthWrite", l_lovrPassSetDepthWrite },
  { "setDepthOffset", l_lovrPassSetDepthOffset },
  { "setDepthClamp", l_lovrPassSetDepthClamp },
  { "setFont", l_lovrPassSetFont },
  { "setMaterial", l_lovrPassSetMaterial },
  { "setMeshMode", l_lovrPassSetMeshMode },
  { "setSampler", l_lovrPassSetSampler },
  { "setScissor", l_lovrPassSetScissor },
  { "setShader", l_lovrPassSetShader },
  { "setStencilTest", l_lovrPassSetStencilTest },
  { "setStencilWrite", l_lovrPassSetStencilWrite },
  { "setViewport", l_lovrPassSetViewport },
  { "setWinding", l_lovrPassSetWinding },
  { "setWireframe", l_lovrPassSetWireframe },

  { "send", l_lovrPassSend },

  { "points", l_lovrPassPoints },
  { "line", l_lovrPassLine },
  { "plane", l_lovrPassPlane },
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

  { "compute", l_lovrPassCompute },

  { "clear", l_lovrPassClear },
  { "copy", l_lovrPassCopy },
  { "blit", l_lovrPassBlit },
  { "mipmap", l_lovrPassMipmap },
  { "read", l_lovrPassRead },

  { "tick", l_lovrPassTick },
  { "tock", l_lovrPassTock },

  { NULL, NULL }
};
