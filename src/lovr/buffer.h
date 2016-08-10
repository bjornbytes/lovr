#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../graphics/buffer.h"

void luax_pushbuffer(lua_State* L, Buffer* buffer);
Buffer* luax_checkbuffer(lua_State* L, int index);
int luax_destroybuffer(lua_State* L);
extern const luaL_Reg lovrBuffer[];

int l_lovrBufferDraw(lua_State* L);
int l_lovrBufferGetVertex(lua_State* L);
int l_lovrBufferSetVertex(lua_State* L);
int l_lovrBufferGetDrawMode(lua_State* L);
int l_lovrBufferSetDrawMode(lua_State* L);
int l_lovrBufferGetDrawRange(lua_State* L);
int l_lovrBufferSetDrawRange(lua_State* L);
