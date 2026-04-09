#include "api.h"
#include "task/task.h"
#include "core/job.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static void luax_pintask(lua_State* L, Task* task) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrtasks");
  lua_pushlightuserdata(L, task);
  lua_pushthread(L);
  lua_rawset(L, -3);
  lua_pop(L, 1);
}

static void luax_unpintask(lua_State* L, Task* task) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrtasks");
  lua_pushlightuserdata(L, task);
  lua_pushnil(L);
  lua_rawset(L, -3);
  lua_pop(L, 1);
}

int luax_yieldpoll(lua_State* L, fn_task* poll, fn_task* block, fn_continuation* continuation, void* context) {
  Task* task = luax_getthreaddata(L);

  if (!task) {
    if (block(&context)) {
      return continuation ? continuation(L, true, context) : 0;
    } else {
      if (continuation) continuation(L, false, context);
      return luaL_error(L, lovrGetError());
    }
  }

  lovrTaskWaitPoll(task, poll, block, continuation, context);
  luax_pintask(L, task);
  return lua_yield(L, 0);
}

static void runJob(void* arg) {
  Task* task = arg;

  if (!task->fn(&task->context)) {
    char* expected = NULL;
    char* error = lovrStrdup(lovrGetError());
    if (!atomic_compare_exchange_strong(&task->error, &expected, error)) {
      lovrFree(error);
    }
  }

  if (atomic_fetch_sub(&task->deps, 1) == 1 && task->waiting == WAIT_JOB) {
    lovrTaskEnqueue(task);
  }
}

int luax_yieldjob(lua_State* L, fn_task* fn, fn_continuation* continuation, void* context, uint32_t count) {
  Task* task = luax_getthreaddata(L);

  if (!task) {
    if (count == 1) {
      if (fn(&context)) {
        return continuation ? continuation(L, true, context) : 0;
      } else {
        if (continuation) continuation(L, false, context);
        return luaL_error(L, lovrGetError());
      }
    } else {
      Task stack = { 0 };
      task = &stack;
      task->fn = fn;
      task->context = context;
      atomic_store(&task->deps, count);

      for (uint32_t i = 0; i < count; i++) {
        if (!job_start(runJob, task)) {
          runJob(task);
        }
      }

      while (atomic_load(&task->deps) > 0) {
        job_spin();
      }

      char* error = atomic_load(&task->error);

      if (error) {
        if (continuation) continuation(L, false, task->context);
        lua_pushstring(L, error);
        lovrFree(error);
        return lua_error(L);
      } else {
        return continuation ? continuation(L, true, task->context) : 0;
      }
    }
  }

  task->fn = fn;
  task->context = context;
  task->continuation = continuation;
  atomic_store(&task->deps, count);
  task->waiting = WAIT_JOB;

  for (uint32_t i = 0; i < count; i++) {
    if (!job_start(runJob, task)) {
      runJob(task);
    }
  }

  if (atomic_load(&task->deps) == 0) {
    lovrTaskDequeue(task);
    task->waiting = WAIT_NONE;
    const char* error = atomic_load(&task->error);

    if (error) {
      if (continuation) continuation(L, false, task->context);
      return luaL_error(L, error);
    } else {
      return continuation ? continuation(L, true, task->context) : 0;
    }
  }

  luax_pintask(L, task);
  return lua_yield(L, 0);
}

