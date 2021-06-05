#include "api.h"
#include "graphics/graphics.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrBatchClear(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchClear(batch);
  return 0;
}

static int l_lovrBatchPush(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchPush(batch);
  return 0;
}

static int l_lovrBatchPop(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchPop(batch);
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

static int l_lovrBatchGetAlphaToCoverage(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lua_pushboolean(L, lovrBatchGetAlphaToCoverage(batch));
  return 1;
}

static int l_lovrBatchSetAlphaToCoverage(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  lovrBatchSetAlphaToCoverage(batch, lua_toboolean(L, 2));
  return 1;
}

static int l_lovrBatchGetBlendMode(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t target = luaL_optinteger(L, 2, 1) - 1;
  lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
  BlendMode mode;
  BlendAlphaMode alphaMode;
  lovrBatchGetBlendMode(batch, target, &mode, &alphaMode);
  if (mode == BLEND_NONE) {
    lua_pushnil(L);
    return 1;
  } else {
    luax_pushenum(L, BlendMode, mode);
    luax_pushenum(L, BlendAlphaMode, alphaMode);
    return 2;
  }
}

static int l_lovrBatchSetBlendMode(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    uint32_t target = lua_tonumber(L, 2) - 1;
    lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
    BlendMode mode = lua_isnoneornil(L, 3) ? BLEND_NONE : luax_checkenum(L, 3, BlendMode, NULL);
    BlendAlphaMode alphaMode = luax_checkenum(L, 4, BlendAlphaMode, "alphamultiply");
    lovrBatchSetBlendMode(batch, target, mode, alphaMode);
    return 0;
  }

  BlendMode mode = lua_isnoneornil(L, 2) ? BLEND_NONE : luax_checkenum(L, 2, BlendMode, NULL);
  BlendAlphaMode alphaMode = luax_checkenum(L, 3, BlendAlphaMode, "alphamultiply");
  for (uint32_t i = 0; i < 4; i++) {
    lovrBatchSetBlendMode(batch, i, mode, alphaMode);
  }
  return 0;
}

static int l_lovrBatchGetColorMask(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  uint32_t target = luaL_optinteger(L, 2, 1) - 1;
  lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
  bool r, g, b, a;
  lovrBatchGetColorMask(batch, target, &r, &g, &b, &a);
  lua_pushboolean(L, r);
  lua_pushboolean(L, g);
  lua_pushboolean(L, b);
  lua_pushboolean(L, a);
  return 4;
}

static int l_lovrBatchSetColorMask(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  if (lua_type(L, 2) == LUA_TNUMBER) {
    uint32_t target = lua_tonumber(L, 2) - 1;
    lovrAssert(target < 4, "Invalid color target index: %d", target + 1);
    bool r = lua_toboolean(L, 3);
    bool g = lua_toboolean(L, 4);
    bool b = lua_toboolean(L, 5);
    bool a = lua_toboolean(L, 6);
    lovrBatchSetColorMask(batch, target, r, g, b, a);
    return 0;
  }

  bool r = lua_toboolean(L, 2);
  bool g = lua_toboolean(L, 3);
  bool b = lua_toboolean(L, 4);
  bool a = lua_toboolean(L, 5);
  for (uint32_t i = 0; i < 4; i++) {
    lovrBatchSetColorMask(batch, i, r, g, b, a);
  }
  return 0;
}

static int l_lovrBatchGetCullMode(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  luax_pushenum(L, CullMode, lovrBatchGetCullMode(batch));
  return 1;
}

static int l_lovrBatchSetCullMode(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  CullMode mode = luax_checkenum(L, 2, CullMode, "none");
  lovrBatchSetCullMode(batch, mode);
  return 0;
}

static int l_lovrBatchGetDepthTest(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  CompareMode test;
  bool write;
  lovrBatchGetDepthTest(batch, &test, &write);
  if (test == COMPARE_NONE) {
    lua_pushnil(L);
  } else {
    luax_pushenum(L, CompareMode, test);
  }
  lua_pushboolean(L, write);
  return 2;
}

static int l_lovrBatchSetDepthTest(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  CompareMode test = lua_isnoneornil(L, 2) ? COMPARE_NONE : luax_checkenum(L, 2, CompareMode, NULL);
  bool write = lua_isnoneornil(L, 3) ? true : lua_toboolean(L, 3);
  lovrBatchSetDepthTest(batch, test, write);
  return 0;
}

