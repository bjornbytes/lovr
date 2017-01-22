#include "lovr/types/buffer.h"
#include "lovr/types/texture.h"
#include "lovr/graphics.h"
#include "util.h"

void luax_checkbufferformat(lua_State* L, int index, BufferFormat* format) {
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
    BufferAttributeType* type = (BufferAttributeType*) luax_checkenum(L, -2, &BufferAttributeTypes, "buffer attribute type");
    int count = lua_tointeger(L, -1);
    BufferAttribute attribute = { .name = name, .type = *type, .count = count };
    vec_push(format, attribute);
    lua_pop(L, 4);
  }
}

const luaL_Reg lovrBuffer[] = {
  { "draw", l_lovrBufferDraw },
  { "getVertexCount", l_lovrBufferGetVertexCount },
  { "getVertex", l_lovrBufferGetVertex },
  { "setVertex", l_lovrBufferSetVertex },
  { "getVertexAttribute", l_lovrBufferGetVertexAttribute },
  { "setVertexAttribute", l_lovrBufferSetVertexAttribute },
  { "setVertices", l_lovrBufferSetVertices },
  { "getVertexMap", l_lovrBufferGetVertexMap },
  { "setVertexMap", l_lovrBufferSetVertexMap },
  { "getDrawMode", l_lovrBufferGetDrawMode },
  { "setDrawMode", l_lovrBufferSetDrawMode },
  { "getDrawRange", l_lovrBufferGetDrawRange },
  { "setDrawRange", l_lovrBufferSetDrawRange },
  { "getTexture", l_lovrBufferGetTexture },
  { "setTexture", l_lovrBufferSetTexture },
  { NULL, NULL }
};

int l_lovrBufferDraw(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  lovrBufferDraw(buffer);
  return 0;
}

int l_lovrBufferGetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  luax_pushenum(L, &BufferDrawModes, lovrBufferGetDrawMode(buffer));
  return 1;
}

int l_lovrBufferSetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  BufferDrawMode* drawMode = (BufferDrawMode*) luax_checkenum(L, 2, &BufferDrawModes, "buffer draw mode");
  lovrBufferSetDrawMode(buffer, *drawMode);
  return 0;
}

int l_lovrBufferGetVertexCount(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  lua_pushnumber(L, lovrBufferGetVertexCount(buffer));
  return 1;
}

int l_lovrBufferGetVertex(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  int index = luaL_checkint(L, 2) - 1;
  char* vertex = lovrBufferGetScratchVertex(buffer);
  lovrBufferGetVertex(buffer, index, vertex);
  BufferFormat format = lovrBufferGetVertexFormat(buffer);

  int total = 0;
  for (int i = 0; i < format.length; i++) {
    BufferAttribute attribute = format.data[i];
    total += attribute.count;
    for (int j = 0; j < attribute.count; j++) {
      switch (attribute.type) {
        case BUFFER_FLOAT: lua_pushnumber(L, *((float*) vertex)); break;
        case BUFFER_BYTE: lua_pushnumber(L, *((unsigned char*) vertex)); break;
        case BUFFER_INT: lua_pushnumber(L, *((int*) vertex)); break;
      }

      vertex += sizeof(attribute.type);
    }
  }

  return total;
}

int l_lovrBufferSetVertex(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  int index = luaL_checkint(L, 2) - 1;
  BufferFormat format = lovrBufferGetVertexFormat(buffer);
  char* vertex = lovrBufferGetScratchVertex(buffer);

  if (index < 0 || index >= buffer->size) {
    return luaL_error(L, "Invalid buffer vertex index: %d", index + 1);
  }

  // Unwrap table
  int arg = 3;
  if (lua_istable(L, 3)) {
    arg++;
    for (size_t i = 0; i < lua_objlen(L, 3); i++) {
      lua_rawgeti(L, 3, i + 1);
    }
  }

  for (int i = 0; i < format.length; i++) {
    BufferAttribute attribute = format.data[i];
    for (int j = 0; j < attribute.count; j++) {
      switch (attribute.type) {
        case BUFFER_FLOAT: *((float*) vertex) = luaL_optnumber(L, arg++, 0.f); break;
        case BUFFER_BYTE: *((unsigned char*) vertex) = luaL_optint(L, arg++, 255); break;
        case BUFFER_INT: *((int*) vertex) = luaL_optint(L, arg++, 0); break;
      }

      vertex += sizeof(attribute.type);
    }
  }

  lovrBufferSetVertex(buffer, index, lovrBufferGetScratchVertex(buffer));
  return 0;
}

int l_lovrBufferGetVertexAttribute(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  char* vertex = lovrBufferGetScratchVertex(buffer);
  lovrBufferGetVertex(buffer, vertexIndex, vertex);
  BufferFormat format = lovrBufferGetVertexFormat(buffer);

  if (vertexIndex < 0 || vertexIndex >= buffer->size) {
    return luaL_error(L, "Invalid buffer vertex index: %d", vertexIndex + 1);
  } else if (attributeIndex < 0 || attributeIndex >= format.length) {
    return luaL_error(L, "Invalid buffer attribute index: %d", attributeIndex + 1);
  }

  BufferAttribute attribute;
  for (int i = 0; i <= attributeIndex; i++) {
    attribute = format.data[i];
    if (i == attributeIndex) {
      for (int j = 0; j < attribute.count; j++) {
        switch (attribute.type) {
          case BUFFER_FLOAT: lua_pushnumber(L, *((float*) vertex)); break;
          case BUFFER_BYTE: lua_pushinteger(L, *((unsigned char*) vertex)); break;
          case BUFFER_INT: lua_pushinteger(L, *((int*) vertex)); break;
        }
        vertex += sizeof(attribute.type);
      }
    } else {
      vertex += attribute.count * sizeof(attribute.type);
    }
  }

  return attribute.count;
}

