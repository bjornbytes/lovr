#include "api.h"
#include "task/task.h"
#include "core/job.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

int luax_runtask(Task* task, int n) {
  lua_State* T = task->T;

  if (task->waiting) {
    // Untrack task: unpin from registry, release its held reference
    luax_pushtype(T, Task, task);
    lua_pushnil(T);
    lua_rawset(T, LUA_REGISTRYINDEX);

    // Remove it from the ready queue if it's there
    lovrTaskDequeue(task);

    // Handle error: can't actually throw an error in T without Lua 5.2 continuations, fake it instead
    if (task->error) {
      task->waiting = WAIT_NONE;
      lua_settop(T, 0);
      lua_pushstring(T, task->error);
      lovrTaskFinish(task);
      return LUA_ERRRUN;
    }

    // Set up resume values
    if (task->waiting == WAIT_JOB) {
      n = task->continuation(T, task->context);
    } else {
      n = 0;

      // Copy the first result from each dependency, replacing each coroutine with its result
      int top = lua_gettop(T);
      for (int i = 1; i < top; i++) {
        Task* dep = luax_totype(T, i, Task);
        lua_State* D = dep->T;
        lua_pushvalue(D, 1);
        lua_xmove(D, T, 1);
        lua_replace(T, i);
        n++;
      }

      // ...Except for the last dependency.  Copy ALL of its results instead
      lua_State* D = ((Task*) luax_totype(T, top, Task))->T;
      int rest = lua_gettop(D);
      luax_check(T, lua_checkstack(D, rest), "stack overflow");
      for (int i = 1; i <= rest; i++) {
        lua_pushvalue(D, i);
      }
      lua_pop(T, 1);
      lua_xmove(D, T, rest);
      n += rest;
    }

    task->waiting = WAIT_NONE;
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

static int l_lovrTaskResume(lua_State* L) {
  Task* task = luax_checktype(L, 1, Task);
  lua_State* T = task->T;

  if (task->complete) {
    lua_pushnil(L);
    lua_pushliteral(L, "already complete");
    return 2;
  } else if (!lovrTaskIsReady(task)) {
    lua_pushnil(L);
    lua_pushliteral(L, "not ready");
    return 2;
  }

  int n = 0;

  // If the task wasn't waiting on anything (it yielded with coroutine.yield), pass through the
  // remaining arguments to this function as the return values for coroutine.yield
  if (!task->waiting) {
    n = lua_gettop(L) - 1;
    lua_xmove(L, T, n);
  }

  int status = luax_runtask(task, n);

  // See what happened
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
    // If it yielded with coroutine.yield, return the results it yielded with
    if (!task->waiting) {
      int n = lua_gettop(T);
      luax_check(L, lua_checkstack(T, n), "stack overflow");
      for (int i = 1; i <= n; i++) {
        lua_pushvalue(T, i);
      }
      lua_xmove(T, L, n);
      return n + 1;
    } else {
      return 1;
    }
  } else {
    lua_pushboolean(L, false);
    lua_pushvalue(T, -1);
    lua_xmove(T, L, 1);
    return 2;
  }
}

static int l_lovrTaskIsReady(lua_State* L) {
  Task* task = luax_checktype(L, 1, Task);
  bool ready = lovrTaskIsReady(task);
  lua_pushboolean(L, ready);
  return 1;
}

static int l_lovrTaskIsComplete(lua_State* L) {
  Task* task = luax_checktype(L, 1, Task);
  lua_pushboolean(L, task->complete);
  return 1;
}

static int l_lovrTaskGetResults(lua_State* L) {
  Task* task = luax_checktype(L, 1, Task);

  if (!task->complete || task->error) {
    return 0;
  }

  lua_State* T = task->T;
  int n = lua_gettop(T);
  luax_check(L, lua_checkstack(T, n), "stack overflow");
  for (int i = 1; i <= n; i++) {
    lua_pushvalue(T, i);
  }
  lua_xmove(T, L, n);
  return n;
}

static int l_lovrTaskGetError(lua_State* L) {
  Task* task = luax_checktype(L, 1, Task);
  if (task->complete && task->error) {
    lua_pushstring(L, task->error);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int l_lovrTaskDestroy(lua_State* L) {
  Task* task = luax_totype(L, 1, Task);

  if (task) {
    lua_State* T = task->T;
    lua_getfield(T, LUA_REGISTRYINDEX, "_lovrtasks");
    lua_pushthread(T);
    lua_pushnil(T);
    lua_rawset(T, -3);
    lua_pop(T, 1);
  }

  return luax_release(L);
}

const luaL_Reg lovrTask[] = {
  { "resume", l_lovrTaskResume },
  { "isReady", l_lovrTaskIsReady },
  { "isComplete", l_lovrTaskIsComplete },
  { "getResults", l_lovrTaskGetResults },
  { "getError", l_lovrTaskGetError },
  { "__gc", l_lovrTaskDestroy },
  { NULL, NULL }
};
