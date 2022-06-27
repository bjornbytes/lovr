#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/maf.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static int l_lovrPassGetType(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  const PassInfo* info = lovrPassGetInfo(pass);
  luax_pushenum(L, PassType, info->type);
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
  if (lua_type(L, 3) == LUA_TSTRING && !strcmp(lua_tostring(L, 3), "orthographic")) {
    float ortho[16];
    float width = luax_checkfloat(L, 4);
    float height = luax_checkfloat(L, 5);
    float near = luax_optfloat(L, 6, -1.f);
    float far = luax_optfloat(L, 7, 1.f);
    mat4_orthographic(ortho, 0.f, width, 0.f, height, near, far);
    lovrPassSetProjection(pass, view, ortho);
  } else if (lua_type(L, 3) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 3);
    float right = luax_checkfloat(L, 4);
    float up = luax_checkfloat(L, 5);
    float down = luax_checkfloat(L, 6);
    float clipNear = luax_optfloat(L, 7, .01f);
    float clipFar = luax_optfloat(L, 8, 100.f);
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
  if (lua_gettop(L) <= 1) {
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

static int l_lovrPassSetMaterial(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Material* material = luax_totype(L, 2, Material);
  Texture* texture = luax_totype(L, 2, Texture);
  lovrPassSetMaterial(pass, material, texture);
  return 0;
}

static int l_lovrPassSetVertexMode(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  VertexMode mode = luax_checkenum(L, 2, VertexMode, NULL);
  lovrPassSetVertexMode(pass, mode);
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

  Buffer* buffer = luax_totype(L, 3, Buffer);

  if (buffer) {
    uint32_t offset = lua_tointeger(L, 4);
    uint32_t extent = lua_tointeger(L, 5);
    lovrPassSendBuffer(pass, name, length, slot, buffer, offset, extent);
    return 0;
  }

  Texture* texture = luax_totype(L, 4, Texture);

  if (texture) {
    lovrPassSendTexture(pass, name, length, slot, texture);
    return 0;
  }

  Sampler* sampler = luax_totype(L, 4, Sampler);

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
  luax_readbufferfield(L, 3, type, data);
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
      } else if (innerType == LUA_TUSERDATA) {
        for (uint32_t i = 0; i < count; i++) {
          lua_rawgeti(L, index, i + 1);
          vec3_init(vertices, luax_checkvector(L, -1, V_VEC3, NULL));
          lua_pop(L, 1);
          vertices += 3;
        }
      }
      break;
    case LUA_TUSERDATA:
      for (uint32_t i = 0; i < count; i++) {
        vec3_init(vertices, luax_checkvector(L, index + i, V_VEC3, NULL));
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

static int l_lovrPassTorus(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, -2);
  uint32_t segmentsT = luax_optu32(L, index++, 64);
  uint32_t segmentsP = luax_optu32(L, index++, 32);
  lovrPassTorus(pass, transform, segmentsT, segmentsP);
  return 0;
}

static int l_lovrPassCylinder(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  float transform[16];
  // TODO vec3+vec3
  int index = luax_readmat4(L, 2, transform, -2);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  float angle1 = luax_optfloat(L, index++, 0.f);
  float angle2 = luax_optfloat(L, index++, 2.f * (float) M_PI);
  uint32_t segments = luax_optu32(L, index++, 64);
  lovrPassCylinder(pass, transform, capped, angle1, angle2, segments);
  return 0;
}

static int l_lovrPassText(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Font* font = luax_totype(L, 2, Font);
  int index = font ? 3 : 2;
  size_t length;
  const char* text = luaL_checklstring(L, index++, &length);
  float transform[16];
  index = luax_readmat4(L, index++, transform, 1);
  float wrap = luax_optfloat(L, index++, 0.);
  HorizontalAlign halign = luax_checkenum(L, index++, HorizontalAlign, "center");
  VerticalAlign valign = luax_checkenum(L, index++, VerticalAlign, "middle");
  lovrPassText(pass, font, text, (uint32_t)length, transform, wrap, halign, valign);
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

static int l_lovrPassMesh(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Buffer* vertices = !lua_toboolean(L, 2) ? NULL : luax_totype(L, 2, Buffer);
  Buffer* indices = luax_totype(L, 3, Buffer);
  float transform[16];
  int index = luax_readmat4(L, indices ? 4 : 3, transform, 1);
  uint32_t start = luax_optu32(L, index++, 1) - 1;
  uint32_t count = luax_optu32(L, index++, ~0u);
  uint32_t instances = luax_optu32(L, index, 1);
  lovrPassMesh(pass, vertices, indices, transform, start, count, instances);
  return 0;
}

static int l_lovrPassMultimesh(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Buffer* vertices = !lua_toboolean(L, 2) ? NULL : luax_totype(L, 2, Buffer);
  Buffer* indices = luax_totype(L, 3, Buffer);
  Buffer* draws = luax_checktype(L, 4, Buffer);
  uint32_t count = luax_optu32(L, 5, 1);
  uint32_t offset = luax_optu32(L, 6, 0);
  uint32_t stride = luax_optu32(L, 7, 0);
  lovrPassMultimesh(pass, vertices, indices, draws, count, offset, stride);
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
    uint32_t offset = luax_optu32(L, 3, 0);
    uint32_t extent = luax_optu32(L, 4, ~0u);
    lovrPassClearBuffer(pass, buffer, offset, extent);
    return 0;
  }

  Texture* texture = luax_totype(L, 2, Texture);

  if (texture) {
    float value[4];
    luax_readcolor(L, 3, value);
    int index = lua_istable(L, 3) ? 4 : 6;
    uint32_t layer = luax_optu32(L, index++, 1) - 1;
    uint32_t layerCount = luax_optu32(L, index++, ~0u);
    uint32_t level = luax_optu32(L, index++, 1) - 1;
    uint32_t levelCount = luax_optu32(L, index++, ~0u);
    lovrPassClearTexture(pass, texture, value, layer, layerCount, level, levelCount);
    return 0;
  }

  return luax_typeerror(L, 2, "Buffer or Texture");
}

static int l_lovrPassCopy(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);

  if (lua_istable(L, 2)) {
    Buffer* buffer = luax_checktype(L, 3, Buffer);
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
    Buffer* buffer = luax_checktype(L, 3, Buffer);
    graphics_size srcOffset = luax_optgraphics_size(L, 4, 0);
    graphics_size dstOffset = luax_optgraphics_size(L, 5, 0);
    const BufferInfo* info = lovrBufferGetInfo(buffer);
    graphics_size limit = (graphics_size)(MIN(blob->size - srcOffset, info->length * info->stride - dstOffset));
    graphics_size extent = luax_optgraphics_size(L, 6, limit);
    lovrCheck(extent <= blob->size - srcOffset, "Buffer copy range exceeds Blob size");
    lovrCheck(extent <= info->length * info->stride - dstOffset, "Buffer copy offset exceeds Buffer size");
    void* data = lovrPassCopyDataToBuffer(pass, buffer, dstOffset, extent);
    memcpy(data, (char*) blob->data + srcOffset, extent);
    return 0;
  }

  Buffer* buffer = luax_totype(L, 2, Buffer);

  if (buffer) {
    Buffer* dst = luax_checktype(L, 3, Buffer);
    uint32_t srcOffset = luax_optu32(L, 4, 0);
    uint32_t dstOffset = luax_optu32(L, 5, 0);
    const BufferInfo* srcInfo = lovrBufferGetInfo(buffer);
    const BufferInfo* dstInfo = lovrBufferGetInfo(dst);
    uint32_t limit = MIN(srcInfo->length * srcInfo->stride - srcOffset, dstInfo->length * dstInfo->stride - dstOffset);
    uint32_t extent = luax_optu32(L, 6, limit);
    lovrPassCopyBufferToBuffer(pass, buffer, dst, srcOffset, dstOffset, extent);
    return 0;
  }

  Image* image = luax_checktype(L, 2, Image);

  if (image) {
    Texture* texture = luax_checktype(L, 3, Texture);
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
    lovrPassCopyImageToTexture(pass, image, texture, srcOffset, dstOffset, extent);
    return 0;
  }

  Texture* texture = luax_checktype(L, 2, Texture);

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

  return luax_typeerror(L, 2, "Blob, Buffer, Image, or Texture");
}

