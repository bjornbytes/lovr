#include "api.h"
#include "api/data.h"
#include "api/math.h"
#include "graphics/graphics.h"
#include <limits.h>

int l_lovrMeshAttachAttributes(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Mesh* other = luax_checktype(L, 2, Mesh);
  int instanceDivisor = luaL_optinteger(L, 3, 0);
  if (lua_isnoneornil(L, 4)) {
    VertexFormat* format = lovrMeshGetVertexFormat(other);
    for (int i = 0; i < format->count; i++) {
      const char* name = format->attributes[i].name;
      MeshAttribute* attribute = lovrMeshGetAttribute(other, name);
      MeshAttribute attachment = *attribute;
      attachment.divisor = instanceDivisor;
      attachment.enabled = true;
      lovrMeshAttachAttribute(mesh, name, &attachment);
    }
  } else if (lua_istable(L, 4)) {
    int length = lua_objlen(L, 4);
    for (int i = 0; i < length; i++) {
      lua_rawgeti(L, 4, i + 1);
      const char* name = lua_tostring(L, -1);
      MeshAttribute* attribute = lovrMeshGetAttribute(other, name);
      lovrAssert(attribute, "Tried to attach non-existent attribute %s", name);
      MeshAttribute attachment = *attribute;
      attachment.divisor = instanceDivisor;
      attachment.enabled = true;
      lovrMeshAttachAttribute(mesh, name, &attachment);
      lua_pop(L, 1);
    }
  } else {
    int top = lua_gettop(L);
    for (int i = 4; i <= top; i++) {
      const char* name = lua_tostring(L, i);
      MeshAttribute* attribute = lovrMeshGetAttribute(other, name);
      lovrAssert(attribute, "Tried to attach non-existent attribute %s", name);
      MeshAttribute attachment = *attribute;
      attachment.divisor = instanceDivisor;
      attachment.enabled = true;
      lovrMeshAttachAttribute(mesh, name, &attachment);
    }
  }

  return 0;
}

int l_lovrMeshDetachAttributes(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  if (lua_isuserdata(L, 2)) {
    Mesh* other = luax_checktype(L, 2, Mesh);
    VertexFormat* format = lovrMeshGetVertexFormat(other);
    for (int i = 0; i < format->count; i++) {
      lovrMeshDetachAttribute(mesh, format->attributes[i].name);
    }
  } else if (lua_istable(L, 2)) {
    int length = lua_objlen(L, 2);
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

int l_lovrMeshDraw(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  float transform[16];
  int index = luax_readmat4(L, 2, transform, 1, NULL);
  int instances = luaL_optinteger(L, index, 1);
  lovrGraphicsDraw(&(DrawRequest) {
    .transform = transform,
    .mesh = mesh,
    .material = lovrMeshGetMaterial(mesh),
    .instances = instances
  });
  return 0;
}

int l_lovrMeshGetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  lua_pushstring(L, DrawModes[lovrMeshGetDrawMode(mesh)]);
  return 1;
}

int l_lovrMeshSetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  DrawMode mode = luaL_checkoption(L, 2, NULL, DrawModes);
  lovrMeshSetDrawMode(mesh, mode);
  return 0;
}

int l_lovrMeshGetVertexFormat(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  return luax_pushvertexformat(L, format);
}

int l_lovrMeshGetVertexCount(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  lua_pushinteger(L, lovrMeshGetVertexCount(mesh));
  return 1;
}

int l_lovrMeshGetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(lovrMeshIsReadable(mesh), "Mesh:getVertex can only be used if the Mesh was created with the readable flag");
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  VertexPointer vertex = { .raw = lovrMeshMapVertices(mesh, index * format->stride) };
  return luax_pushvertex(L, &vertex, format);
}

int l_lovrMeshSetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex index: %d", index + 1);
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  VertexPointer vertex = { .raw = lovrMeshMapVertices(mesh, index * format->stride) };
  luax_setvertex(L, 3, &vertex, format);
  lovrMeshFlushVertices(mesh, index * format->stride, format->stride);
  return 0;
}

int l_lovrMeshGetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  lovrAssert(lovrMeshIsReadable(mesh), "Mesh:getVertex can only be used if the Mesh was created with the readable flag");
  lovrAssert(vertexIndex >= 0 && vertexIndex < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex: %d", vertexIndex + 1);
  lovrAssert(attributeIndex >= 0 && attributeIndex < format->count, "Invalid mesh attribute: %d", attributeIndex + 1);
  Attribute attribute = format->attributes[attributeIndex];
  VertexPointer vertex = { .raw = lovrMeshMapVertices(mesh, vertexIndex * format->stride + attribute.offset) };
  return luax_pushvertexattribute(L, &vertex, attribute);
}

