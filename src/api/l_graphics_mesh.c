#include "api.h"
#include "graphics/buffer.h"
#include "graphics/graphics.h"
#include "graphics/material.h"
#include "graphics/mesh.h"
#include "data/blob.h"
#include <lua.h>
#include <lauxlib.h>
#include <limits.h>

static int l_lovrMeshAttachAttributes(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Mesh* other = luax_checktype(L, 2, Mesh);
  int instanceDivisor = luaL_optinteger(L, 3, 0);
  if (lua_isnoneornil(L, 4)) {
    uint32_t count = lovrMeshGetAttributeCount(other);
    for (uint32_t i = 0; i < count; i++) {
      MeshAttribute attachment = *lovrMeshGetAttribute(other, i);
      if (attachment.buffer != lovrMeshGetVertexBuffer(other)) {
        break;
      }
      attachment.divisor = instanceDivisor;
      lovrMeshAttachAttribute(mesh, lovrMeshGetAttributeName(other, i), &attachment);
    }
  } else if (lua_istable(L, 4)) {
    int length = luax_len(L, 4);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, 4, i + 1);
      const char* name = lua_tostring(L, -1);
      uint32_t index = lovrMeshGetAttributeIndex(other, name);
      const MeshAttribute* attribute = lovrMeshGetAttribute(other, index);
      lovrAssert(attribute, "Tried to attach non-existent attribute %s", name);
      MeshAttribute attachment = *attribute;
      attachment.divisor = instanceDivisor;
      lovrMeshAttachAttribute(mesh, name, &attachment);
      lua_pop(L, 1);
    }
  } else {
    int top = lua_gettop(L);
    for (int i = 4; i <= top; i++) {
      const char* name = lua_tostring(L, i);
      uint32_t index = lovrMeshGetAttributeIndex(other, name);
      const MeshAttribute* attribute = lovrMeshGetAttribute(other, index);
      lovrAssert(attribute, "Tried to attach non-existent attribute %s", name);
      MeshAttribute attachment = *attribute;
      attachment.divisor = instanceDivisor;
      lovrMeshAttachAttribute(mesh, name, &attachment);
    }
  }

  return 0;
}

static int l_lovrMeshDetachAttributes(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  if (lua_isuserdata(L, 2)) {
    Mesh* other = luax_checktype(L, 2, Mesh);
    uint32_t count = lovrMeshGetAttributeCount(other);
    for (uint32_t i = 0; i < count; i++) {
      const MeshAttribute* attachment = lovrMeshGetAttribute(other, i);
      if (attachment->buffer != lovrMeshGetVertexBuffer(other)) {
        break;
      }
      lovrMeshDetachAttribute(mesh, lovrMeshGetAttributeName(other, i));
    }
  } else if (lua_istable(L, 2)) {
    int length = luax_len(L, 2);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, 2, i + 1);
      lovrMeshDetachAttribute(mesh, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else {
    int top = lua_gettop(L);
    for (int i = 2; i <= top; i++) {
      lovrMeshDetachAttribute(mesh, lua_tostring(L, i));
    }
  }
  return 0;
}

static int l_lovrMeshDraw(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1);
  int instances = luaL_optinteger(L, index, 1);
  lovrGraphicsDrawMesh(mesh, transform, instances, NULL);
  return 0;
}

static int l_lovrMeshGetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  luax_pushenum(L, DrawMode, lovrMeshGetDrawMode(mesh));
  return 1;
}

static int l_lovrMeshSetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  DrawMode mode = luax_checkenum(L, 2, DrawMode, NULL);
  lovrMeshSetDrawMode(mesh, mode);
  return 0;
}

