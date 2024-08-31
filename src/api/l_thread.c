#include "api.h"
#include "data/blob.h"
#include "event/event.h"
#include "thread/thread.h"
#include "core/os.h"
#include "util.h"
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

static char* threadRunner(Thread* thread, Blob* body, Variant* arguments, uint32_t argumentCount) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  luax_preload(L);

  lua_pushcfunction(L, luax_getstack);
  int errhandler = lua_gettop(L);

  if (!luaL_loadbuffer(L, body->data, body->size, "thread")) {
    for (uint32_t i = 0; i < argumentCount; i++) {
      luax_pushvariant(L, &arguments[i]);
    }

    if (!lua_pcall(L, argumentCount, 0, errhandler)) {
      lua_close(L);
      return NULL;
    }
  }

  // Error handling
  size_t length;
  const char* message = lua_tolstring(L, -1, &length);

  if (message) {
    char* error = lovrMalloc(length + 1);
    memcpy(error, message, length + 1);
    lua_close(L);
    return error;
  }

  lua_close(L);
  return NULL;
}

static int l_lovrThreadNewThread(lua_State* L) {
  Blob* blob = luax_totype(L, 1, Blob);
  if (!blob) {
    size_t length;
    const char* str = luaL_checklstring(L, 1, &length);
    if (memchr(str, '\n', MIN(1024, length))) {
      void* data = lovrMalloc(length + 1);
      memcpy(data, str, length + 1);
      blob = lovrBlobCreate(data, length, "thread code");
    } else {
      void* code = luax_readfile(str, &length);
      if (!code) luaL_error(L, "Could not read thread code from file '%s'", str);
      blob = lovrBlobCreate(code, length, str);
    }
  } else {
    lovrRetain(blob);
  }
  Thread* thread = lovrThreadCreate(threadRunner, blob);
  luax_pushtype(L, Thread, thread);
  lovrRelease(thread, lovrThreadDestroy);
  lovrRelease(blob, lovrBlobDestroy);
  return 1;
}

static int l_lovrThreadNewChannel(lua_State* L) {
  Channel* channel = lovrChannelCreate(0);
  luax_pushtype(L, Channel, channel);
  lovrRelease(channel, lovrChannelDestroy);
  return 1;
}

static int l_lovrThreadGetChannel(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  Channel* channel = lovrThreadGetChannel(name);
  luax_pushtype(L, Channel, channel);
  // Note: Channels are intentionally not released here (see thread.h)
  return 1;
}

static const luaL_Reg lovrThreadModule[] = {
  { "newThread", l_lovrThreadNewThread },
  { "newChannel", l_lovrThreadNewChannel },
  { "getChannel", l_lovrThreadGetChannel },
  { NULL, NULL }
};

extern const luaL_Reg lovrThread[];
extern const luaL_Reg lovrChannel[];

int luaopen_lovr_thread(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrThreadModule);
  luax_registertype(L, Thread);
  luax_registertype(L, Channel);

  int32_t workers = -1;

  luax_pushconf(L);
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "thread");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "workers");
      if (lua_type(L, -1) == LUA_TNUMBER) {
        workers = lua_tointeger(L, -1);
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lovrThreadModuleInit(workers);
  luax_atexit(L, lovrThreadModuleDestroy);
  return 1;
}
