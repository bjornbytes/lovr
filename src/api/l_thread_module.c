#include "api.h"
#include "event/event.h"
#include "filesystem/filesystem.h"
#include "thread/thread.h"
#include "thread/channel.h"
#include "core/ref.h"
#include <stdlib.h>
#include <string.h>

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
  lovrSetErrorCallback((errorFn*) luax_vthrow, L);

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  luaL_register(L, NULL, lovrModules);
  lua_pop(L, 2);

  if (luaL_loadbuffer(L, thread->body->data, thread->body->size, "thread") || lua_pcall(L, 0, 0, 0)) {
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
  Blob* blob = luax_totype(L, 1, Blob);
  if (!blob) {
    size_t length;
    const char* str = lua_tolstring(L, 1, &length);
    if (memchr(str, '\n', MIN(1024, length))) {
      blob = lovrBlobCreate(strdup(str), length, "thread code");
    } else {
      void* code = lovrFilesystemRead(str, -1, &length);
      lovrAssert(code, "Could not read thread code from %s", str);
      blob = lovrBlobCreate(code, length, str);
    }
  } else {
    lovrRetain(blob);
  }
  Thread* thread = lovrThreadCreate(threadRunner, blob);
  luax_pushtype(L, Thread, thread);
  lovrRelease(Thread, thread);
  lovrRelease(Blob, blob);
  return 1;
}

static int l_lovrThreadGetChannel(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  Channel* channel = lovrThreadGetChannel(name);
  luax_pushtype(L, Channel, channel);
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
  luax_registertype(L, Thread);
  luax_registertype(L, Channel);
  if (lovrThreadModuleInit()) {
    luax_atexit(L, lovrThreadModuleDestroy);
  }
  return 1;
}
