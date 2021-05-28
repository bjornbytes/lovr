#include "api/api.h"
#include "graphics/graphics.h"
#include "data/image.h"
#include "core/maf.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

static int l_lovrCanvasBegin(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrCanvasBegin(canvas);
  return 0;
}

static int l_lovrCanvasFinish(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrCanvasFinish(canvas);
  return 0;
}

static int l_lovrCanvasIsActive(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushboolean(L, lovrCanvasIsActive(canvas));
  return 1;
}

static int l_lovrCanvasGetViewPose(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    bool invert = lua_toboolean(L, 4);
    lovrCanvasGetViewMatrix(canvas, view, matrix);
    if (!invert) mat4_invert(matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], angle, ax, ay, az;
    lovrCanvasGetViewMatrix(canvas, view, matrix);
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

static int l_lovrCanvasSetViewPose(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  VectorType type;
  float* p = luax_tovector(L, 3, &type);
  if (p && type == V_MAT4) {
    float matrix[16];
    mat4_init(matrix, p);
    bool inverted = lua_toboolean(L, 3);
    if (!inverted) mat4_invert(matrix);
    lovrCanvasSetViewMatrix(canvas, view, matrix);
  } else {
    int index = 3;
    float position[4], orientation[4], matrix[16];
    index = luax_readvec3(L, index, position, "vec3, number, or mat4");
    index = luax_readquat(L, index, orientation, NULL);
    mat4_fromQuat(matrix, orientation);
    memcpy(matrix + 12, position, 3 * sizeof(float));
    mat4_invert(matrix);
    lovrCanvasSetViewMatrix(canvas, view, matrix);
  }
  return 0;
}

static int l_lovrCanvasGetProjection(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_gettop(L) > 2) {
    float* matrix = luax_checkvector(L, 3, V_MAT4, NULL);
    lovrCanvasGetProjection(canvas, view, matrix);
    lua_settop(L, 3);
    return 1;
  } else {
    float matrix[16], left, right, up, down;
    lovrCanvasGetProjection(canvas, view, matrix);
    mat4_getFov(matrix, &left, &right, &up, &down);
    lua_pushnumber(L, left);
    lua_pushnumber(L, right);
    lua_pushnumber(L, up);
    lua_pushnumber(L, down);
    return 4;
  }
}

static int l_lovrCanvasSetProjection(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t view = luaL_checkinteger(L, 2) - 1;
  lovrAssert(view < 6, "Invalid view index %d", view + 1);
  if (lua_type(L, 3) == LUA_TNUMBER) {
    float left = luax_checkfloat(L, 3);
    float right = luax_checkfloat(L, 4);
    float up = luax_checkfloat(L, 5);
    float down = luax_checkfloat(L, 6);
    float clipNear = luax_optfloat(L, 7, .1f);
    float clipFar = luax_optfloat(L, 8, 100.f);
    float matrix[16];
    mat4_fov(matrix, left, right, up, down, clipNear, clipFar);
    lovrCanvasSetProjection(canvas, view, matrix);
  } else {
    float* matrix = luax_checkvector(L, 2, V_MAT4, "mat4 or number");
    lovrCanvasSetProjection(canvas, view, matrix);
  }
  return 0;
}

static int l_lovrCanvasPush(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrCanvasPush(canvas);
  return 0;
}

static int l_lovrCanvasPop(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrCanvasPop(canvas);
  return 0;
}

static int l_lovrCanvasOrigin(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrCanvasOrigin(canvas);
  return 0;
}

static int l_lovrCanvasTranslate(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  float translation[4];
  luax_readvec3(L, 2, translation, NULL);
  lovrCanvasTranslate(canvas, translation);
  return 0;
}

static int l_lovrCanvasRotate(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  float rotation[4];
  luax_readquat(L, 2, rotation, NULL);
  lovrCanvasRotate(canvas, rotation);
  return 0;
}

