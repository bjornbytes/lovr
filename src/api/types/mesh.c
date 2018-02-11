#include "api.h"

int l_lovrMeshDrawInstanced(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int instances = luaL_checkinteger(L, 2);
  float transform[16];
  luax_readtransform(L, 3, transform, 1);
  lovrMeshDraw(mesh, transform, NULL, instances);
  return 0;
}

int l_lovrMeshDraw(lua_State* L) {
  lua_pushinteger(L, 1);
  lua_insert(L, 2);
  return l_lovrMeshDrawInstanced(L);
}

int l_lovrMeshGetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  luax_pushenum(L, &MeshDrawModes, lovrMeshGetDrawMode(mesh));
  return 1;
}

int l_lovrMeshSetDrawMode(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  MeshDrawMode* drawMode = (MeshDrawMode*) luax_checkenum(L, 2, &MeshDrawModes, "mesh draw mode");
  lovrMeshSetDrawMode(mesh, *drawMode);
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
  VertexPointer vertex = lovrMeshMap(mesh, index, 1, true, false);
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  return luax_pushvertex(L, &vertex, format);
}

int l_lovrMeshSetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int index = luaL_checkint(L, 2) - 1;
  lovrAssert(index >= 0 && index < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex index: %d", index + 1);
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  VertexPointer vertex = lovrMeshMap(mesh, index, 1, false, true);
  luax_setvertex(L, 3, &vertex, format);
  return 0;
}

int l_lovrMeshGetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  lovrAssert(vertexIndex >= 0 && vertexIndex < lovrMeshGetVertexCount(mesh), "Invalid mesh vertex: %d", vertexIndex + 1);
  lovrAssert(attributeIndex >= 0 && attributeIndex < format->count, "Invalid mesh attribute: %d", attributeIndex + 1);
  Attribute attribute = format->attributes[attributeIndex];
  VertexPointer vertex = lovrMeshMap(mesh, vertexIndex, 1, true, false);
  vertex.bytes += attribute.offset;
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
  VertexPointer vertex = lovrMeshMap(mesh, vertexIndex, 1, false, true);
  vertex.bytes += attribute.offset;
  luax_setvertexattribute(L, 4, &vertex, attribute);
  return 0;
}

int l_lovrMeshSetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  VertexFormat* format = lovrMeshGetVertexFormat(mesh);
  luaL_checktype(L, 2, LUA_TTABLE);
  int vertexCount = lua_objlen(L, 2);
  int start = luaL_optnumber(L, 3, 1) - 1;
  int maxVertices = lovrMeshGetVertexCount(mesh);
  lovrAssert(start + vertexCount <= maxVertices, "Overflow in Mesh:setVertices: Mesh can only hold %d vertices", maxVertices);
  VertexPointer vertices = lovrMeshMap(mesh, start, vertexCount, false, true);

  for (int i = 0; i < vertexCount; i++) {
    lua_rawgeti(L, 2, i + 1);
    luaL_checktype(L, -1, LUA_TTABLE);
    luax_setvertex(L, -1, &vertices, format);
    lua_pop(L, 1);
  }

  return 0;
}

int l_lovrMeshGetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  size_t count;
  IndexPointer indices = lovrMeshGetVertexMap(mesh, &count);

  if (count == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  for (size_t i = 0; i < count; i++) {
    uint32_t index = mesh->indexSize == sizeof(uint32_t) ? indices.ints[i] : indices.shorts[i];
    lua_pushinteger(L, index + 1);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

int l_lovrMeshSetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);

  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetVertexMap(mesh, NULL, 0);
    return 0;
  }

  luaL_checktype(L, 2, LUA_TTABLE);
  int count = lua_objlen(L, 2);
  int vertexCount = lovrMeshGetVertexCount(mesh);
  int indexSize = mesh->indexSize;
  IndexPointer indices = lovrMeshGetVertexMap(mesh, NULL);
  indices.raw = realloc(indices.raw, indexSize * count);

  for (int i = 0; i < count; i++) {
    lua_rawgeti(L, 2, i + 1);
    if (!lua_isnumber(L, -1)) {
      return luaL_error(L, "Mesh vertex map index #%d must be numeric", i);
    }

    int index = lua_tointeger(L, -1);
    if (index > vertexCount || index < 1) {
      return luaL_error(L, "Invalid vertex map value: %d", index);
    }

    if (indexSize == sizeof(uint16_t)) {
      indices.shorts[i] = index - 1;
    } else if (indexSize == sizeof(uint32_t)) {
      indices.ints[i] = index - 1;
    }

    lua_pop(L, 1);
  }

  lovrMeshSetVertexMap(mesh, indices.raw, count);
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
  if (!lovrMeshIsRangeEnabled(mesh)) {
    lua_pushnil(L);
    return 1;
  }

  int start, count;
  lovrMeshGetDrawRange(mesh, &start, &count);
  lua_pushinteger(L, start + 1);
  lua_pushinteger(L, count);
  return 2;
}

int l_lovrMeshSetDrawRange(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  if (lua_isnoneornil(L, 2)) {
    lovrMeshSetRangeEnabled(mesh, 0);
    return 0;
  }

  lovrMeshSetRangeEnabled(mesh, 1);
  int rangeStart = luaL_checkinteger(L, 2) - 1;
  int rangeCount = luaL_checkinteger(L, 3);
  lovrMeshSetDrawRange(mesh, rangeStart, rangeCount);
  return 0;
}

int l_lovrMeshGetMaterial(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  Material* material = lovrMeshGetMaterial(mesh);
  if (material) {
    luax_pushtype(L, Material, material);
  } else {
    lua_pushnil(L);
  }
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
  { "drawInstanced", l_lovrMeshDrawInstanced },
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
