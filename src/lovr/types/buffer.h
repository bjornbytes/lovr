#include "graphics/buffer.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int luax_pushvertex(lua_State* L, void* vertex, BufferFormat format);
void luax_checkbufferformat(lua_State* L, int index, BufferFormat* format);
extern const luaL_Reg lovrBuffer[];

int l_lovrBufferDraw(lua_State* L);
int l_lovrBufferGetVertexCount(lua_State* L);
int l_lovrBufferGetVertex(lua_State* L);
int l_lovrBufferSetVertex(lua_State* L);
int l_lovrBufferSetVertices(lua_State* L);
int l_lovrBufferGetVertexMap(lua_State* L);
int l_lovrBufferSetVertexMap(lua_State* L);
int l_lovrBufferGetDrawMode(lua_State* L);
int l_lovrBufferSetDrawMode(lua_State* L);
int l_lovrBufferGetDrawRange(lua_State* L);
int l_lovrBufferSetDrawRange(lua_State* L);
int l_lovrBufferGetTexture(lua_State* L);
int l_lovrBufferSetTexture(lua_State* L);