static int l_lovrBatchGetDepthNudge(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float nudge, sloped, clamp;
  lovrBatchGetDepthNudge(batch, &nudge, &sloped, &clamp);
  lua_pushnumber(L, nudge);
  lua_pushnumber(L, sloped);
  lua_pushnumber(L, clamp);
  return 3;
}

static int l_lovrBatchSetDepthNudge(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  float nudge = luax_optfloat(L, 2, 0.f);
  float sloped = luax_optfloat(L, 3, 0.f);
  float clamp = luax_optfloat(L, 4, 0.f);
  lovrBatchSetDepthNudge(batch, nudge, sloped, clamp);
  return 0;
}

static int l_lovrBatchGetDepthClamp(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool clamp = lovrBatchGetDepthClamp(batch);
  lua_pushboolean(L, clamp);
  return 1;
}

static int l_lovrBatchSetDepthClamp(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool clamp = lua_toboolean(L, 2);
  lovrBatchSetDepthClamp(batch, clamp);
  return 0;
}

static int l_lovrBatchGetShader(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Shader* shader = lovrBatchGetShader(batch);
  luax_pushtype(L, Shader, shader);
  return 1;
}

static int l_lovrBatchSetShader(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Shader* shader = lua_isnoneornil(L, 2) ? NULL : luax_checktype(L, 2, Shader);
  lovrBatchSetShader(batch, shader);
  return 0;
}

static int l_lovrBatchGetStencilTest(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  CompareMode test;
  uint8_t value;
  lovrBatchGetStencilTest(batch, &test, &value);
  if (test == COMPARE_NONE) {
    lua_pushnil(L);
    return 1;
  }
  luax_pushenum(L, CompareMode, test);
  lua_pushinteger(L, value);
  return 2;
}

static int l_lovrBatchSetStencilTest(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  if (lua_isnoneornil(L, 2)) {
    lovrBatchSetStencilTest(batch, COMPARE_NONE, 0);
  } else {
    CompareMode test = luax_checkenum(L, 2, CompareMode, NULL);
    uint8_t value = luaL_checkinteger(L, 3);
    lovrBatchSetStencilTest(batch, test, value);
  }
  return 0;
}