static int l_lovrCanvasScale(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  float scale[4];
  luax_readscale(L, 2, scale, 3, NULL);
  lovrCanvasScale(canvas, scale);
  return 0;
}

static int l_lovrCanvasTransform(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  float transform[16];
  luax_readmat4(L, 2, transform, 3);
  lovrCanvasTransform(canvas, transform);
  return 0;
}

static int l_lovrCanvasGetAlphaToCoverage(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lua_pushboolean(L, lovrCanvasGetAlphaToCoverage(canvas));
  return 1;
}

static int l_lovrCanvasSetAlphaToCoverage(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  lovrCanvasSetAlphaToCoverage(canvas, lua_toboolean(L, 2));
  return 1;
}

static int l_lovrCanvasGetBlendMode(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t target = luaL_optinteger(L, 2, 1) - 1;
  lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
  BlendMode mode;
  BlendAlphaMode alphaMode;
  lovrCanvasGetBlendMode(canvas, target, &mode, &alphaMode);
  if (mode == BLEND_NONE) {
    lua_pushnil(L);
    return 1;
  } else {
    luax_pushenum(L, BlendMode, mode);
    luax_pushenum(L, BlendAlphaMode, alphaMode);
    return 2;
  }
}

static int l_lovrCanvasSetBlendMode(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    uint32_t target = lua_tonumber(L, 2) - 1;
    lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
    BlendMode mode = lua_isnoneornil(L, 3) ? BLEND_NONE : luax_checkenum(L, 3, BlendMode, NULL);
    BlendAlphaMode alphaMode = luax_checkenum(L, 4, BlendAlphaMode, "alphamultiply");
    lovrCanvasSetBlendMode(canvas, target, mode, alphaMode);
    return 0;
  }

  BlendMode mode = lua_isnoneornil(L, 2) ? BLEND_NONE : luax_checkenum(L, 2, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 3, BlendAlphaMode, "alphamultiply");
  for (uint32_t i = 0; i < 4; i++) {
    lovrCanvasSetBlendMode(canvas, i, mode, alphaMode);
  }
  return 0;
}

static int l_lovrCanvasGetClear(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  float color[MAX_COLOR_ATTACHMENTS][4];
  float depth;
  uint8_t stencil;
  lovrCanvasGetClear(canvas, color, &depth, &stencil);
  lua_createtable(L, info->count, 2);
  for (uint32_t i = 0; i < info->count; i++) {
    lua_createtable(L, 4, 0);
    lua_pushnumber(L, color[i][0]);
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, color[i][1]);
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, color[i][2]);
    lua_rawseti(L, -2, 3);
    lua_pushnumber(L, color[i][3]);
    lua_rawseti(L, -2, 4);
    lua_rawseti(L, -2, i + 1);
  }
  if (info->depth.enabled) {
    lua_pushnumber(L, depth);
    lua_setfield(L, -2, "depth");
    if (info->depth.format == FORMAT_D24S8) {
      lua_pushinteger(L, stencil);
      lua_setfield(L, -2, "stencil");
    }
  }
  return 1;
}

static int l_lovrCanvasSetClear(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  const CanvasInfo* info = lovrCanvasGetInfo(canvas);
  float color[MAX_COLOR_ATTACHMENTS][4];
  float depth;
  uint8_t stencil;
  lovrCanvasGetClear(canvas, color, &depth, &stencil);
  if (lua_istable(L, 2)) {
    for (uint32_t i = 0; i < info->count; i++) {
      lua_rawgeti(L, 2, i + 1);
      if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        lua_rawgeti(L, -3, 3);
        lua_rawgeti(L, -4, 4);
        luax_readcolor(L, -4, color[i]);
        lua_pop(L, 4);
      } else {
        luax_readcolor(L, -1, color[i]);
      }
      lua_pop(L, 1);
    }
    lua_getfield(L, 2, "depth");
    depth = luax_optfloat(L, -1, depth);
    lua_getfield(L, 2, "stencil");
    stencil = luaL_optinteger(L, -1, stencil);
    lua_pop(L, 2);
  } else {
    for (uint32_t i = 0; i < info->count; i++) {
      if (lua_istable(L, i + 1)) {
        luax_readcolor(L, i + 1, color[i]);
      } else {
        lua_pushvalue(L, i + 1);
        luax_readcolor(L, -1, color[i]);
        lua_pop(L, 1);
      }
    }
  }
  lovrCanvasSetClear(canvas, color, depth, stencil);
  return 0;
}

