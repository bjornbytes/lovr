#include "buffer.h"
#include <stdlib.h>

void luax_pushbuffer(lua_State* L, Buffer* buffer) {
  Buffer** userdata = (Buffer**) lua_newuserdata(L, sizeof(Buffer*));
  luaL_getmetatable(L, "Buffer");
  lua_setmetatable(L, -2);
  *userdata = buffer;
}

Buffer* luax_checkbuffer(lua_State* L, int index) {
  return *(Buffer**) luaL_checkudata(L, index, "Buffer");
}

int luax_destroybuffer(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  glDeleteBuffers(1, &buffer->vbo);
  glDeleteVertexArrays(1, &buffer->vao);
  free(buffer->data);
  free(buffer);
  return 0;
}

int lovrBufferDraw(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  GLenum* drawMode = (GLenum*)map_get(&BufferDrawModes, buffer->drawMode);

  if (drawMode == NULL) {
    return luaL_error(L, "Invalid buffer draw mode '%s'", buffer->drawMode);
  }

  glBindVertexArray(buffer->vao);
  glEnableVertexAttribArray(0);
  glDrawArrays(*drawMode, buffer->rangeStart, buffer->rangeCount);
  glDisableVertexAttribArray(0);

  return 0;
}

int lovrBufferGetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lua_pushstring(L, buffer->drawMode);
  return 1;
}

int lovrBufferSetDrawMode(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  const char* drawMode = luaL_checkstring(L, 2);

  if (!map_get(&BufferDrawModes, drawMode)) {
    return luaL_error(L, "Invalid buffer draw mode '%s'", drawMode);
  }

  buffer->drawMode = drawMode;

  return 0;
}

int lovrBufferGetVertex(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int index = luaL_checkint(L, 2) - 1;

  lua_pushnumber(L, buffer->data[3 * index + 0]);
  lua_pushnumber(L, buffer->data[3 * index + 1]);
  lua_pushnumber(L, buffer->data[3 * index + 2]);

  return 3;
}

int lovrBufferSetVertex(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int index = luaL_checkint(L, 2) - 1;
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);
  float z = luaL_checknumber(L, 5);

  buffer->data[3 * index + 0] = x;
  buffer->data[3 * index + 1] = y;
  buffer->data[3 * index + 2] = z;

  glBindVertexArray(buffer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);
  glBufferData(GL_ARRAY_BUFFER, buffer->size * 3 * sizeof(GLfloat), buffer->data, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

  return 0;
}

int lovrBufferGetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  lua_pushnumber(L, buffer->rangeStart + 1);
  lua_pushnumber(L, buffer->rangeStart + 1 + buffer->rangeCount);
  return 2;
}

int lovrBufferSetDrawRange(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);
  int rangeStart = luaL_checknumber(L, 2);
  int rangeEnd = luaL_checknumber(L, 3);

  if (rangeStart <= 0 || rangeEnd <= 0 || rangeStart > rangeEnd) {
    return luaL_error(L, "Invalid buffer draw range (%d, %d)", rangeStart, rangeEnd);
  }

  buffer->rangeStart = rangeStart - 1;
  buffer->rangeCount = rangeEnd - rangeStart + 1;

  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "draw", lovrBufferDraw },
  { "getVertex", lovrBufferGetVertex },
  { "setVertex", lovrBufferSetVertex },
  { "getDrawMode", lovrBufferGetDrawMode },
  { "setDrawMode", lovrBufferSetDrawMode },
  { "getDrawRange", lovrBufferGetDrawRange },
  { "setDrawRange", lovrBufferSetDrawRange },
  { NULL, NULL }
};
