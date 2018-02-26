#include "api.h"
#include "thread/thread.h"
#include "event/event.h"

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
  l_lovrInit(L);
  lua_setglobal(L, "lovr");

  if (luaL_loadbuffer(L, thread->body, strlen(thread->body), "thread") || lua_pcall(L, 0, 0, 0)) {
    thread->error = lua_tostring(L, -1);
  }

  mtx_lock(&thread->lock);
  thread->running = false;
  mtx_unlock(&thread->lock);
  lovrRelease(thread);

  if (thread->error) {
    Event event;
    event.type = EVENT_THREAD_ERROR;
    event.data.threaderror.thread = thread;
    event.data.threaderror.error = thread->error;
    lovrEventPush(event);
    lua_close(L);
    return 1;
  }

  lua_close(L);
  return 0;
}

int l_lovrThreadInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrThreadModule);
  luax_registertype(L, "Thread", lovrThread);
  luax_registertype(L, "Channel", lovrChannel);
  return 1;
}

int l_lovrThreadNewThread(lua_State* L) {
  const char* body = luaL_checkstring(L, 1);
  Thread* thread = lovrThreadCreate(threadRunner, body);
  luax_pushtype(L, Thread, thread);
  lovrRelease(thread);
  return 1;
}

int l_lovrThreadGetChannel(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  Channel* channel = lovrThreadGetChannel(name);
  luax_pushtype(L, Channel, channel);
  lovrRelease(channel);
  return 1;
}

const luaL_Reg lovrThreadModule[] = {
  { "newThread", l_lovrThreadNewThread },
  { "getChannel", l_lovrThreadGetChannel },
  { NULL, NULL }
};