static int l_lovrCanvasGetColorMask(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  uint32_t target = luaL_optinteger(L, 2, 1) - 1;
  lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
  bool r, g, b, a;
  lovrCanvasGetColorMask(canvas, target, &r, &g, &b, &a);
  lua_pushboolean(L, r);
  lua_pushboolean(L, g);
  lua_pushboolean(L, b);
  lua_pushboolean(L, a);
  return 4;
}

static int l_lovrCanvasSetColorMask(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    uint32_t target = lua_tonumber(L, 2) - 1;
    lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
    bool r = lua_toboolean(L, 3);
    bool g = lua_toboolean(L, 4);
    bool b = lua_toboolean(L, 5);
    bool a = lua_toboolean(L, 6);
    lovrCanvasSetColorMask(canvas, target, r, g, b, a);
    return 0;
  }

  bool r = lua_toboolean(L, 2);
  bool g = lua_toboolean(L, 3);
  bool b = lua_toboolean(L, 4);
  bool a = lua_toboolean(L, 5);
  for (uint32_t i = 0; i < 4; i++) {
    lovrCanvasSetColorMask(canvas, i, r, g, b, a);
  }
  return 0;
}

static int l_lovrCanvasGetCullMode(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  luax_pushenum(L, CullMode, lovrCanvasGetCullMode(canvas));
  return 1;
}

static int l_lovrCanvasSetCullMode(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  CullMode mode = luax_checkenum(L, 2, CullMode, "none");
  lovrCanvasSetCullMode(canvas, mode);
  return 0;
}

static int l_lovrCanvasGetDepthTest(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  CompareMode test;
  bool write;
  lovrCanvasGetDepthTest(canvas, &test, &write);
  if (test == COMPARE_NONE) {
    lua_pushnil(L);
  } else {
    luax_pushenum(L, CompareMode, test);
  }
  lua_pushboolean(L, write);
  return 2;
}

static int l_lovrCanvasSetDepthTest(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  CompareMode test = lua_isnoneornil(L, 2) ? COMPARE_NONE : luax_checkenum(L, 2, CompareMode, NULL);
  bool write = lua_isnoneornil(L, 3) ? true : lua_toboolean(L, 3);
  lovrCanvasSetDepthTest(canvas, test, write);
  return 0;
}

static int l_lovrCanvasGetDepthNudge(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  float nudge, sloped, clamp;
  lovrCanvasGetDepthNudge(canvas, &nudge, &sloped, &clamp);
  lua_pushnumber(L, nudge);
  lua_pushnumber(L, sloped);
  lua_pushnumber(L, clamp);
  return 3;
}

static int l_lovrCanvasSetDepthNudge(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  float nudge = luax_optfloat(L, 2, 0.f);
  float sloped = luax_optfloat(L, 3, 0.f);
  float clamp = luax_optfloat(L, 4, 0.f);
  lovrCanvasSetDepthNudge(canvas, nudge, sloped, clamp);
  return 0;
}

static int l_lovrCanvasGetDepthClamp(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  bool clamp = lovrCanvasGetDepthClamp(canvas);
  lua_pushboolean(L, clamp);
  return 1;
}

static int l_lovrCanvasSetDepthClamp(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  bool clamp = lua_toboolean(L, 2);
  lovrCanvasSetDepthClamp(canvas, clamp);
  return 0;
}