int l_lovrBufferSetVertexAttribute(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  int vertexIndex = luaL_checkint(L, 2) - 1;
  int attributeIndex = luaL_checkint(L, 3) - 1;
  char* vertex = lovrBufferGetScratchVertex(buffer);
  lovrBufferGetVertex(buffer, vertexIndex, vertex);
  BufferFormat format = lovrBufferGetVertexFormat(buffer);

  if (vertexIndex < 0 || vertexIndex >= buffer->size) {
    return luaL_error(L, "Invalid buffer vertex index: %d", vertexIndex + 1);
  } else if (attributeIndex < 0 || attributeIndex >= format.length) {
    return luaL_error(L, "Invalid buffer attribute index: %d", attributeIndex + 1);
  }

  int arg = 4;
  for (int i = 0; i <= attributeIndex; i++) {
    BufferAttribute attribute = format.data[i];
    if (i == attributeIndex) {
      for (int j = 0; j < attribute.count; j++) {
        switch (attribute.type) {
          case BUFFER_FLOAT: *((float*) vertex) = luaL_optnumber(L, arg++, 0.f); break;
          case BUFFER_BYTE: *((unsigned char*) vertex) = luaL_optint(L, arg++, 255); break;
          case BUFFER_INT: *((int*) vertex) = luaL_optint(L, arg++, 0); break;
        }
        vertex += sizeof(attribute.type);
      }
    } else {
      vertex += attribute.count * sizeof(attribute.type);
    }
  }

  lovrBufferSetVertex(buffer, vertexIndex, lovrBufferGetScratchVertex(buffer));
  return 0;
}

int l_lovrBufferSetVertices(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  BufferFormat format = lovrBufferGetVertexFormat(buffer);
  luaL_checktype(L, 2, LUA_TTABLE);
  int vertexCount = lua_objlen(L, 2);

  if (vertexCount > lovrBufferGetVertexCount(buffer)) {
    return luaL_error(L, "Too many vertices for Buffer\n", lovrBufferGetVertexCount(buffer));
  }

  char* vertices = malloc(buffer->stride * vertexCount);
  char* vertex = vertices;

  for (int i = 0; i < vertexCount; i++) {
    lua_rawgeti(L, 2, i + 1);
    int component = 0;
    for (int j = 0; j < format.length; j++) {
      BufferAttribute attribute = format.data[j];
      for (int k = 0; k < attribute.count; k++) {
        lua_rawgeti(L, -1, ++component);
        switch (attribute.type) {
          case BUFFER_FLOAT: *((float*) vertex) = luaL_optnumber(L, -1, 0.f); break;
          case BUFFER_BYTE: *((unsigned char*) vertex) = luaL_optint(L, -1, 255); break;
          case BUFFER_INT: *((int*) vertex) = luaL_optint(L, -1, 0); break;
        }
        vertex += sizeof(attribute.type);
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  lovrBufferSetVertices(buffer, vertices, buffer->stride * vertexCount);
  free(vertices);
  return 0;
}

int l_lovrBufferGetVertexMap(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  int count;
  unsigned int* indices = lovrBufferGetVertexMap(buffer, &count);

  if (count == 0) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  for (int i = 0; i < count; i++) {
    lua_pushinteger(L, indices[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

int l_lovrBufferSetVertexMap(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);

  if (lua_isnoneornil(L, 2)) {
    lovrBufferSetVertexMap(buffer, NULL, 0);
    return 0;
  }

  luaL_checktype(L, 2, LUA_TTABLE);
  int count = lua_objlen(L, 2);
  unsigned int* indices = malloc(count * sizeof(unsigned int));

  for (int i = 0; i < count; i++) {
    lua_rawgeti(L, 2, i + 1);
    if (!lua_isnumber(L, -1)) {
      free(indices);
      return luaL_error(L, "Buffer vertex map index #%d must be numeric", i);
    }

    int index = lua_tointeger(L, -1);
    if (index > buffer->size || index < 0) {
      free(indices);
      return luaL_error(L, "Invalid vertex map value: %d", index);
    }

    indices[i] = index - 1;
    lua_pop(L, 1);
  }

  lovrBufferSetVertexMap(buffer, indices, count);
  free(indices);
  return 0;
}

int l_lovrBufferGetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  if (!lovrBufferIsRangeEnabled(buffer)) {
    lua_pushnil(L);
    return 1;
  }

  int start, count;
  lovrBufferGetDrawRange(buffer, &start, &count);
  lua_pushinteger(L, start + 1);
  lua_pushinteger(L, count);
  return 2;
}

int l_lovrBufferSetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  if (lua_isnoneornil(L, 2)) {
    lovrBufferSetRangeEnabled(buffer, 0);
    return 0;
  }

  lovrBufferSetRangeEnabled(buffer, 1);
  int rangeStart = luaL_checkinteger(L, 2) - 1;
  int rangeCount = luaL_checkinteger(L, 3);
  if (lovrBufferSetDrawRange(buffer, rangeStart, rangeCount)) {
    return luaL_error(L, "Invalid buffer draw range (%d, %d)", rangeStart + 1, rangeCount);
  }

  return 0;
}

int l_lovrBufferGetTexture(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  Texture* texture = lovrBufferGetTexture(buffer);

  if (texture) {
    luax_pushtype(L, Texture, texture);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

int l_lovrBufferSetTexture(lua_State* L) {
  Buffer* buffer = luax_checktype(L, 1, Buffer);
  Texture* texture = lua_isnoneornil(L, 2) ? NULL : luax_checktype(L, 2, Texture);
  lovrBufferSetTexture(buffer, texture);
  return 0;
}