static int l_lovrMeshGetVertexFormat(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t attributeCount = lovrMeshGetAttributeCount(mesh);
  lua_createtable(L, attributeCount, 0);
  for (uint32_t i = 0; i < attributeCount; i++) {
    const MeshAttribute* attribute = lovrMeshGetAttribute(mesh, i);
    if (attribute->buffer != lovrMeshGetVertexBuffer(mesh)) {
      break;
    }
    lua_createtable(L, 3, 0);
    lua_pushstring(L, lovrMeshGetAttributeName(mesh, i));
    lua_rawseti(L, -2, 1);
    luax_pushenum(L, AttributeType, attribute->type);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, attribute->components);
    lua_rawseti(L, -2, 3);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static int l_lovrMeshGetVertexCount(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  lua_pushinteger(L, lovrMeshGetVertexCount(mesh));
  return 1;
}

static int l_lovrMeshGetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int index = luaL_checkinteger(L, 2) - 1;
  Buffer* buffer = lovrMeshGetVertexBuffer(mesh);
  uint32_t attributeCount = lovrMeshGetAttributeCount(mesh);
  const MeshAttribute* firstAttribute = lovrMeshGetAttribute(mesh, 0);

  if (!buffer || attributeCount == 0 || firstAttribute->buffer != buffer) {
    lovrThrow("Mesh does not have a vertex buffer");
  }

  lovrAssert(lovrBufferIsReadable(buffer), "Mesh:getVertex can only be used if the Mesh was created with the readable flag");
  AttributeData data = { .raw = lovrBufferMap(buffer, index * firstAttribute->stride, false) };

  int components = 0;
  for (uint32_t i = 0; i < attributeCount; i++) {
    const MeshAttribute* attribute = lovrMeshGetAttribute(mesh, i);
    if (attribute->buffer != buffer) {
      break;
    }
    for (unsigned j = 0; j < attribute->components; j++, components++) {
      switch (attribute->type) {
        case I8: lua_pushinteger(L, *data.i8++); break;
        case U8: lua_pushinteger(L, *data.u8++); break;
        case I16: lua_pushinteger(L, *data.i16++); break;
        case U16: lua_pushinteger(L, *data.u16++); break;
        case I32: lua_pushinteger(L, *data.i32++); break;
        case U32: lua_pushinteger(L, *data.u32++); break;
        case F32: lua_pushnumber(L, *data.f32++); break;
      }
    }
  }
  return components;
}

static int l_lovrMeshSetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = lovrMeshGetVertexBuffer(mesh);
  uint32_t index = luaL_checkinteger(L, 2) - 1;
  lovrAssert(index < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex index: %d", index + 1);
  bool table = lua_istable(L, 3);
  uint32_t attributeCount = lovrMeshGetAttributeCount(mesh);
  const MeshAttribute* firstAttribute = lovrMeshGetAttribute(mesh, 0);

  if (!buffer || attributeCount == 0 || firstAttribute->buffer != buffer) {
    lovrThrow("Mesh does not have a vertex buffer");
  }

  size_t stride = firstAttribute->stride;
  AttributeData data = { .raw = lovrBufferMap(buffer, index * stride, false) };
  int component = 0;
  for (uint32_t i = 0; i < attributeCount; i++) {
    const MeshAttribute* attribute = lovrMeshGetAttribute(mesh, i);
    if (attribute->buffer != buffer) {
      break;
    }

    for (unsigned j = 0; j < attribute->components; j++) {
      int k = 3 + j;
      if (table) {
        lua_rawgeti(L, 3, ++component);
        k = -1;
      }

      switch (attribute->type) {
        case I8: *data.i8++ = luaL_optinteger(L, k, 0); break;
        case U8: *data.u8++ = luaL_optinteger(L, k, 0); break;
        case I16: *data.i16++ = luaL_optinteger(L, k, 0); break;
        case U16: *data.u16++ = luaL_optinteger(L, k, 0); break;
        case I32: *data.i32++ = luaL_optinteger(L, k, 0); break;
        case U32: *data.u32++ = luaL_optinteger(L, k, 0); break;
        case F32: *data.f32++ = luaL_optnumber(L, k, 0.); break;
      }

      if (table) {
        lua_pop(L, 1);
      }
    }
  }
  lovrBufferFlush(buffer, index * stride, stride);
  return 0;
}