int l_lovrMeshSetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  lovrAssert(vertexIndex >= 0 && vertexIndex < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex: %d", vertexIndex + 1);
  lovrAssert(attributeIndex >= 0 && attributeIndex < format->count, "Invalid mesh attribute: %d", attributeIndex + 1);
  Attribute attribute = format->attributes[attributeIndex];
  VertexPointer vertex = { .raw = lovrMeshMapVertices(mesh, vertexIndex * format->stride + attribute.offset) };
  luax_setvertexattribute(L, 4, &vertex, attribute);
  lovrMeshFlushVertices(mesh, vertexIndex * format->stride + attribute.offset, attribute.size);
  return 0;
}

int l_lovrMeshSetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  uint32_t capacity = lovrMeshGetVertexCount(mesh);

  VertexData* vertexData = NULL;
  uint32_t sourceSize;
  if (lua_istable(L, 2)) {
    sourceSize = lua_objlen(L, 2);
  } else {
    vertexData = luax_checktype(L, 2, VertexData);
    sourceSize = vertexData->count;
    bool sameFormat = !memcmp(&vertexData->format, format, sizeof(VertexFormat));
    lovrAssert(sameFormat, "Mesh and VertexData must have the same format to copy vertices");
  }

  uint32_t start = luaL_optnumber(L, 3, 1) - 1;
  uint32_t count = luaL_optinteger(L, 4, sourceSize);
  lovrAssert(start + count <= capacity, "Overflow in Mesh:setVertices: Mesh can only hold %d vertices", capacity);
  lovrAssert(count <= sourceSize, "Cannot set %d vertices on Mesh: source only has %d vertices", count, sourceSize);

  VertexPointer vertices = { .raw = lovrMeshMapVertices(mesh, start * format->stride) };

  if (vertexData) {
    memcpy(vertices.raw, vertexData->blob.data, count * format->stride);
  } else {
    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, 2, i + 1);
      luaL_checktype(L, -1, LUA_TTABLE);
      luax_setvertex(L, -1, &vertices, format);
      lua_pop(L, 1);
    }
  }

  lovrMeshFlushVertices(mesh, start * format->stride, count * format->stride);

  return 0;
}

int l_lovrMeshGetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  uint32_t count;
  size_t size;
  IndexPointer indices = { .raw = lovrMeshReadIndices(mesh, &count, &size) };

  if (count == 0 || !indices.raw) {
    lua_pushnil(L);
    return 1;
  }

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

  for (size_t i = 0; i < count; i++) {
    uint32_t index = size == sizeof(uint32_t) ? indices.ints[i] : indices.shorts[i];
    lua_pushinteger(L, index + 1);
    lua_rawseti(L, 2, i + 1);
  }

  return 1;
}

int l_lovrMeshSetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  if (lua_isnoneornil(L, 2)) {
    lovrMeshMapIndices(mesh, 0, 0, 0);
    return 0;
  }

  if (lua_type(L, 2) == LUA_TUSERDATA) {
    Blob* blob = luax_checktype(L, 2, Blob);
    size_t size = luaL_optinteger(L, 3, 4);
    lovrAssert(size == 2 || size == 4, "Size of Mesh indices should be 2 bytes or 4 bytes");
    uint32_t count = blob->size / size;
    void* indices = lovrMeshMapIndices(mesh, count, size, 0);
    memcpy(indices, blob->data, blob->size);
  } else {
    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t count = lua_objlen(L, 2);
    uint32_t vertexCount = lovrMeshGetVertexCount(mesh);
    size_t size = vertexCount > USHRT_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
    IndexPointer indices = { .raw = lovrMeshMapIndices(mesh, count, size, 0) };

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
  }

  lovrMeshFlushIndices(mesh);

  return 0;
}

int l_lovrMeshIsAttributeEnabled(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const char* attribute = luaL_checkstring(L, 2);
  lua_pushboolean(L, lovrMeshIsAttributeEnabled(mesh, attribute));
  return 1;
}

int l_lovrMeshSetAttributeEnabled(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  const char* attribute = luaL_checkstring(L, 2);
  bool enabled = lua_toboolean(L, 3);
  lovrMeshSetAttributeEnabled(mesh, attribute, enabled);
  return 0;
}

int l_lovrMeshGetDrawRange(lua_State* L) {
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

int l_lovrMeshSetDrawRange(lua_State* L) {
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

int l_lovrMeshGetMaterial(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Material* material = lovrMeshGetMaterial(mesh);
  luax_pushobject(L, material);
  return 1;
}

int l_lovrMeshSetMaterial(lua_State* L) {
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
