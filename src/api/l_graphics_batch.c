#include "api.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrBatchGetType(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  const BatchInfo* info = lovrBatchGetInfo(batch);
  luax_pushenum(L, BatchType, info->type);
  return 1;
}

static int l_lovrBatchGetCapacity(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  const BatchInfo* info = lovrBatchGetInfo(batch);
  lua_pushinteger(L, info->capacity);
  return 1;
}

static int l_lovrBatchGetCount(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t count = lovrBatchGetCount(batch);
  lua_pushinteger(L, count);
  return 1;
}

static int l_lovrBatchReset(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchReset(batch);
  return 0;
}

static bool luax_filterpredicate(void* context, uint32_t i) {
  lua_State* L = context;
  lua_pushvalue(L, -1);
  lua_pushinteger(L, i);
  lua_call(L, 1, 1);
  bool result = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return result;
}

static int l_lovrBatchFilter(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_settop(L, 2);
  lovrBatchFilter(batch, luax_filterpredicate, L);
  return 0;
}

static int l_lovrBatchGetViewport(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float viewport[4], depthRange[2];
  lovrBatchGetViewport(batch, viewport, depthRange);
  if (viewport[2] == 0.f && viewport[3] == 0.f) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushnumber(L, viewport[0]);
  lua_pushnumber(L, viewport[1]);
  lua_pushnumber(L, viewport[2]);
  lua_pushnumber(L, viewport[3]);
  lua_pushnumber(L, depthRange[0]);
  lua_pushnumber(L, depthRange[1]);
  return 6;
}

static int l_lovrBatchSetViewport(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float viewport[4], depthRange[2];

  if (lua_isnil(L, 2)) {
    memset(viewport, 0, sizeof(viewport));
    memset(depthRange, 0, sizeof(depthRange));
    lovrBatchSetViewport(batch, viewport, depthRange);
    return 0;
  }

  viewport[0] = luax_checkfloat(L, 2);
  viewport[1] = luax_checkfloat(L, 3);
  viewport[2] = luax_checkfloat(L, 4);
  viewport[3] = luax_checkfloat(L, 5);
  depthRange[0] = luax_checkfloat(L, 6);
  depthRange[1] = luax_checkfloat(L, 7);
  lovrBatchSetViewport(batch, viewport, depthRange);
  return 0;
}

static int l_lovrBatchGetScissor(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t scissor[4];
  lovrBatchGetScissor(batch, scissor);

  if (scissor[2] == 0 && scissor[3] == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushinteger(L, scissor[0]);
  lua_pushinteger(L, scissor[1]);
  lua_pushinteger(L, scissor[2]);
  lua_pushinteger(L, scissor[3]);
  return 4;
}

static int l_lovrBatchSetScissor(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t scissor[4];

  if (lua_isnil(L, 2)) {
    memset(scissor, 0, sizeof(scissor));
    lovrBatchSetScissor(batch, scissor);
    return 0;
  }

  scissor[0] = luaL_checkinteger(L, 2);
  scissor[1] = luaL_checkinteger(L, 3);
  scissor[2] = luaL_checkinteger(L, 4);
  scissor[3] = luaL_checkinteger(L, 5);
  lovrBatchSetScissor(batch, scissor);
  return 0;
}

static int l_lovrBatchGetViewPose(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    bool invert = lua_toboolean(L, 4);
    lovrBatchGetViewMatrix(batch, view, matrix);
    if (!invert) mat4_invert(matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], angle, ax, ay, az;
    lovrBatchGetViewMatrix(batch, view, matrix);
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

static int l_lovrBatchSetViewPose(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  VectorType type;
  float* p = luax_tovector(L, 3, &type);
  if (p && type == V_MAT4) {
    float matrix[16];
    mat4_init(matrix, p);
    bool inverted = lua_toboolean(L, 3);
    if (!inverted) mat4_invert(matrix);
    lovrBatchSetViewMatrix(batch, view, matrix);
  } else {
    int index = 3;
    float position[4], orientation[4], matrix[16];
    index = luax_readvec3(L, index, position, "vec3, number, or mat4");
    index = luax_readquat(L, index, orientation, NULL);
    mat4_fromQuat(matrix, orientation);
    memcpy(matrix + 12, position, 3 * sizeof(float));
    mat4_invert(matrix);
    lovrBatchSetViewMatrix(batch, view, matrix);
  }
  return 0;
}

static int l_lovrBatchGetProjection(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    lovrBatchGetProjection(batch, view, matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], left, right, up, down;
    lovrBatchGetProjection(batch, view, matrix);
    mat4_getFov(matrix, &left, &right, &up, &down);
    lua_pushnumber(L, left);
    lua_pushnumber(L, right);
    lua_pushnumber(L, up);
    lua_pushnumber(L, down);
    return 4;
  }
}

static int l_lovrBatchSetProjection(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_type(L, 3) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 3);
    float right = luax_checkfloat(L, 4);
    float up = luax_checkfloat(L, 5);
    float down = luax_checkfloat(L, 6);
    float clipNear = luax_optfloat(L, 7, .01f);
    float clipFar = luax_optfloat(L, 8, 100.f);
    float matrix[16];
    mat4_fov(matrix, left, right, up, down, clipNear, clipFar);
    lovrBatchSetProjection(batch, view, matrix);
  } else {
    float* matrix = luax_checkvector(L, 2, V_MAT4, "mat4 or number");
    lovrBatchSetProjection(batch, view, matrix);
  }
  return 0;
}