static int l_lovrMeshGetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t vertexIndex = luaL_checkinteger(L, 2) - 1;
  uint32_t attributeIndex = luaL_checkinteger(L, 3) - 1;
  Buffer* buffer = lovrMeshGetVertexBuffer(mesh);
  lovrAssert(lovrBufferIsReadable(buffer), "Mesh:getVertex can only be used if the Mesh was created with the readable flag");
  lovrAssert(vertexIndex < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex: %d", vertexIndex + 1);
  const MeshAttribute* attribute = lovrMeshGetAttribute(mesh, attributeIndex);
  lovrAssert(attribute && attribute->buffer == buffer, "Invalid mesh attribute: %d", attributeIndex + 1);
  AttributeData data = { .raw = lovrBufferMap(buffer, vertexIndex * attribute->stride + attribute->offset, false) };
  for (unsigned i = 0; i < attribute->components; i++) {
    switch (attribute->type) {
      case I8: lua_pushinteger(L, *data.i8++); break;
      case U8: lua_pushinteger(L, *data.u8++); break;
      case I16: lua_pushinteger(L, *data.i16++); break;
      case U16: lua_pushinteger(L, *data.u16++); break;
      case I32: lua_pushinteger(L, *data.i32++); break;
      case U32: lua_pushinteger(L, *data.u32++); break;
      case F32: lua_pushnumber(L, *data.f32++); break;
    }
  }
  return attribute->components;
}

static int l_lovrMeshSetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = lovrMeshGetVertexBuffer(mesh);
  uint32_t vertexIndex = luaL_checkinteger(L, 2) - 1;
  uint32_t attributeIndex = luaL_checkinteger(L, 3) - 1;
  bool table = lua_istable(L, 4);
  lovrAssert(vertexIndex < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex: %d", vertexIndex + 1);
  const MeshAttribute* attribute = lovrMeshGetAttribute(mesh, attributeIndex);
  lovrAssert(attribute && attribute->buffer == buffer, "Invalid mesh attribute: %d", attributeIndex + 1);
  AttributeData data = { .raw = lovrBufferMap(buffer, vertexIndex * attribute->stride + attribute->offset, false) };
  for (unsigned i = 0; i < attribute->components; i++) {
    int index = 4 + i;
    if (table) {
      lua_rawgeti(L, 4, i + 1);
      index = -1;
    }

    switch (attribute->type) {
      case I8: *data.i8++ = luaL_optinteger(L, index, 0); break;
      case U8: *data.u8++ = luaL_optinteger(L, index, 0); break;
      case I16: *data.i16++ = luaL_optinteger(L, index, 0); break;
      case U16: *data.u16++ = luaL_optinteger(L, index, 0); break;
      case I32: *data.i32++ = luaL_optinteger(L, index, 0); break;
      case U32: *data.u32++ = luaL_optinteger(L, index, 0); break;
      case F32: *data.f32++ = luaL_optnumber(L, index, 0.); break;
    }

    if (table) {
      lua_pop(L, 1);
    }
  }
  size_t attributeSize = 0;
  switch (attribute->type) {
    case I8: attributeSize = attribute->components * sizeof(int8_t); break;
    case U8: attributeSize = attribute->components * sizeof(uint8_t); break;
    case I16: attributeSize = attribute->components * sizeof(int16_t); break;
    case U16: attributeSize = attribute->components * sizeof(uint16_t); break;
    case I32: attributeSize = attribute->components * sizeof(int32_t); break;
    case U32: attributeSize = attribute->components * sizeof(uint32_t); break;
    case F32: attributeSize = attribute->components * sizeof(float); break;
  }
  lovrBufferFlush(buffer, vertexIndex * attribute->stride + attribute->offset, attributeSize);
  return 0;
}

