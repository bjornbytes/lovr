#include "api/lovr.h"

void luax_checkmeshformat(lua_State* L, int index, MeshFormat* format) {
  if (!lua_istable(L, index)) {
    return;
  }

  int length = lua_objlen(L, index);
  for (int i = 0; i < length; i++) {
    lua_rawgeti(L, index, i + 1);

    if (!lua_istable(L, -1) || lua_objlen(L, -1) != 3) {
      luaL_error(L, "Expected vertex format specified as tables containing name, data type, and size");
      return;
    }

    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -2, 2);
    lua_rawgeti(L, -3, 3);

    const char* name = lua_tostring(L, -3);
    MeshAttributeType* type = (MeshAttributeType*) luax_checkenum(L, -2, &MeshAttributeTypes, "mesh attribute type");
    int count = lua_tointeger(L, -1);
    MeshAttribute attribute = { .name = name, .type = *type, .count = count };
    vec_push(format, attribute);
    lua_pop(L, 4);
  }
}

int l_lovrMeshDraw(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  float transform[16];
  luax_readtransform(L, 2, transform, 1);
  lovrMeshDraw(mesh, transform);
  return 0;
}

int l_lovrMeshGetVertexFormat(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  MeshFormat format = lovrMeshGetVertexFormat(mesh);
  lua_newtable(L);
  for (int i = 0; i < format.length; i++) {
    MeshAttribute attribute = format.data[i];
    lua_newtable(L);

    // Name
    lua_pushstring(L, attribute.name);
    lua_rawseti(L, -2, 1);

    // Type
    luax_pushenum(L, &MeshAttributeTypes, attribute.type);
    lua_rawseti(L, -2, 2);

    // Count
    lua_pushinteger(L, attribute.count);
    lua_rawseti(L, -2, 3);

    lua_rawseti(L, -2, i + 1);
  }
  return 1;
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

int l_lovrMeshGetVertexCount(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  lua_pushnumber(L, lovrMeshGetVertexCount(mesh));
  return 1;
}

int l_lovrMeshGetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int index = luaL_checkint(L, 2) - 1;
  char* vertex = lovrMeshMap(mesh, index, 1, 1, 0);
  MeshFormat format = lovrMeshGetVertexFormat(mesh);

  int total = 0;
  for (int i = 0; i < format.length; i++) {
    MeshAttribute attribute = format.data[i];
    total += attribute.count;
    for (int j = 0; j < attribute.count; j++) {
      switch (attribute.type) {
        case MESH_FLOAT: lua_pushnumber(L, *((float*) vertex)); break;
        case MESH_BYTE: lua_pushnumber(L, *((unsigned char*) vertex)); break;
        case MESH_INT: lua_pushnumber(L, *((int*) vertex)); break;
      }

      vertex += sizeof(attribute.type);
    }
  }

  return total;
}

int l_lovrMeshSetVertex(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int index = luaL_checkint(L, 2) - 1;

  if (index < 0 || index >= lovrMeshGetVertexCount(mesh)) {
    return luaL_error(L, "Invalid mesh vertex index: %d", index + 1);
  }

  MeshFormat format = lovrMeshGetVertexFormat(mesh);
  char* vertex = lovrMeshMap(mesh, index, 1, 0, 1);

  // Unwrap table
  int arg = 3;
  if (lua_istable(L, 3)) {
    arg++;
    for (size_t i = 0; i < lua_objlen(L, 3); i++) {
      lua_rawgeti(L, 3, i + 1);
    }
  }

  for (int i = 0; i < format.length; i++) {
    MeshAttribute attribute = format.data[i];
    for (int j = 0; j < attribute.count; j++) {
      switch (attribute.type) {
        case MESH_FLOAT: *((float*) vertex) = luaL_optnumber(L, arg++, 0.f); break;
        case MESH_BYTE: *((unsigned char*) vertex) = luaL_optint(L, arg++, 255); break;
        case MESH_INT: *((int*) vertex) = luaL_optint(L, arg++, 0); break;
      }

      vertex += sizeof(attribute.type);
    }
  }

  return 0;
}

int l_lovrMeshGetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  MeshFormat format = lovrMeshGetVertexFormat(mesh);

  if (vertexIndex < 0 || vertexIndex >= lovrMeshGetVertexCount(mesh)) {
    return luaL_error(L, "Invalid mesh vertex index: %d", vertexIndex + 1);
  } else if (attributeIndex < 0 || attributeIndex >= format.length) {
    return luaL_error(L, "Invalid mesh attribute index: %d", attributeIndex + 1);
  }

  char* vertex = lovrMeshMap(mesh, vertexIndex, 1, 1, 0);

  MeshAttribute attribute;
  for (int i = 0; i <= attributeIndex; i++) {
    attribute = format.data[i];
    if (i == attributeIndex) {
      for (int j = 0; j < attribute.count; j++) {
        switch (attribute.type) {
          case MESH_FLOAT: lua_pushnumber(L, *((float*) vertex)); break;
          case MESH_BYTE: lua_pushinteger(L, *((unsigned char*) vertex)); break;
          case MESH_INT: lua_pushinteger(L, *((int*) vertex)); break;
        }
        vertex += sizeof(attribute.type);
      }
    } else {
      vertex += attribute.count * sizeof(attribute.type);
    }
  }

  return attribute.count;
}