static int l_lovrCanvasGetShader(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Shader* shader = lovrCanvasGetShader(canvas);
  luax_pushtype(L, Shader, shader);
  return 1;
}

static int l_lovrCanvasSetShader(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Shader* shader = lua_isnoneornil(L, 2) ? NULL : luax_checktype(L, 2, Shader);
  lovrCanvasSetShader(canvas, shader);
  return 0;
}

static int l_lovrCanvasGetStencilTest(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  CompareMode test;
  uint8_t value;
  lovrCanvasGetStencilTest(canvas, &test, &value);
  if (test == COMPARE_NONE) {
    lua_pushnil(L);
    return 1;
  }
  luax_pushenum(L, CompareMode, test);
  lua_pushinteger(L, value);
  return 2;
}

static int l_lovrCanvasSetStencilTest(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  if (lua_isnoneornil(L, 2)) {
    lovrCanvasSetStencilTest(canvas, COMPARE_NONE, 0);
  } else {
    CompareMode test = luax_checkenum(L, 2, CompareMode, NULL);
    uint8_t value = luaL_checkinteger(L, 3);
    lovrCanvasSetStencilTest(canvas, test, value);
  }
  return 0;
}

static int l_lovrCanvasGetVertexFormat(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  VertexAttribute attributes[16];
  uint32_t count;
  lovrCanvasGetVertexFormat(canvas, attributes, &count);
  lua_createtable(L, count, 0);
  for (uint32_t i = 0; i < count; i++) {
    lua_newtable(L);
    lua_pushinteger(L, attributes[i].location);
    lua_rawseti(L, -2, 1);
    luax_pushenum(L, FieldType, attributes[i].fieldType);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, attributes[i].buffer + 1);
    lua_setfield(L, -2, "buffer");
    lua_pushinteger(L, attributes[i].offset);
    lua_setfield(L, -2, "offset");
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrCanvasSetVertexFormat(lua_State* L) {
  uint32_t strides[] = {
    [FIELD_I8] = 1,
    [FIELD_U8] = 1,
    [FIELD_I16] = 2,
    [FIELD_U16] = 2,
    [FIELD_I32] = 4,
    [FIELD_U32] = 4,
    [FIELD_F32] = 4,
    [FIELD_I8x2] = 2,
    [FIELD_U8x2] = 2,
    [FIELD_I8Nx2] = 2,
    [FIELD_U8Nx2] = 2,
    [FIELD_I16x2] = 4,
    [FIELD_U16x2] = 4,
    [FIELD_I16Nx2] = 4,
    [FIELD_U16Nx2] = 4,
    [FIELD_I32x2] = 8,
    [FIELD_U32x2] = 8,
    [FIELD_F32x2] = 8,
    [FIELD_I32x3] = 12,
    [FIELD_U32x3] = 12,
    [FIELD_F32x3] = 12,
    [FIELD_I8x4] = 4,
    [FIELD_U8x4] = 4,
    [FIELD_I8Nx4] = 4,
    [FIELD_U8Nx4] = 4,
    [FIELD_I16x4] = 8,
    [FIELD_U16x4] = 8,
    [FIELD_I16Nx4] = 8,
    [FIELD_U16Nx4] = 8,
    [FIELD_I32x4] = 16,
    [FIELD_U32x4] = 16,
    [FIELD_F32x4] = 16,
    [FIELD_MAT2] = 0,
    [FIELD_MAT3] = 0,
    [FIELD_MAT4] = 0
  };

  Canvas* canvas = luax_checktype(L, 1, Canvas);
  VertexAttribute attributes[16];
  uint32_t count = 0;
  if (lua_type(L, 2) == LUA_TSTRING) {
    uint32_t offset = 0;
    int top = lua_gettop(L);
    for (int i = 2; i <= top; i++) {
      FieldType type = luax_checkfieldtype(L, i);
      attributes[count++] = (VertexAttribute) {
        .location = i - 2,
        .buffer = 0,
        .fieldType = type,
        .offset = offset
      };
      offset += strides[type];
    }
  } else if (lua_type(L, 2) == LUA_TUSERDATA) {
    int top = lua_gettop(L);
    for (int i = 2; i <= top; i++) {
      Buffer* buffer = luax_checktype(L, i, Buffer);
      const BufferInfo* info = lovrBufferGetInfo(buffer);
      for (uint32_t j = 0; j < info->fieldCount; j++) {
        attributes[count] = (VertexAttribute) {
          .location = count,
          .buffer = i - 2,
          .fieldType = info->types[j],
          .offset = info->offsets[j]
        };
        count++;
      }
    }
  } else {
    uint32_t offset = 0;
    luaL_checktype(L, 2, LUA_TTABLE);
    int length = luax_len(L, 2);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, 2, i + 1);
      lovrAssert(lua_istable(L, -1), "Vertex format should be a FieldTypes, Buffers, or a table of tables");
      VertexAttribute* attribute = &attributes[count];

      lua_rawgeti(L, -1, 1);
      attribute->location = luaL_optinteger(L, -1, count);
      lua_pop(L, 1);

      lua_rawgeti(L, -1, 2);
      attribute->fieldType = luax_checkfieldtype(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "buffer");
      attribute->buffer = luaL_optinteger(L, -1, 1) - 1;
      lua_pop(L, 1);

      lua_getfield(L, -1, "offset");
      attribute->offset = luaL_optinteger(L, -1, offset);
      lua_pop(L, 1);

      lua_pop(L, 1);
      offset += strides[attribute->fieldType];
      count++;
    }
  }
  lovrCanvasSetVertexFormat(canvas, attributes, count);
  return 0;
}

