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

static int l_lovrBatchBegin(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchBegin(batch);
  return 0;
}

static int l_lovrBatchFinish(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchFinish(batch);
  return 0;
}

static int l_lovrBatchIsActive(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool active = lovrBatchIsActive(batch);
  lua_pushboolean(L, active);
  return 1;
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
  float clamp = luax_optfloat(L, 4, 0.f);
  lovrBatchSetDepthNudge(batch, nudge, sloped, clamp);
  return 0;
}

static int l_lovrBatchSetDepthClamp(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool clamp = lua_toboolean(L, 2);
  lovrBatchSetDepthClamp(batch, clamp);
  return 0;
}

static int l_lovrBatchSetShader(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Shader* shader = lua_isnoneornil(L, 2) ? NULL : luax_checktype(L, 2, Shader);
  lovrBatchSetShader(batch, shader);
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

static int l_lovrBatchCube(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  DrawStyle style = luax_checkenum(L, 2, DrawStyle, NULL);
  float transform[16];
  luax_readmat4(L, 3, transform, 1);
  lovrBatchCube(batch, style, transform);
  return 0;
}

const luaL_Reg lovrBatch[] = {
  { "getType", l_lovrBatchGetType },
  { "getCapacity", l_lovrBatchGetCapacity },
  { "getCount", l_lovrBatchGetCount },
  { "begin", l_lovrBatchBegin },
  { "finish", l_lovrBatchFinish },
  { "isActive", l_lovrBatchIsActive },

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
  { "setColorMask", l_lovrBatchSetColorMask },
  { "setCullMode", l_lovrBatchSetCullMode },
  { "setDepthTest", l_lovrBatchSetDepthTest },
  { "setDepthWrite", l_lovrBatchSetDepthWrite },
  { "setDepthNudge", l_lovrBatchSetDepthNudge },
  { "setDepthClamp", l_lovrBatchSetDepthClamp },
  { "setShader", l_lovrBatchSetShader },
  { "setStencilTest", l_lovrBatchSetStencilTest },
  { "setStencilWrite", l_lovrBatchSetStencilWrite },
  { "setWinding", l_lovrBatchSetWinding },
  { "setWireframe", l_lovrBatchSetWireframe },

  { "bind", l_lovrBatchBind },

  { "cube", l_lovrBatchCube },

  { NULL, NULL }
};