static int l_lovrPassBlit(lua_State* L) {
  Pass* pass = luax_checktype(L, 1, Pass);
  Texture* src = luax_checktype(L, 2, Texture);
  Texture* dst = luax_checktype(L, 3, Texture);
  uint32_t srcOffset[4], dstOffset[4], srcExtent[3], dstExtent[3];
  srcOffset[0] = luax_optu32(L, 4, 0);
  srcOffset[1] = luax_optu32(L, 5, 0);
  srcOffset[2] = luax_optu32(L, 6, 0);
  dstOffset[0] = luax_optu32(L, 7, 0);
  dstOffset[1] = luax_optu32(L, 8, 0);
  dstOffset[2] = luax_optu32(L, 9, 0);
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
  uint32_t base = luax_optu32(L, 3, 0);
  uint32_t count = luax_optu32(L, 4, ~0u);
  lovrPassMipmap(pass, texture, base, count);
  return 0;
}

const luaL_Reg lovrPass[] = {
  { "getType", l_lovrPassGetType },

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
  { "setMaterial", l_lovrPassSetMaterial },
  { "setSampler", l_lovrPassSetSampler },
  { "setScissor", l_lovrPassSetScissor },
  { "setShader", l_lovrPassSetShader },
  { "setStencilTest", l_lovrPassSetStencilTest },
  { "setStencilWrite", l_lovrPassSetStencilWrite },
  { "setVertexMode", l_lovrPassSetVertexMode },
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
  { "torus", l_lovrPassTorus },
  { "cylinder", l_lovrPassCylinder },
  { "text", l_lovrPassText },
  { "fill", l_lovrPassFill },
  { "monkey", l_lovrPassMonkey },
  { "mesh", l_lovrPassMesh },
  { "multimesh", l_lovrPassMultimesh },

  { "compute", l_lovrPassCompute },

  { "clear", l_lovrPassClear },
  { "copy", l_lovrPassCopy },
  { "blit", l_lovrPassBlit },
  { "mipmap", l_lovrPassMipmap },

  { NULL, NULL }
};