static int l_lovrBatchPush(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  StackType type = luax_checkenum(L, 2, StackType, "transform");
  lovrBatchPush(batch, type);
  return 0;
}

static int l_lovrBatchPop(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  StackType type = luax_checkenum(L, 2, StackType, "transform");
  lovrBatchPop(batch, type);
  return 0;
}

static int l_lovrBatchOrigin(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchOrigin(batch);
  return 0;
}

static int l_lovrBatchTranslate(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float translation[4];
  luax_readvec3(L, 2, translation, NULL);
  lovrBatchTranslate(batch, translation);
  return 0;
}

static int l_lovrBatchRotate(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float rotation[4];
  luax_readquat(L, 2, rotation, NULL);
  lovrBatchRotate(batch, rotation);
  return 0;
}

static int l_lovrBatchScale(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float scale[4];
  luax_readscale(L, 2, scale, 3, NULL);
  lovrBatchScale(batch, scale);
  return 0;
}

static int l_lovrBatchTransform(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float transform[16];
  luax_readmat4(L, 2, transform, 3);
  lovrBatchTransform(batch, transform);
  return 0;
}

static int l_lovrBatchSetAlphaToCoverage(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchSetAlphaToCoverage(batch, lua_toboolean(L, 2));
  return 1;
}

static int l_lovrBatchSetBlendMode(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  BlendMode mode = lua_isnoneornil(L, 2) ? BLEND_NONE : luax_checkenum(L, 2, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 3, BlendAlphaMode, "alphamultiply");
  lovrBatchSetBlendMode(batch, mode, alphaMode);
  return 0;
}

static int l_lovrBatchSetColor(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float color[4];
  luax_readcolor(L, 2, color);
  lovrBatchSetColor(batch, color);
  return 0;
}

static int l_lovrBatchSetColorMask(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool r = lua_toboolean(L, 2);
  bool g = lua_toboolean(L, 3);
  bool b = lua_toboolean(L, 4);
  bool a = lua_toboolean(L, 5);
  lovrBatchSetColorMask(batch, r, g, b, a);
  return 0;
}

static int l_lovrBatchSetCullMode(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  CullMode mode = luax_checkenum(L, 2, CullMode, "none");
  lovrBatchSetCullMode(batch, mode);
  return 0;
}