int l_lovrMeshSetVertexAttribute(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  MeshFormat format = lovrMeshGetVertexFormat(mesh);

  if (vertexIndex < 0 || vertexIndex >= lovrMeshGetVertexCount(mesh)) {
    return luaL_error(L, "Invalid mesh vertex index: %d", vertexIndex + 1);
  } else if (attributeIndex < 0 || attributeIndex >= format.length) {
    return luaL_error(L, "Invalid mesh attribute index: %d", attributeIndex + 1);
  }

  char* vertex = lovrMeshMap(mesh, vertexIndex, 1, 0, 1);

  int arg = 4;
  for (int i = 0; i <= attributeIndex; i++) {
    MeshAttribute attribute = format.data[i];
    if (i == attributeIndex) {
      for (int j = 0; j < attribute.count; j++) {
        switch (attribute.type) {
          case MESH_FLOAT: *((float*) vertex) = luaL_optnumber(L, arg++, 0.f); break;
          case MESH_BYTE: *((unsigned char*) vertex) = luaL_optint(L, arg++, 255); break;
          case MESH_INT: *((int*) vertex) = luaL_optint(L, arg++, 0); break;
        }
        vertex += sizeof(attribute.type);
      }
    } else {
      vertex += attribute.count * sizeof(attribute.type);
    }
  }

  return 0;
}

int l_lovrMeshSetVertices(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  MeshFormat format = lovrMeshGetVertexFormat(mesh);
  luaL_checktype(L, 2, LUA_TTABLE);
  int vertexCount = lua_objlen(L, 2);
  int start = luaL_optnumber(L, 3, 1) - 1;
  int maxVertices = lovrMeshGetVertexCount(mesh);

  if (start + vertexCount > maxVertices) {
    return luaL_error(L, "Mesh can only hold %d vertices", maxVertices);
  }

  void* vertices = lovrMeshMap(mesh, start, vertexCount, 0, 1);
  char* vertex = vertices;

  for (int i = 0; i < vertexCount; i++) {
    lua_rawgeti(L, 2, i + 1);
    int component = 0;
    for (int j = 0; j < format.length; j++) {
      MeshAttribute attribute = format.data[j];
      for (int k = 0; k < attribute.count; k++) {
        lua_rawgeti(L, -1, ++component);
        switch (attribute.type) {
          case MESH_FLOAT: *((float*) vertex) = luaL_optnumber(L, -1, 0.f); break;
          case MESH_BYTE: *((unsigned char*) vertex) = luaL_optint(L, -1, 255); break;
          case MESH_INT: *((int*) vertex) = luaL_optint(L, -1, 0); break;
        }
        vertex += sizeof(attribute.type);
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  return 0;
}

int l_lovrMeshGetVertexMap(lua_State* L) {
  Mesh* mesh = luax_checktype(L, 1, Mesh);
  size_t count;
  unsigned int* indices = lovrMeshGetVertexMap(mesh, &count);

  if (count == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  for (size_t i = 0; i < count; i++) {
    lua_pushinteger(L, indices[i] + 1);
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
  int indexSize = mesh->indexSize;
  void* indices = realloc(lovrMeshGetVertexMap(mesh, NULL), indexSize * count);

  for (int i = 0; i < count; i++) {
    lua_rawgeti(L, 2, i + 1);
    if (!lua_isnumber(L, -1)) {
      return luaL_error(L, "Mesh vertex map index #%d must be numeric", i);
    }

    int index = lua_tointeger(L, -1);
    if (index > lovrMeshGetVertexCount(mesh) || index < 1) {
      return luaL_error(L, "Invalid vertex map value: %d", index);
    }

    if (indexSize == sizeof(uint16_t)) {
      *(((uint16_t*) indices) + i) = index - 1;
    } else if (indexSize == sizeof(uint32_t)) {
      *(((uint32_t*) indices) + i) = index - 1;
    }

    lua_pop(L, 1);
  }

  lovrMeshSetVertexMap(mesh, indices, count);
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
  int enabled = lua_toboolean(L, 3);
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
  if (lovrMeshSetDrawRange(mesh, rangeStart, rangeCount)) {
    return luaL_error(L, "Invalid mesh draw range (%d, %d)", rangeStart + 1, rangeCount);
  }

  return 0;
}

const luaL_Reg lovrMesh[] = {
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
  { NULL, NULL }
};