static int luax_runtask(Task* task, int n) {
  lua_State* T = task->T;

  if (task->waiting) {
    // Unpin from registry
    luax_unpintask(T, task);

    // Remove it from the ready queue if it's there
    lovrTaskDequeue(task);

    // Set up resume values
    if (task->waiting == WAIT_TASK) {
      n = 0;

      // Copy the first result from each dependency, replacing each coroutine with its result
      int top = lua_gettop(T);
      for (int i = 1; i < top; i++) {
        lua_State* D = lua_tothread(T, i);
        lua_pushvalue(D, 1);
        lua_xmove(D, T, 1);
        lua_replace(T, i);
        n++;
      }

      // ...Except for the last dependency.  Copy ALL of its results instead
      lua_State* D = lua_tothread(T, top);
      int rest = lua_gettop(D);
      luax_check(T, lua_checkstack(D, rest), "stack overflow");
      for (int i = 1; i <= rest; i++) {
        lua_pushvalue(D, i);
      }
      lua_pop(T, 1);
      lua_xmove(D, T, rest);
      n += rest;
    } else {
      n = task->continuation ? task->continuation(T, !task->error, task->context) : 0;
    }

    task->waiting = WAIT_NONE;

    // Handle error: can't actually throw an error in T without Lua 5.2 continuations
    if (task->error) {
      lua_settop(T, 0);
      lua_pushstring(T, task->error);
      lovrTaskFinish(task);
      return LUA_ERRRUN;
    }
  }

  int status = luax_resume(T, n);

  // Handle error/completion
  if (status != LUA_YIELD) {
    if (status != LUA_OK) {
      task->error = lovrStrdup(lua_tostring(T, -1));
    }

    lovrTaskFinish(task);
  }

  return status;
}

static int l_lovrTaskStart(lua_State* L) {
  int args = lua_gettop(L) - 1;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_State* T = lua_newthread(L);
  lua_insert(L, 1);
  lua_xmove(L, T, args + 1);
  Task* task = lovrTaskCreate(T);
  luax_setthreaddata(T, task);
  int status = luax_runtask(task, args);

  if (!task->waiting) {
    luax_setthreaddata(T, NULL);
    lovrTaskDestroy(task);

    if (status != LUA_OK && status != LUA_YIELD) {
      lua_pushvalue(T, -1);
      lua_xmove(T, L, 1);
      return lua_error(L);
    }
  }

  return 1;
}

static int l_lovrTaskResume(lua_State* L) {
  luaL_checktype(L, 1, LUA_TTHREAD);
  lua_State* T = lua_tothread(L, 1);
  Task* task = luax_getthreaddata(T);

  if (!task) {
    task = lovrTaskCreate(T);
    luax_setthreaddata(T, task);
  } else if (task->complete) {
    lua_pushnil(L);
    lua_pushliteral(L, "already complete");
    return 2;
  } else if (!lovrTaskIsReady(task)) {
    lua_pushnil(L);
    lua_pushliteral(L, "not ready");
    return 2;
  }

  int n = 0;

  // If the task wasn't waiting on anything (it yielded with coroutine.yield), pass through the rest
  // of the arguments
  if (!task->waiting) {
    n = lua_gettop(L) - 1;
    lua_xmove(L, T, n);
  }

  int status = luax_runtask(task, n);

  if (task->waiting) {
    lua_pushboolean(L, true);
    return 1;
  }

  luax_setthreaddata(T, NULL);
  lovrTaskDestroy(task);

  if (status == LUA_OK) {
    lua_pushboolean(L, true);
    int n = lua_gettop(T);
    luax_check(L, lua_checkstack(T, n), "stack overflow");
    for (int i = 1; i <= n; i++) {
      lua_pushvalue(T, i);
    }
    lua_xmove(T, L, n);
    return n + 1;
  } else if (status == LUA_YIELD) {
    lua_pushboolean(L, true);
    // It yielded with coroutine.yield, return the results it yielded with
    int n = lua_gettop(T);
    luax_check(L, lua_checkstack(T, n), "stack overflow");
    for (int i = 1; i <= n; i++) {
      lua_pushvalue(T, i);
    }
    lua_xmove(T, L, n);
    return n + 1;
  } else {
    lua_pushboolean(L, false);
    lua_pushvalue(T, -1);
    lua_xmove(T, L, 1);
    return 2;
  }
}

