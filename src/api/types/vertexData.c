#include "api.h"

void luax_checkvertexformat(lua_State* L, int index, VertexFormat* format) {
  if (!lua_istable(L, index)) {
    return;
  }

  int length = lua_objlen(L, index);
  lovrAssert(length <= 8, "Only 8 vertex attributes are supported");
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
    AttributeType* type = (AttributeType*) luax_checkenum(L, -2, &AttributeTypes, "mesh attribute type");
    int count = lua_tointeger(L, -1);
    vertexFormatAppend(format, name, *type, count);
    lua_pop(L, 4);
  }
}

int luax_pushvertexformat(lua_State* L, VertexFormat* format) {
  lua_newtable(L);
  for (int i = 0; i < format->count; i++) {
    Attribute attribute = format->attributes[i];
    lua_newtable(L);

    // Name
    lua_pushstring(L, attribute.name);
    lua_rawseti(L, -2, 1);

    // Type
    luax_pushenum(L, &AttributeTypes, attribute.type);
    lua_rawseti(L, -2, 2);

    // Count
    lua_pushinteger(L, attribute.count);
    lua_rawseti(L, -2, 3);

    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

int luax_pushvertexattribute(lua_State* L, VertexPointer* vertex, Attribute attribute) {
  for (int i = 0; i < attribute.count; i++) {
    switch (attribute.type) {
      case ATTR_FLOAT: lua_pushnumber(L, *vertex->floats++); break;
      case ATTR_BYTE: lua_pushnumber(L, *vertex->bytes++); break;
      case ATTR_INT: lua_pushnumber(L, *vertex->ints++); break;
    }
  }
  return attribute.count;
}

int luax_pushvertex(lua_State* L, VertexPointer* vertex, VertexFormat* format) {
  int count = 0;
  for (int i = 0; i < format->count; i++) {
    count += luax_pushvertexattribute(L, vertex, format->attributes[i]);
  }
  return count;
}

void luax_setvertexattribute(lua_State* L, int index, VertexPointer* vertex, Attribute attribute) {
  for (int i = 0; i < attribute.count; i++) {
    switch (attribute.type) {
      case ATTR_FLOAT: *vertex->floats++ = luaL_optnumber(L, index++, 0.f); break;
      case ATTR_BYTE: *vertex->bytes++ = luaL_optint(L, index++, 255); break;
      case ATTR_INT: *vertex->ints++ = luaL_optint(L, index++, 0); break;
    }
  }
}

void luax_setvertex(lua_State* L, int index, VertexPointer* vertex, VertexFormat* format) {
  if (lua_istable(L, index)) {
    int component = 0;
    for (int i = 0; i < format->count; i++) {
      Attribute attribute = format->attributes[i];
      for (int j = 0; j < attribute.count; j++) {
        lua_rawgeti(L, index, ++component);
        switch (attribute.type) {
          case ATTR_FLOAT: *vertex->floats++ = luaL_optnumber(L, -1, 0.f); break;
          case ATTR_BYTE: *vertex->bytes++ = luaL_optint(L, -1, 255); break;
          case ATTR_INT: *vertex->ints++ = luaL_optint(L, -1, 0); break;
        }
        lua_pop(L, 1);
      }
    }
  } else {
    for (int i = 0; i < format->count; i++) {
      luax_setvertexattribute(L, index, vertex, format->attributes[i]);
      index += format->attributes[i].count;
    }
  }
}

//

int l_lovrVertexDataGetPointer(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  lua_pushlightuserdata(L, vertexData->data.raw);
  return 1;
}

int l_lovrVertexDataGetSize(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  lua_pushinteger(L, vertexData->count * vertexData->format.stride);
  return 1;
}

int l_lovrVertexDataGetString(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  lua_pushlstring(L, vertexData->data.raw, vertexData->count * vertexData->format.stride);
  return 1;
}

int l_lovrVertexDataGetCount(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  uint32_t count = vertexData->count;
  lua_pushinteger(L, count);
  return 1;
}

int l_lovrVertexDataGetFormat(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  return luax_pushvertexformat(L, &vertexData->format);
}

int l_lovrVertexDataGetVertex(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  uint32_t index = (uint32_t) luaL_checkint(L, 2) - 1;
  VertexPointer vertex = { .raw = vertexData->data.bytes + index * vertexData->format.stride };
  return luax_pushvertex(L, &vertex, &vertexData->format);
}

int l_lovrVertexDataSetVertex(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  uint32_t index = (uint32_t) luaL_checkint(L, 2) - 1;
  lovrAssert(index < vertexData->count, "Invalid vertex index: %d", index + 1);
  VertexFormat* format = &vertexData->format;
  VertexPointer* vertex = &vertexData->data;
  vertex->bytes += index * format->stride;
  luax_setvertex(L, 3, vertex, format);
  return 0;
}

int l_lovrVertexDataGetVertexAttribute(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  uint32_t vertexIndex = (uint32_t) luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  VertexFormat* format = &vertexData->format;
  lovrAssert(vertexIndex < vertexData->count, "Invalid vertex index: %d", vertexIndex + 1);
  lovrAssert(attributeIndex >= 0 && attributeIndex < format->count, "Invalid attribute index: %d", attributeIndex + 1);
  Attribute attribute = format->attributes[attributeIndex];
  VertexPointer vertex = vertexData->data;
  vertex.bytes += vertexIndex * format->stride + attribute.offset;
  return luax_pushvertexattribute(L, &vertex, attribute);
}

int l_lovrVertexDataSetVertexAttribute(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  uint32_t vertexIndex = (uint32_t) luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  VertexFormat* format = &vertexData->format;
  lovrAssert(vertexIndex < vertexData->count, "Invalid vertex index: %d", vertexIndex + 1);
  lovrAssert(attributeIndex >= 0 && attributeIndex < format->count, "Invalid attribute index: %d", attributeIndex + 1);
  Attribute attribute = format->attributes[attributeIndex];
  VertexPointer vertex = vertexData->data;
  vertex.bytes += vertexIndex * format->stride + attribute.offset;
  luax_setvertexattribute(L, 4, &vertex, attribute);
  return 0;
}

int l_lovrVertexDataSetVertices(lua_State* L) {
  VertexData* vertexData = luax_checktype(L, 1, VertexData);
  VertexFormat* format = &vertexData->format;
  luaL_checktype(L, 2, LUA_TTABLE);
  uint32_t vertexCount = lua_objlen(L, 2);
  int start = luaL_optnumber(L, 3, 1) - 1;
  lovrAssert(start + vertexCount <= vertexData->count, "VertexData can only hold %d vertices", vertexData->count);
  VertexPointer vertices = vertexData->data;
  vertices.bytes += start * format->stride;

  for (uint32_t i = 0; i < vertexCount; i++) {
    lua_rawgeti(L, 2, i + 1);
    luaL_checktype(L, -1, LUA_TTABLE);
    luax_setvertex(L, -1, &vertices, format);
    lua_pop(L, 1);
  }

  return 0;
}

const luaL_Reg lovrVertexData[] = {
  { "getPointer", l_lovrVertexDataGetPointer },
  { "getSize", l_lovrVertexDataGetSize },
  { "getString", l_lovrVertexDataGetString },
  { "getCount", l_lovrVertexDataGetCount },
  { "getFormat", l_lovrVertexDataGetFormat },
  { "getVertex", l_lovrVertexDataGetVertex },
  { "setVertex", l_lovrVertexDataSetVertex },
  { "getVertexAttribute", l_lovrVertexDataGetVertexAttribute },
  { "setVertexAttribute", l_lovrVertexDataSetVertexAttribute },
  { "setVertices", l_lovrVertexDataSetVertices },
  { NULL, NULL }
};
