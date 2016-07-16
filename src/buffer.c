#include "buffer.h"
#include "glfw.h"

void luax_pushbuffer(lua_State* L, Buffer* buffer) {
  Buffer** userdata = (Buffer**) lua_newuserdata(L, sizeof(Buffer*));

  luaL_getmetatable(L, "Buffer");
  lua_setmetatable(L, -2);

  *userdata = buffer;
}

Buffer* luax_checkbuffer(lua_State* L, int index) {
  return *(Buffer**) luaL_checkudata(L, index, "Buffer");
}

int lovrBufferDraw(lua_State* L) {
  Buffer* buffer = luax_checkbuffer(L, 1);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, buffer->data);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  return 0;
}

const luaL_Reg lovrBuffer[] = {
  { "draw", lovrBufferDraw },
  { NULL, NULL }
};
