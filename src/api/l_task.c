#include "api.h"
#include "task/task.h"
#include "core/job.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static Task* luax_gettask(lua_State* L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrtasks");
  if (lua_isnil(L, -1)) return lua_pop(L, 1), NULL;
  lua_pushthread(L);
  lua_rawget(L, -2);
  Task* task = lua_touserdata(L, -1);
  lua_pop(L, 2);
  return task;
}

static void taskRunner(void* arg) {
  Task* task = arg;

  if (!task->fn(&task->context)) {
    task->error = lovrStrdup(lovrGetError());
  }

  lovrTaskEnqueue(task);
  atomic_fetch_sub(&task->deps, 1);
}

int luax_runasync(lua_State* L, fn_task* fn, fn_continuation* continuation, void* context) {
  Task* task = luax_gettask(L);

  if (!task) {
    if (fn(&context)) {
      return continuation(L, context);
    } else {
      lua_pushstring(L, lovrGetError());
      return lua_error(L);
    }
  }

  task->fn = fn;
  task->context = context;
  task->continuation = continuation;
  atomic_store(&task->deps, 1);
  task->waiting = WAIT_JOB;

  if (!job_start(taskRunner, task)) {
    task->waiting = WAIT_NONE;
    atomic_store(&task->deps, 0);
    if (fn(&context)) {
      return continuation(L, context);
    } else {
      lua_pushstring(L, lovrGetError());
      return lua_error(L);
    }
  } else {
    luax_pushtype(L, Task, task);
    lua_pushvalue(L, -1);
    lua_rawset(L, LUA_REGISTRYINDEX);
    return lua_yield(L, 0);
  }
}

static int l_lovrTaskNewTask(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_settop(L, 1);
  lua_State* T = lua_newthread(L);
  lua_insert(L, 1);
  lua_xmove(L, T, 1);

  Task* task = lovrTaskCreate(T);
  luax_pushtype(L, Task, task);
  lovrRelease(task, lovrTaskDestroy);

  // _lovrtasks[T] = task, cleared on __gc
  lua_getfield(L, LUA_REGISTRYINDEX, "_lovrtasks");
  lua_pushvalue(L, 1);
  lua_pushlightuserdata(L, task);
  lua_rawset(L, -3);
  lua_pop(L, 1);

  return 1;
}

static int l_lovrTaskNext(lua_State* L) {
  Task* task = lovrTaskModulePoll();
  luax_pushtype(L, Task, task);
  return 1;
}

static int l_lovrTaskPoll(lua_State* L) {
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

int luax_runtask(Task* task, int n);

static int luax_waittask(Task* task) {
  lua_State* T = task->T;

  if (task->complete) {
    return task->error ? LUA_ERRRUN : LUA_OK;
  }

  if (task->waiting == WAIT_JOB) {
    while (atomic_load(&task->deps) > 0) {
      job_spin();
    }
  } else {
    int n = lua_gettop(T);
    for (int i = 1; i <= n; i++) {
      luax_waittask(luax_totype(T, i, Task));
    }
  }

  return luax_runtask(task, 0);
}

static int l_lovrTaskWait(lua_State* L) {
  Task* self = luax_gettask(L);

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
      Task* task = luax_checktype(L, i, Task);
      luax_assert(L, lovrTaskAddDependency(self, task));
    }

    // Only yield if we're actually waiting on something.  If everything was already complete, fall
    // through to the synchronous path, which handles errors and gathers results
    if (self->waiting == WAIT_TASK) {
      luax_pushtype(L, Task, self);
      lua_pushvalue(L, -1);
      lua_rawset(L, LUA_REGISTRYINDEX);
      return lua_yield(L, n);
    }
  }

  for (int i = 1; i <= n; i++) {
    Task* task = luax_checktype(L, i, Task);

    for (;;) {
      int status = luax_waittask(task);

      if (status == LUA_OK) {
        break;
      } else if (status != LUA_YIELD) {
        lua_pushboolean(L, false);
        lua_pushstring(L, task->error);
        return 2;
      }
    }
  }

  int results = 0;

  for (int i = 1; i <= n; i++) {
    Task* task = luax_checktype(L, i, Task);
    lua_State* T = task->T;

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

extern const luaL_Reg lovrTask[];

static const luaL_Reg lovrTaskModule[] = {
  { "newTask", l_lovrTaskNewTask },
  { "next", l_lovrTaskNext },
  { "wait", l_lovrTaskWait },
  { NULL, NULL }
};

int luaopen_lovr_task(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrTaskModule);
  luax_registertype(L, Task);

  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, "_lovrtasks");

  lua_pushcfunction(L, l_lovrTaskNext);
  lua_pushcclosure(L, l_lovrTaskPoll, 1);
  lua_setfield(L, -2, "poll");

  lovrTaskModuleInit();
  luax_atexit(L, lovrTaskModuleDestroy);
  return 1;
}