static int l_lovrBatchGetVertexFormat(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  VertexAttribute attributes[16];
  uint32_t count;
  lovrBatchGetVertexFormat(batch, attributes, &count);
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

static int l_lovrBatchSetVertexFormat(lua_State* L) {
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

  Batch* batch = luax_checktype(L, 1, Batch);
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
  lovrBatchSetVertexFormat(batch, attributes, count);
  return 0;
}

static int l_lovrBatchGetWinding(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Winding winding = lovrBatchGetWinding(batch);
  luax_pushenum(L, Winding, winding);
  return 1;
}

static int l_lovrBatchSetWinding(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  Winding winding = luax_checkenum(L, 2, Winding, NULL);
  lovrBatchSetWinding(batch, winding);
  return 0;
}

static int l_lovrBatchIsWireframe(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool wireframe = lovrBatchIsWireframe(batch);
  lua_pushboolean(L, wireframe);
  return 1;
}

static int l_lovrBatchSetWireframe(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  bool wireframe = lua_toboolean(L, 2);
  lovrBatchSetWireframe(batch, wireframe);
  return 0;
}

static int l_lovrBatchDraw(lua_State* L) {
  Batch* batch = luax_checktype(L, 1, Batch);
  DrawInfo draw;
  bool indexed = false;
  int index = 2;

  // Texture
  if (luax_totype(L, index, Texture)) {
    //draw.texture = luax_checktype(L, index++, Texture);
  } else {
    //draw.texture = NULL;
  }

  // Topology
  if (lua_type(L, index) == LUA_TSTRING) {
    draw.mode = luax_checkenum(L, index++, DrawMode, NULL);
  } else {
    draw.mode = DRAW_TRIANGLES;
  }

  // Vertices
  if (!lua_toboolean(L, index)) {
    draw.bufferCount = 0;
    index++;
  } else if (lua_isuserdata(L, index)) {
    draw.bufferCount = 1;
    draw.vertexBuffers[0] = luax_checktype(L, index++, Buffer);
  } else if (lua_istable(L, index)) {
    lua_rawgeti(L, index, 1);
    bool buffers = luax_totype(L, -1, Buffer);
    lua_pop(L, 1);

    if (buffers) {
      int length = luax_len(L, index);
      draw.bufferCount = length;
      for (int i = 0; i < length; i++) {
        lua_rawgeti(L, index, i + 1);
        draw.vertexBuffers[i] = luax_checktype(L, -1, Buffer);
        lua_pop(L, 1);
      }
    } else {
      // TODO immediate-mode vertices, oh my
      lovrThrow("TODO");
    }
    index++;
  } else {
    lovrThrow("Expected nil, false, Buffer or table for vertex data");
  }

  // Indices
  if (luax_totype(L, index, Buffer)) {
    draw.indexBuffer = luax_checktype(L, index++, Buffer);
    indexed = true;
  } else if (lua_istable(L, index)) {
    // TODO read indices
    lovrThrow("TODO");
    indexed = true;
  }

  // Transform
  index = luax_readmat4(L, index, draw.transform, 1);

  // Parameters
  if (lua_isuserdata(L, index)) {
    draw.indirectBuffer = luax_checktype(L, index++, Buffer);
    draw.indirectCount = luaL_optinteger(L, index++, 1);
    draw.indirectOffset = luaL_optinteger(L, index++, 0);
  } else {
    draw.start = luaL_optinteger(L, index++, 1) - 1;
    if (lua_type(L, index) == LUA_TNUMBER) {
      draw.count = lua_tointeger(L, index++);
    } else if (indexed) {
      draw.count = lovrBufferGetInfo(draw.indexBuffer)->length - draw.start;
    } else if (draw.bufferCount > 0) {
      draw.count = ~0u;
      for (uint32_t i = 0; i < draw.bufferCount; i++) {
        const BufferInfo* info = lovrBufferGetInfo(draw.vertexBuffers[i]);
        draw.count = MIN(draw.count, info->length - draw.start);
      }
    } else {
      return 0;
    }
    draw.instances = luaL_optinteger(L, index++, 1);
  }

  lovrBatchDraw(batch, &draw);
  return 0;
}

const luaL_Reg lovrBatch[] = {
  { "clear", l_lovrBatchClear },
  { "push", l_lovrBatchPush },
  { "pop", l_lovrBatchPop },
  { "origin", l_lovrBatchOrigin },
  { "translate", l_lovrBatchTranslate },
  { "rotate", l_lovrBatchRotate },
  { "scale", l_lovrBatchScale },
  { "transform", l_lovrBatchTransform },
  { "getAlphaToCoverage", l_lovrBatchGetAlphaToCoverage },
  { "setAlphaToCoverage", l_lovrBatchSetAlphaToCoverage },
  { "getBlendMode", l_lovrBatchGetBlendMode },
  { "setBlendMode", l_lovrBatchSetBlendMode },
  { "getColorMask", l_lovrBatchGetColorMask },
  { "setColorMask", l_lovrBatchSetColorMask },
  { "getCullMode", l_lovrBatchGetCullMode },
  { "setCullMode", l_lovrBatchSetCullMode },
  { "getDepthTest", l_lovrBatchGetDepthTest },
  { "setDepthTest", l_lovrBatchSetDepthTest },
  { "getDepthNudge", l_lovrBatchGetDepthNudge },
  { "setDepthNudge", l_lovrBatchSetDepthNudge },
  { "getDepthClamp", l_lovrBatchGetDepthClamp },
  { "setDepthClamp", l_lovrBatchSetDepthClamp },
  { "getShader", l_lovrBatchGetShader },
  { "setShader", l_lovrBatchSetShader },
  { "getStencilTest", l_lovrBatchGetStencilTest },
  { "setStencilTest", l_lovrBatchSetStencilTest },
  { "getVertexFormat", l_lovrBatchGetVertexFormat },
  { "setVertexFormat", l_lovrBatchSetVertexFormat },
  { "getWinding", l_lovrBatchGetWinding },
  { "setWinding", l_lovrBatchSetWinding },
  { "isWireframe", l_lovrBatchIsWireframe },
  { "setWireframe", l_lovrBatchSetWireframe },
  { "draw", l_lovrBatchDraw },
  { NULL, NULL }
};
