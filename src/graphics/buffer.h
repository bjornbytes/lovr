#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../glfw.h"

typedef struct {
  GLfloat* data;
  GLenum drawMode;
  GLuint vao;
  GLuint vbo;
} Buffer;

void luax_pushbuffer(lua_State* L, Buffer* buffer);
Buffer* luax_checkbuffer(lua_State* L, int index);

int lovrBufferDraw(lua_State* L);
int lovrBufferSetVertex(lua_State* L);
extern const luaL_Reg lovrBuffer[];