static int l_lovrMeshSetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = lovrMeshGetVertexBuffer(mesh);
  uint32_t attributeCount = lovrMeshGetAttributeCount(mesh);
  const MeshAttribute* firstAttribute = lovrMeshGetAttribute(mesh, 0);

  if (!buffer || attributeCount == 0 || firstAttribute->buffer != buffer) {
    lovrThrow("Mesh:setVertices does not work when the Mesh does not have a vertex buffer");
  }

  uint32_t capacity = lovrMeshGetVertexCount(mesh);
  uint32_t start = luaL_optinteger(L, 3, 1) - 1;
  uint32_t count = luaL_optinteger(L, 4, capacity - start);
  size_t stride = firstAttribute->stride;

  Blob* blob = luax_totype(L, 2, Blob);
  if (blob) {
    count = MIN(count, (uint32_t) (blob->size / stride));
    lovrAssert(start + count <= capacity, "Overflow in Mesh:setVertices: Mesh can only hold %d vertices", capacity);
    void* data = lovrBufferMap(buffer, start * stride, false);
    memcpy(data, blob->data, count * stride);
    lovrBufferFlush(buffer, start * stride, count * stride);
    return 0;
  }

  luaL_checktype(L, 2, LUA_TTABLE);
  count = MIN(count, (uint32_t) luax_len(L, 2));
  lovrAssert(start + count <= capacity, "Overflow in Mesh:setVertices: Mesh can only hold %d vertices", capacity);

  AttributeData data = { .raw = lovrBufferMap(buffer, start * stride, false) };

  for (uint32_t i = 0; i < count; i++) {
    lua_rawgeti(L, 2, i + 1);
    luaL_checktype(L, -1, LUA_TTABLE);
    int component = 0;
    for (uint32_t j = 0; j < attributeCount; j++) {
      const MeshAttribute* attribute = lovrMeshGetAttribute(mesh, j);
      if (attribute->buffer != buffer) {
        break;
      }

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

  lovrBufferFlush(buffer, start * stride, count * stride);
  return 0;
}

static int l_lovrMeshGetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* buffer = lovrMeshGetIndexBuffer(mesh);
  uint32_t count = lovrMeshGetIndexCount(mesh);
  size_t size = lovrMeshGetIndexSize(mesh);

  if (!buffer || count == 0 || size == 0) {
    lua_pushnil(L);
    return 1;
  }

  lovrAssert(lovrBufferIsReadable(buffer), "Mesh:getVertexMap can only be used if the Mesh was created with the readable flag");
  union { void* raw; uint16_t* shorts; uint32_t* ints; } indices = { .raw = lovrBufferMap(buffer, 0, false) };

  if (lua_istable(L, 2)) {
    lua_settop(L, 2);
  } else if (lua_isuserdata(L, 2)) {
    Blob* blob = luax_checktype(L, 2, Blob);
    lovrAssert(size * count <= blob->size, "Mesh vertex map is %zu bytes, but Blob can only hold %zu", size * count, blob->size);
    memcpy(blob->data, indices.raw, size * count);
    return 0;
  } else {
    lua_settop(L, 1);
    lua_createtable(L, count, 0);
  }

  for (uint32_t i = 0; i < count; i++) {
    uint32_t index = size == sizeof(uint32_t) ? indices.ints[i] : indices.shorts[i];
    lua_pushinteger(L, index + 1);
    lua_rawseti(L, 2, i + 1);
  }

  return 1;
}

static int l_lovrMeshSetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Buffer* release = NULL;

  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetIndexBuffer(mesh, NULL, 0, 0, 0);
    return 0;
  }

  if (lua_type(L, 2) == LUA_TUSERDATA) {
    Blob* blob = luax_checktype(L, 2, Blob);
    size_t size = luaL_optinteger(L, 3, 4);
    lovrAssert(size == 2 || size == 4, "Size of Mesh indices should be 2 bytes or 4 bytes");
    lovrAssert(blob->size / size < UINT32_MAX, "Too many Mesh indices");
    uint32_t count = (uint32_t) (blob->size / size);
    Buffer* indexBuffer = lovrMeshGetIndexBuffer(mesh);
    if (!indexBuffer || count * size > lovrBufferGetSize(indexBuffer)) {
      Buffer* vertexBuffer = lovrMeshGetVertexBuffer(mesh);
      BufferUsage usage = vertexBuffer ? lovrBufferGetUsage(vertexBuffer) : USAGE_DYNAMIC;
      bool readable = vertexBuffer ? lovrBufferIsReadable(vertexBuffer) : false;
      indexBuffer = release = lovrBufferCreate(blob->size, blob->data, BUFFER_INDEX, usage, readable);
      lovrMeshSetIndexBuffer(mesh, indexBuffer, count, size, 0);
    } else {
      void* indices = lovrBufferMap(indexBuffer, 0, false);
      memcpy(indices, blob->data, blob->size);
      lovrBufferFlush(indexBuffer, 0, blob->size);
    }
  } else {
    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t count = luax_len(L, 2);
    uint32_t vertexCount = lovrMeshGetVertexCount(mesh);
    size_t size = vertexCount > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);

    Buffer* indexBuffer = lovrMeshGetIndexBuffer(mesh);
    if (!indexBuffer || count * size > lovrBufferGetSize(indexBuffer)) {
      Buffer* vertexBuffer = lovrMeshGetVertexBuffer(mesh);
      BufferUsage usage = vertexBuffer ? lovrBufferGetUsage(vertexBuffer) : USAGE_DYNAMIC;
      bool readable = vertexBuffer ? lovrBufferIsReadable(vertexBuffer) : false;
      indexBuffer = release = lovrBufferCreate(count * size, NULL, BUFFER_INDEX, usage, readable);
    }

    union { void* raw; uint16_t* shorts; uint32_t* ints; } indices = { .raw = lovrBufferMap(indexBuffer, 0, false) };

    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, 2, i + 1);
      if (!lua_isnumber(L, -1)) {
        return luaL_error(L, "Mesh vertex map index #%d must be numeric", i);
      }

      uint32_t index = lua_tointeger(L, -1);
      if (index > vertexCount || index < 1) {
        return luaL_error(L, "Invalid vertex map value: %d", index);
      }

      if (size == sizeof(uint16_t)) {
        indices.shorts[i] = index - 1;
      } else {
        indices.ints[i] = index - 1;
      }

      lua_pop(L, 1);
    }

    lovrMeshSetIndexBuffer(mesh, indexBuffer, count, size, 0);
    lovrBufferFlush(indexBuffer, 0, count * size);
  }

  lovrRelease(release, lovrBufferDestroy);
  return 0;
}