static int l_lovrBatchSetDepthTest(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  CompareMode test = lua_isnoneornil(L, 2) ? COMPARE_NONE : luax_checkenum(L, 2, CompareMode, NULL);
  lovrBatchSetDepthTest(batch, test);
  return 0;
}

static int l_lovrBatchSetDepthWrite(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool write = lua_toboolean(L, 2);
  lovrBatchSetDepthWrite(batch, write);
  return 0;
}

static int l_lovrBatchSetDepthNudge(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float nudge = luax_optfloat(L, 2, 0.f);
  float sloped = luax_optfloat(L, 3, 0.f);
  lovrBatchSetDepthNudge(batch, nudge, sloped);
  return 0;
}

static int l_lovrBatchSetDepthClamp(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool clamp = lua_toboolean(L, 2);
  lovrBatchSetDepthClamp(batch, clamp);
  return 0;
}

static int l_lovrBatchSetStencilTest(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  if (lua_isnoneornil(L, 2)) {
    lovrBatchSetStencilTest(batch, COMPARE_NONE, 0, 0xff);
  } else {
    CompareMode test = luax_checkenum(L, 2, CompareMode, NULL);
    uint8_t value = luaL_checkinteger(L, 3);
    uint8_t mask = luaL_optinteger(L, 4, 0xff);
    lovrBatchSetStencilTest(batch, test, value, mask);
  }
  return 0;
}

static int l_lovrBatchSetStencilWrite(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  StencilAction actions[3];
  if (lua_isnoneornil(L, 2)) {
    actions[0] = actions[1] = actions[2] = STENCIL_KEEP;
    lovrBatchSetStencilWrite(batch, actions, 0, 0xff);
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
    lovrBatchSetStencilWrite(batch, actions, value, mask);
  }
  return 0;
}

static int l_lovrBatchSetWinding(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Winding winding = luax_checkenum(L, 2, Winding, NULL);
  lovrBatchSetWinding(batch, winding);
  return 0;
}

static int l_lovrBatchSetWireframe(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool wireframe = lua_toboolean(L, 2);
  lovrBatchSetWireframe(batch, wireframe);
  return 0;
}

static int l_lovrBatchSetShader(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Shader* shader = lua_isnoneornil(L, 2) ? NULL : luax_checktype(L, 2, Shader);
  lovrBatchSetShader(batch, shader);
  return 0;
}

static int l_lovrBatchBind(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  const char* name = NULL;
  size_t length = 0;
  uint32_t slot = ~0u;

  switch (lua_type(L, 2)) {
    case LUA_TSTRING: name = lua_tolstring(L, 2, &length); break;
    case LUA_TNUMBER: slot = lua_tointeger(L, 2) - 1; break;
    default: return luax_typeerror(L, 2, "string or number");
  }

  Buffer* buffer = luax_totype(L, 3, Buffer);
  Texture* texture = NULL;
  uint32_t offset = 0;

  if (buffer) {
    offset = lua_tointeger(L, 4);
  } else {
    texture = luax_totype(L, 3, Texture);
    if (!texture) {
      return luax_typeerror(L, 3, "Buffer or Texture");
    }
  }

  lovrBatchBind(batch, name, length, slot, buffer, offset, texture);
  return 0;
}