static int l_lovrCanvasGetWinding(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Winding winding = lovrCanvasGetWinding(canvas);
  luax_pushenum(L, Winding, winding);
  return 1;
}

static int l_lovrCanvasSetWinding(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  Winding winding = luax_checkenum(L, 2, Winding, NULL);
  lovrCanvasSetWinding(canvas, winding);
  return 0;
}

static int l_lovrCanvasIsWireframe(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  bool wireframe = lovrCanvasIsWireframe(canvas);
  lua_pushboolean(L, wireframe);
  return 1;
}

static int l_lovrCanvasSetWireframe(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  bool wireframe = lua_toboolean(L, 2);
  lovrCanvasSetWireframe(canvas, wireframe);
  return 0;
}

static int l_lovrCanvasDraw(lua_State* L) {
  Canvas* canvas = luax_checktype(L, 1, Canvas);
  DrawCall draw;
  bool indexed = false;
  int index = 2;

  // Texture
  if (lua_isuserdata(L, index)) {
    draw.texture = luax_checktype(L, index++, Texture);
  } else {
    draw.texture = NULL;
  }

  // Topology
  if (lua_type(L, index) == LUA_TSTRING) {
    draw.mode = luax_checkenum(L, index++, DrawMode, NULL);
  } else {
    draw.mode = DRAW_TRIANGLES;
  }

  // Vertices
  if (!lua_toboolean(L, index)) {
    draw.vertexBufferCount = 0;
    index++;
  } else if (lua_isuserdata(L, index)) {
    draw.vertexBufferCount = 1;
    draw.vertexBuffers[0] = luax_checktype(L, index++, Buffer);
  } else if (lua_istable(L, index)) {
    // TODO multiple buffers, immediate-mode vertices, oh my
    lovrThrow("TODO");
  } else {
    lovrThrow("Expected nil, false, Buffer or table for vertex data");
  }

  // Indices
  if (lua_isuserdata(L, index)) {
    draw.indexBuffer = luax_checktype(L, index++, Buffer);
    draw.baseVertex = 0;
    indexed = true;
  } else if (lua_istable(L, index)) {
    // TODO read indices
    lovrThrow("TODO");
    draw.baseVertex = 0;
    indexed = true;
  }

  // Transform
  float transform[16];
  index = luax_readmat4(L, index, transform, 1);
  draw.transform = transform;

  // Parameters
  if (lua_isuserdata(L, index)) {
    draw.indirectBuffer = luax_checktype(L, index++, Buffer);
    draw.indirectCount = luaL_optinteger(L, index++, 1);
    draw.indirectOffset = luaL_optinteger(L, index++, 0);
    if (indexed) {
      lovrCanvasDrawIndirectIndexed(canvas, &draw);
    } else {
      lovrCanvasDrawIndirect(canvas, &draw);
    }
  } else {
    draw.start = luaL_optinteger(L, index++, 1) - 1;
    draw.count = luaL_optinteger(L, index++, 0); // TODO min length of all buffers or length of table, minus start
    draw.instances = luaL_optinteger(L, index++, 1);
    if (indexed) {
      lovrCanvasDrawIndexed(canvas, &draw);
    } else {
      lovrCanvasDraw(canvas, &draw);
    }
  }

  return 0;
}