static int l_lovrMeshIsAttributeEnabled(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const char* attribute = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrMeshIsAttributeEnabled(mesh, attribute));
  return 1;
}

static int l_lovrMeshSetAttributeEnabled(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const char* attribute = luaL_checkstring(L, 2);
  bool enabled = lua_toboolean(L, 3);
  lovrMeshSetAttributeEnabled(mesh, attribute, enabled);
  return 0;
}

static int l_lovrMeshGetDrawRange(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t start, count;
  lovrMeshGetDrawRange(mesh, &start, &count);

  if (count == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_pushinteger(L, start + 1);
  lua_pushinteger(L, count);
  return 2;
}

static int l_lovrMeshSetDrawRange(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetDrawRange(mesh, 0, 0);
    return 0;
  }

  int rangeStart = luaL_checkinteger(L, 2) - 1;
  int rangeCount = luaL_checkinteger(L, 3);
  lovrMeshSetDrawRange(mesh, rangeStart, rangeCount);
  return 0;
}

static int l_lovrMeshGetMaterial(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Material* material = lovrMeshGetMaterial(mesh);
  luax_pushtype(L, Material, material);
  return 1;
}

static int l_lovrMeshSetMaterial(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetMaterial(mesh, NULL);
  } else {
    Material* material = luax_checktype(L, 2, Material);
    lovrMeshSetMaterial(mesh, material);
  }
  return 0;
}

const luaL_Reg lovrMesh[] = {
  { "attachAttributes", l_lovrMeshAttachAttributes },
  { "detachAttributes", l_lovrMeshDetachAttributes },
  { "draw", l_lovrMeshDraw },
  { "getVertexFormat", l_lovrMeshGetVertexFormat },
  { "getVertexCount", l_lovrMeshGetVertexCount },
  { "getVertex", l_lovrMeshGetVertex },
  { "setVertex", l_lovrMeshSetVertex },
  { "getVertexAttribute", l_lovrMeshGetVertexAttribute },
  { "setVertexAttribute", l_lovrMeshSetVertexAttribute },
  { "setVertices", l_lovrMeshSetVertices },
  { "getVertexMap", l_lovrMeshGetVertexMap },
  { "setVertexMap", l_lovrMeshSetVertexMap },
  { "isAttributeEnabled", l_lovrMeshIsAttributeEnabled },
  { "setAttributeEnabled", l_lovrMeshSetAttributeEnabled },
  { "getDrawMode", l_lovrMeshGetDrawMode },
  { "setDrawMode", l_lovrMeshSetDrawMode },
  { "getDrawRange", l_lovrMeshGetDrawRange },
  { "setDrawRange", l_lovrMeshSetDrawRange },
  { "getMaterial", l_lovrMeshGetMaterial },
  { "setMaterial", l_lovrMeshSetMaterial },
  { NULL, NULL }
};
