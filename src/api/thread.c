#include "api.h"
#include "event/event.h"
#include "thread/thread.h"

static int threadRunner(void* data) {
  Thread* thread = (Thread*) data;

  lovrRetain(thread);
  mtx_lock(&thread->lock);
  thread->running = true;
  thread->error = NULL;
  mtx_unlock(&thread->lock);

  // Lua state
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  lovrSetErrorCallback((lovrErrorHandler) luax_vthrow, L);

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luaL_register(L, NULL, lovrModules);
  lua_pop(L, 2);

  if (luaL_loadbuffer(L, thread->body, strlen(thread->body), "thread") || lua_pcall(L, 0, 0, 0)) {
    thread->error = lua_tostring(L, -1);
  }

  mtx_lock(&thread->lock);
  thread->running = false;
  mtx_unlock(&thread->lock);
  lovrRelease(Thread, thread);

  if (thread->error) {
    lovrEventPush((Event) {
      .type = EVENT_THREAD_ERROR,
      .data.thread = { thread, strdup(thread->error) }
    });
    lua_close(L);
    return 1;
  }

  lua_close(L);
  return 0;
}

static int l_lovrThreadNewThread(lua_State* L) {
  const char* body = luaL_checkstring(L, 1);
  Thread* thread = lovrThreadCreate(threadRunner, body);
  luax_pushobject(L, thread);
  lovrRelease(Thread, thread);
  return 1;
}

static int l_lovrThreadGetChannel(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  Channel* channel = lovrThreadGetChannel(name);
  luax_pushobject(L, channel);
  return 1;
}

static const luaL_Reg lovrThreadModule[] = {
  { "newThread", l_lovrThreadNewThread },
  { "getChannel", l_lovrThreadGetChannel },
  { NULL, NULL }
};

int luaopen_lovr_thread(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrThreadModule);
  luax_registertype(L, "Thread", lovrThread);
  luax_registertype(L, "Channel", lovrChannel);
  if (lovrThreadModuleInit()) {
    luax_atexit(L, lovrThreadModuleDestroy);
  }
  return 1;
}