const luaL_Reg lovrCanvas[] = {
  { "begin", l_lovrCanvasBegin },
  { "finish", l_lovrCanvasFinish },
  { "isActive", l_lovrCanvasIsActive },
  { "getViewPose", l_lovrCanvasGetViewPose },
  { "setViewPose", l_lovrCanvasSetViewPose },
  { "getProjection", l_lovrCanvasGetProjection },
  { "setProjection", l_lovrCanvasSetProjection },
  { "push", l_lovrCanvasPush },
  { "pop", l_lovrCanvasPop },
  { "origin", l_lovrCanvasOrigin },
  { "translate", l_lovrCanvasTranslate },
  { "rotate", l_lovrCanvasRotate },
  { "scale", l_lovrCanvasScale },
  { "transform", l_lovrCanvasTransform },
  { "getAlphaToCoverage", l_lovrCanvasGetAlphaToCoverage },
  { "setAlphaToCoverage", l_lovrCanvasSetAlphaToCoverage },
  { "getClear", l_lovrCanvasGetClear },
  { "setClear", l_lovrCanvasSetClear },
  { "getBlendMode", l_lovrCanvasGetBlendMode },
  { "setBlendMode", l_lovrCanvasSetBlendMode },
  { "getColorMask", l_lovrCanvasGetColorMask },
  { "setColorMask", l_lovrCanvasSetColorMask },
  { "getCullMode", l_lovrCanvasGetCullMode },
  { "setCullMode", l_lovrCanvasSetCullMode },
  { "getDepthTest", l_lovrCanvasGetDepthTest },
  { "setDepthTest", l_lovrCanvasSetDepthTest },
  { "getDepthNudge", l_lovrCanvasGetDepthNudge },
  { "setDepthNudge", l_lovrCanvasSetDepthNudge },
  { "getDepthClamp", l_lovrCanvasGetDepthClamp },
  { "setDepthClamp", l_lovrCanvasSetDepthClamp },
  { "getShader", l_lovrCanvasGetShader },
  { "setShader", l_lovrCanvasSetShader },
  { "getStencilTest", l_lovrCanvasGetStencilTest },
  { "setStencilTest", l_lovrCanvasSetStencilTest },
  { "getVertexFormat", l_lovrCanvasGetVertexFormat },
  { "setVertexFormat", l_lovrCanvasSetVertexFormat },
  { "getWinding", l_lovrCanvasGetWinding },
  { "setWinding", l_lovrCanvasSetWinding },
  { "isWireframe", l_lovrCanvasIsWireframe },
  { "setWireframe", l_lovrCanvasSetWireframe },
  { "draw", l_lovrCanvasDraw },
  { NULL, NULL }
};
