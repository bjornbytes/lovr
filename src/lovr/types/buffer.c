#include "buffer.h"
#include "texture.h"
#include "../graphics.h"

void luax_pushbuffer(lua_State* L, Buffer* buffer) {
  if (buffer == NULL) {
    lua_pushnil(L);
    return;
  }

  Buffer** userdata = (Buffer**) lua_newuserdata(L, sizeof(Buffer*));
  luaL_getmetatable(L, "Buffer");
  lua_setmetatable(L, -2);
  *userdata = buffer;
}

int luax_pushbuffervertex(lua_State* L, void* vertex, BufferFormat format) {
  int count = 0;
  int i;
  BufferAttribute attribute;
  vec_foreach(&format, attribute, i) {
    for (int j = 0; j < attribute.size; j++) {
      if (attribute.type == BUFFER_FLOAT) {
        lua_pushnumber(L, *((float*)vertex));
        vertex = (char*) vertex + sizeof(float);
      } else if (attribute.type == BUFFER_BYTE) {
        lua_pushnumber(L, *((unsigned char*)vertex));
        vertex = (char*) vertex + sizeof(unsigned char);
      }
      count++;
    }
  }
  return count;
}

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
    int size = lua_tointeger(L, -1);
    BufferAttribute attribute = { .name = name, .type = *type, .size = size };
    vec_push(format, attribute);
    lua_pop(L, 4);
  }
}

Buffer* luax_checkbuffer(lua_State* L, int index) {
  return *(Buffer**) luaL_checkudata(L, index, "Buffer");
}

int luax_destroybuffer(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lovrBufferDestroy(buffer);
  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "draw", l_lovrBufferDraw },
  { "getVertexCount", l_lovrBufferGetVertexCount },
  { "getVertex", l_lovrBufferGetVertex },
  { "setVertex", l_lovrBufferSetVertex },
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
  Buffer* buffer = luax_checkbuffer(L, 1);
  lovrBufferDraw(buffer);
  return 0;
}

int l_lovrBufferGetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lua_pushstring(L, map_int_find(&BufferDrawModes, lovrBufferGetDrawMode(buffer)));
  return 1;
}

int l_lovrBufferSetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  BufferDrawMode* drawMode = (BufferDrawMode*) luax_checkenum(L, 2, &BufferDrawModes, "buffer draw mode");
  lovrBufferSetDrawMode(buffer, *drawMode);
  return 0;
}

int l_lovrBufferGetVertexCount(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lua_pushnumber(L, lovrBufferGetVertexCount(buffer));
  return 1;
}

int l_lovrBufferGetVertex(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int index = luaL_checkint(L, 2) - 1;
  void* vertex = lovrBufferGetScratchVertex(buffer);
  lovrBufferGetVertex(buffer, index, vertex);
  BufferFormat format = lovrBufferGetVertexFormat(buffer);
  return luax_pushbuffervertex(L, vertex, format);
}

int l_lovrBufferSetVertex(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int index = luaL_checkint(L, 2) - 1;
  BufferFormat format = lovrBufferGetVertexFormat(buffer);
  void* vertex = lovrBufferGetScratchVertex(buffer);

  if (lua_istable(L, 3)) {
    int tableCount = lua_objlen(L, 3);
    int tableIndex = 1;
    void* v = vertex;
    int i;
    BufferAttribute attribute;

    vec_foreach(&format, attribute, i) {
      for (int j = 0; j < attribute.size; j++) {
        if (attribute.type == BUFFER_FLOAT) {
          float value = 0.f;
          if (tableIndex <= tableCount) {
            lua_rawgeti(L, 3, tableIndex++);
            value = lua_tonumber(L, -1);
            lua_pop(L, 1);
          }

          *((float*) v) = value;
          v = (char*) v + sizeof(float);
        } else if (attribute.type == BUFFER_BYTE) {
          unsigned char value = 255;
          if (tableIndex <= tableCount) {
            lua_rawgeti(L, 3, tableIndex++);
            value = lua_tointeger(L, -1);
            lua_pop(L, 1);
          }

          *((unsigned char*) v) = value;
          v = (char*) v + sizeof(unsigned char);
        }
      }
    }
  } else {
    int argumentCount = lua_gettop(L);
    int argumentIndex = 3;
    void* v = vertex;
    int i;
    BufferAttribute attribute;

    vec_foreach(&format, attribute, i) {
      for (int j = 0; j < attribute.size; j++) {
        if (attribute.type == BUFFER_FLOAT) {
          *((float*) v) = argumentIndex <= argumentCount ? lua_tonumber(L, argumentIndex++) : 0.f;
          v = (char*) v + sizeof(float);
        } else if (attribute.type == BUFFER_BYTE) {
          *((char*) v) = argumentIndex <= argumentCount ? lua_tointeger(L, argumentIndex++) : 255;
          v = (char*) v + sizeof(char);
        }
      }
    }
  }

  lovrBufferSetVertex(buffer, index, vertex);
  return 0;
}

int l_lovrBufferGetVertexMap(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
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
  Buffer* buffer = luax_checkbuffer(L, 1);

  if (lua_isnoneornil(L, 2)) {
    lovrBufferSetVertexMap(buffer, NULL, 0);
    return 0;
  }

  luaL_checktype(L, 2, LUA_TTABLE);
  int count = lua_objlen(L, 2);
  unsigned int* indices = malloc(count * sizeof(int));

  for (int i = 0; i < count; i++) {
    lua_rawgeti(L, 2, i + 1);
    if (!lua_isnumber(L, -1)) {
      return luaL_error(L, "Buffer vertex map index #%d must be numeric", i);
    }
    indices[i] = lua_tointeger(L, -1) - 1;
    lua_pop(L, 1);
  }

  lovrBufferSetVertexMap(buffer, indices, count);
  free(indices);
  return 0;
}

int l_lovrBufferGetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
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
  Buffer* buffer = luax_checkbuffer(L, 1);
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
  Buffer* buffer = luax_checkbuffer(L, 1);
  luax_pushtexture(L, lovrBufferGetTexture(buffer));
  return 1;
}

int l_lovrBufferSetTexture(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  Texture* texture = luax_checktexture(L, 2);
  lovrBufferSetTexture(buffer, texture);
  return 0;
}