static int l_lovrBatchMesh(lua_State* L) {
  int index = 1;
  Batch* batch = luax_checktype(L, index++, Batch);
  DrawMode mode = lua_type(L, index) == LUA_TSTRING ? luax_checkenum(L, index++, DrawMode, NULL) : DRAW_TRIANGLES;
  Buffer* vertices = luax_totype(L, index++, Buffer);
  Buffer* indices = luax_totype(L, index, Buffer);
  if (indices) index++;
  float transform[16];
  index = luax_readmat4(L, index, transform, 1);
  uint32_t start = luaL_optinteger(L, index++, 1) - 1;
  uint32_t limit = lovrBufferGetInfo(indices ? indices : vertices)->length;
  uint32_t count = luaL_optinteger(L, index++, limit);
  uint32_t instances = luaL_optinteger(L, index++, 1);
  uint32_t id = lovrBatchMesh(batch, &(DrawInfo) {
    .mode = mode,
    .vertex.buffer = vertices,
    .index.buffer = indices,
    .start = start,
    .count = count,
    .instances = instances
  }, transform);
  lua_pushinteger(L, id);
  return 1;
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

static int l_lovrBatchPoints(lua_State* L) {
  float* vertices;
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t count = luax_getvertexcount(L, 2);
  uint32_t id = lovrBatchPoints(batch, count, &vertices);
  luax_readvertices(L, 2, vertices, count);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchLine(lua_State* L) {
  float* vertices;
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t count = luax_getvertexcount(L, 2);
  uint32_t id = lovrBatchLine(batch, count, &vertices);
  luax_readvertices(L, 2, vertices, count);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchPlane(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  DrawStyle style = luax_checkenum(L, 2, DrawStyle, NULL);
  float transform[16];
  int index = luax_readmat4(L, 3, transform, 2);
  uint32_t segments = luaL_optinteger(L, index, 0);
  uint32_t id = lovrBatchPlane(batch, style, transform, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchCube(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  DrawStyle style = luax_checkenum(L, 2, DrawStyle, NULL);
  float transform[16];
  luax_readmat4(L, 3, transform, 1);
  uint32_t id = lovrBatchBox(batch, style, transform);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchBox(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  DrawStyle style = luax_checkenum(L, 2, DrawStyle, NULL);
  float transform[16];
  luax_readmat4(L, 3, transform, 3);
  uint32_t id = lovrBatchBox(batch, style, transform);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchCircle(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  DrawStyle style = luax_checkenum(L, 2, DrawStyle, NULL);
  float transform[16];
  int index = luax_readmat4(L, 3, transform, 1);
  uint32_t segments = luaL_optinteger(L, index, 32);
  uint32_t id = lovrBatchCircle(batch, style, transform, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchCylinder(lua_State* L) {
  float transform[16];
  Batch* batch = luax_checktype(L, 1, Batch);
  int index = luax_readmat4(L, 2, transform, 1);
  float r1 = luax_optfloat(L, index++, 1.f);
  float r2 = luax_optfloat(L, index++, 1.f);
  bool capped = lua_isnoneornil(L, index) ? true : lua_toboolean(L, index++);
  uint32_t segments = luaL_optinteger(L, index, 32);
  uint32_t id = lovrBatchCylinder(batch, transform, r1, r2, capped, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchSphere(lua_State* L) {
  float transform[16];
  Batch* batch = luax_checktype(L, 1, Batch);
  int index = luax_readmat4(L, 2, transform, 1);
  uint32_t segments = luaL_optinteger(L, index, 32);
  uint32_t id = lovrBatchSphere(batch, transform, segments);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchSkybox(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Texture* texture = luax_checktype(L, 2, Texture);
  uint32_t id = lovrBatchSkybox(batch, texture);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchFill(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Texture* texture = luax_totype(L, 2, Texture);
  uint32_t id = lovrBatchFill(batch, texture);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchModel(lua_State* L) {
  float transform[16];
  Batch* batch = luax_checktype(L, 1, Batch);
  Model* model = luax_checktype(L, 2, Model);
  int index = luax_readmat4(L, 3, transform, 1);
  uint32_t node = luaL_optinteger(L, index++, ~0u); // TODO string
  bool children = lua_isnil(L, index) ? (index++, true) : lua_toboolean(L, index++);
  uint32_t instances = luaL_optinteger(L, index++, 1);
  uint32_t id = lovrBatchModel(batch, model, transform, node, children, instances);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchPrint(lua_State* L) {
  size_t length;
  float transform[16];
  Batch* batch = luax_checktype(L, 1, Batch);
  Font* font = luax_checktype(L, 2, Font);
  const char* text = luaL_checklstring(L, 3, &length);
  int index = luax_readmat4(L, 4, transform, 1);
  float wrap = luax_optfloat(L, index++, 0.f);
  HorizontalAlign halign = luax_checkenum(L, index++, HorizontalAlign, "center");
  VerticalAlign valign = luax_checkenum(L, index++, VerticalAlign, "middle");
  uint32_t id = lovrBatchPrint(batch, font, text, length, transform, wrap, halign, valign);
  lua_pushinteger(L, id);
  return 1;
}

static int l_lovrBatchCompute(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Buffer* buffer = luax_totype(L, 2, Buffer);
  uint32_t id;
  if (buffer) {
    uint32_t offset = lua_tointeger(L, 3);
    id = lovrBatchCompute(batch, 0, 0, 0, buffer, offset);
  } else {
    uint32_t x = luaL_optinteger(L, 2, 1);
    uint32_t y = luaL_optinteger(L, 3, 1);
    uint32_t z = luaL_optinteger(L, 4, 1);
    id = lovrBatchCompute(batch, x, y, z, NULL, 0);
  }
  lua_pushinteger(L, id);
  return 1;
}

const luaL_Reg lovrBatch[] = {
  { "getType", l_lovrBatchGetType },
  { "getCapacity", l_lovrBatchGetCapacity },
  { "getCount", l_lovrBatchGetCount },
  { "reset", l_lovrBatchReset },

  { "filter", l_lovrBatchFilter },

  { "getViewport", l_lovrBatchGetViewport },
  { "setViewport", l_lovrBatchSetViewport },
  { "getScissor", l_lovrBatchGetScissor },
  { "setScissor", l_lovrBatchSetScissor },
  { "getViewPose", l_lovrBatchGetViewPose },
  { "setViewPose", l_lovrBatchSetViewPose },
  { "getProjection", l_lovrBatchGetProjection },
  { "setProjection", l_lovrBatchSetProjection },

  { "push", l_lovrBatchPush },
  { "pop", l_lovrBatchPop },
  { "origin", l_lovrBatchOrigin },
  { "translate", l_lovrBatchTranslate },
  { "rotate", l_lovrBatchRotate },
  { "scale", l_lovrBatchScale },
  { "transform", l_lovrBatchTransform },

  { "setAlphaToCoverage", l_lovrBatchSetAlphaToCoverage },
  { "setBlendMode", l_lovrBatchSetBlendMode },
  { "setColor", l_lovrBatchSetColor },
  { "setColorMask", l_lovrBatchSetColorMask },
  { "setCullMode", l_lovrBatchSetCullMode },
  { "setDepthTest", l_lovrBatchSetDepthTest },
  { "setDepthWrite", l_lovrBatchSetDepthWrite },
  { "setDepthNudge", l_lovrBatchSetDepthNudge },
  { "setDepthClamp", l_lovrBatchSetDepthClamp },
  { "setStencilTest", l_lovrBatchSetStencilTest },
  { "setStencilWrite", l_lovrBatchSetStencilWrite },
  { "setWinding", l_lovrBatchSetWinding },
  { "setWireframe", l_lovrBatchSetWireframe },

  { "setShader", l_lovrBatchSetShader },
  { "bind", l_lovrBatchBind },

  { "mesh", l_lovrBatchMesh },
  { "points", l_lovrBatchPoints },
  { "line", l_lovrBatchLine },
  { "plane", l_lovrBatchPlane },
  { "cube", l_lovrBatchCube },
  { "box", l_lovrBatchBox },
  { "circle", l_lovrBatchCircle },
  { "cylinder", l_lovrBatchCylinder },
  { "sphere", l_lovrBatchSphere },
  { "skybox", l_lovrBatchSkybox },
  { "fill", l_lovrBatchFill },
  { "model", l_lovrBatchModel },
  { "print", l_lovrBatchPrint },

  { "compute", l_lovrBatchCompute },

  { NULL, NULL }
};
