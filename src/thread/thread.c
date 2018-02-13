#include "thread/thread.h"
#include "luax.h"
#include "api.h"
#include <string.h>

static int runner(void* data) {
  Thread* thread = (Thread*) data;

  lovrRetain(&thread->ref);
  mtx_lock(&thread->lock);
  thread->running = true;
  thread->error = NULL;
  mtx_unlock(&thread->lock);

  // Lua state
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

  // lovr
  lua_newtable(L);
  /*lua_pushcfunction(L, lovrGetOS);
  lua_setfield(L, -2, "getOS");
  lua_pushcfunction(L, lovrGetVersion);
  lua_setfield(L, -2, "getVersion");*/
  lua_setglobal(L, "lovr");

  // Preload modules
  luax_preloadmodule(L, "lovr.audio", l_lovrAudioInit);
  luax_preloadmodule(L, "lovr.data", l_lovrDataInit);
  luax_preloadmodule(L, "lovr.event", l_lovrEventInit);
  luax_preloadmodule(L, "lovr.filesystem", l_lovrFilesystemInit);
  luax_preloadmodule(L, "lovr.graphics", l_lovrGraphicsInit);
  luax_preloadmodule(L, "lovr.headset", l_lovrHeadsetInit);
  luax_preloadmodule(L, "lovr.math", l_lovrMathInit);
  luax_preloadmodule(L, "lovr.physics", l_lovrPhysicsInit);
  luax_preloadmodule(L, "lovr.thread", l_lovrThreadInit);
  luax_preloadmodule(L, "lovr.timer", l_lovrTimerInit);

  // Preload libraries
  //luax_preloadmodule(L, "json", luaopen_cjson);
  //luax_preloadmodule(L, "enet", luaopen_enet);

  if (luaL_loadbuffer(L, thread->body, strlen(thread->body), "thread") || lua_pcall(L, 0, 0, 0)) {
    thread->error = lua_tostring(L, -1);
    //lua_getglobal(lovr, "threaderror");
    lovrRelease(&thread->ref);
    return 1;
  }

  mtx_lock(&thread->lock);
  thread->running = false;
  mtx_unlock(&thread->lock);
  lovrRelease(&thread->ref);
  return 0;
}

Thread* lovrThreadCreate(const char* body) {
  Thread* thread = lovrAlloc(sizeof(Thread), lovrThreadDestroy);
  if (!thread) return NULL;

  thread->body = body;
  thread->error = NULL;
  thread->running = false;
  mtx_init(&thread->lock, mtx_plain);

  return thread;
}

void lovrThreadDestroy(const Ref* ref) {
  Thread* thread = containerof(ref, Thread);
  mtx_destroy(&thread->lock);
  thrd_detach(thread->handle);
  free(thread);
}

void lovrThreadStart(Thread* thread) {
  bool running = lovrThreadIsRunning(thread);

  if (running) {
    return;
  }

  if (thrd_create(&thread->handle, runner, thread) != thrd_success) {
    lovrThrow("Could not create thread...sorry");
    return;
  }
}

void lovrThreadWait(Thread* thread) {
  thrd_join(thread->handle, NULL);
}

bool lovrThreadIsRunning(Thread* thread) {
  mtx_lock(&thread->lock);
  bool running = thread->running;
  mtx_unlock(&thread->lock);
  return running;
}

const char* lovrThreadGetError(Thread* thread) {
  return thread->error;
}