static int l_lovrTaskNext(lua_State* L) {
  Task* task = lovrTaskModuleGetNext();
  if (task) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_lovrtasks");
    lua_pushlightuserdata(L, task);
    lua_rawget(L, -2);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrTaskPoll(lua_State* L) {
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static int luax_waittask(lua_State* T) {
  Task* task = luax_getthreaddata(T);
  luax_check(T, task, "Trying to wait on a coroutine that wasn't resumed with lovr.task.resume");

  if (task->complete) {
    return task->error ? LUA_ERRRUN : LUA_OK;
  }

  if (task->waiting == WAIT_JOB) {
    while (atomic_load(&task->deps) > 0) {
      job_spin();
    }
  } else if (task->waiting == WAIT_POLL) {
    if (!task->block(&task->context)) {
      task->error = lovrStrdup(lovrGetError());
    }
  } else {
    int n = lua_gettop(T);
    for (int i = 1; i <= n; i++) {
      luax_waittask(lua_tothread(T, i));
    }
  }

  return luax_runtask(task, 0);
}

static int l_lovrTaskWait(lua_State* L) {
  Task* self = luax_getthreaddata(L);

  if (lua_istable(L, 1)) {
    int length = luax_len(L, 1);

    for (int i = 1; i <= length; i++) {
      lua_rawgeti(L, 1, i);
    }

    lua_remove(L, 1);
  }

  int n = lua_gettop(L);

  if (n == 0) {
    return 0;
  }

  if (self) {
    for (int i = 1; i <= n; i++) {
      luaL_checktype(L, i, LUA_TTHREAD);
      lua_State* T = lua_tothread(L, i);
      Task* task = luax_getthreaddata(T);
      luax_check(T, task, "Trying to wait on a coroutine that wasn't resumed with lovr.task.resume");
      luax_assert(L, lovrTaskAddDependency(self, task));
    }

    // Only yield if we're actually waiting on something.  If everything was already complete, fall
    // through to the synchronous path, which handles errors and gathers results
    if (self->waiting == WAIT_TASK) {
      luax_pintask(L, self);
      return lua_yield(L, n);
    }
  }

  for (int i = 1; i <= n; i++) {
    luaL_checktype(L, i, LUA_TTHREAD);
    lua_State* T = lua_tothread(L, i);

    for (;;) {
      int status = luax_waittask(T);

      if (status == LUA_OK) {
        break;
      } else if (status != LUA_YIELD) {
        lua_pushboolean(L, false);
        lua_pushvalue(T, -1);
        lua_xmove(T, L, 1);
        return 2;
      }
    }
  }

  int results = 0;

  for (int i = 1; i <= n; i++) {
    lua_State* T = lua_tothread(L, i);

    // Last task returns all args, other tasks return first arg
    if (i < n) {
      lua_pushvalue(T, 1);
      lua_xmove(T, L, 1);
      lua_replace(L, i);
      results++;
    } else {
      int rest = lua_gettop(T);
      for (int j = 1; j <= rest; j++) {
        lua_pushvalue(T, j);
      }
      lua_pop(L, 1);
      lua_xmove(T, L, rest);
      results += rest;
    }
  }

  lua_pushboolean(L, true);
  lua_insert(L, 1);

  return results + 1;
}

static int l_lovrTaskGetStatus(lua_State* L) {
  luaL_checktype(L, 1, LUA_TTHREAD);
  lua_State* T = lua_tothread(L, 1);
  Task* task = luax_getthreaddata(T);

  if (T == L) {
    lua_pushliteral(L, "running");
  } else if (!task) {
    lua_pushnil(L);
  } else if (task->complete) {
    lua_pushliteral(L, "complete");
  } else if (task->error) {
    lua_pushliteral(L, "failed");
  } else if (task->waiting && atomic_load(&task->deps) > 0) {
    lua_pushliteral(L, "waiting");
  } else {
    lua_pushliteral(L, "ready");
  }

  return 1;
}

extern const luaL_Reg lovrTask[];

static const luaL_Reg lovrTaskModule[] = {
  { "start", l_lovrTaskStart },
  { "resume", l_lovrTaskResume },
  { "wait", l_lovrTaskWait },
  { "getStatus", l_lovrTaskGetStatus },
  { NULL, NULL }
};

int luaopen_lovr_task(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrTaskModule);

  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, "_lovrtasks");

  lua_pushcfunction(L, l_lovrTaskNext);
  lua_pushcclosure(L, l_lovrTaskPoll, 1);
  lua_setfield(L, -2, "poll");

  lovrTaskModuleInit();
  luax_atexit(L, lovrTaskModuleDestroy);
  return 1;
}
