#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../glfw.h"
#include "../vendor/map/map.h"

typedef struct {
  int size;
  GLfloat* data;
  const char* drawMode;
  GLuint vao;
  GLuint vbo;
} Buffer;

map_int_t BufferDrawModes;

void luax_pushbuffer(lua_State* L, Buffer* buffer);
Buffer* luax_checkbuffer(lua_State* L, int index);
int luax_destroybuffer(lua_State* L);

int lovrBufferDraw(lua_State* L);
int lovrBufferGetVertex(lua_State* L);
int lovrBufferSetVertex(lua_State* L);
int lovrBufferGetDrawMode(lua_State* L);
int lovrBufferSetDrawMode(lua_State* L);

extern const luaL_Reg lovrBuffer[];
