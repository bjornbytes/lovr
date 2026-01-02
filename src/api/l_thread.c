#include "api.h"
#include "data/blob.h"
#include "thread/thread.h"
#include "core/os.h"
#include "util.h"
#include <lualib.h>
#include <threads.h>
#include <stdlib.h>
#include <string.h>

static char* threadBody(Thread* thread, Blob* body, Variant* arguments, uint32_t argumentCount) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  luax_preload(L);

  lua_pushcfunction(L, luax_getstack);
  int errhandler = lua_gettop(L);

  if (!luax_loadbufferx(L, body->data, body->size, body->name, NULL)) {
    for (uint32_t i = 0; i < argumentCount; i++) {
      luax_pushvariant(L, &arguments[i]);
    }

    if (!lua_pcall(L, argumentCount, 0, errhandler)) {
      luax_close(L);
      return NULL;
    }
  }

  // Error handling
  if (lua_type(L, -1) == LUA_TSTRING) {
    const char* message = lua_tostring(L, -1);
    char* error = lovrStrdup(message);
    luax_close(L);
    return error;
  }

  luax_close(L);
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
      if (!code) return luaL_error(L, "Could not read thread code from file '%s'", str);
      blob = lovrBlobCreate(code, length, str);
    }
  } else {
    lovrRetain(blob);
  }
  Thread* thread = lovrThreadCreate(threadBody, blob);
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

typedef struct RunContext {
  struct RunContext* next;
  arr_t(char) code;
  lua_CFunction function;
  uint32_t argumentCount;
  uint32_t resultCount;
  Variant* arguments;
  Variant* results;
  char* error;
} RunContext;

static thread_local RunContext* contextPool;
static thread_local lua_State* workerState;

static void onWorkerQuit(void) {
  if (workerState) {
    lua_close(workerState);
    workerState = NULL;
  }

  while (contextPool) {
    RunContext* context = contextPool;
    contextPool = context->next;
    arr_free(&context->code);
    lovrFree(context);
  }
}

static bool luax_runlua(void** arg) {
  RunContext* context = *arg;
  lua_State* L = workerState;

  if (!L) {
    L = luaL_newstate();
    luaL_openlibs(L);
    luax_preload(L);
    workerState = L;

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "_lovrchunks");
  }

  int base = lua_gettop(L);
  lua_pushcfunction(L, luax_getstack);

  if (context->function) {
    lua_pushcfunction(L, context->function);
  } else {
    lua_getfield(L, LUA_REGISTRYINDEX, "_lovrchunks");
    lua_pushlstring(L, context->code.data, context->code.length);
    lua_rawget(L, -2);

    if (lua_isfunction(L, -1)) {
      lua_remove(L, -2);
    } else {
      if (luax_loadbufferx(L, context->code.data, context->code.length, "", "b")) {
        for (uint32_t i = 0; i < context->argumentCount; i++) {
          lovrVariantDestroy(&context->arguments[i]);
        }
        lovrSetError(lua_tostring(L, -1));
        lua_settop(L, base);
        return false;
      }

      lua_replace(L, -2);
      lua_pushlstring(L, context->code.data, context->code.length);
      lua_pushvalue(L, -2);
      lua_rawset(L, -4);
      lua_remove(L, -2);
    }
  }

  for (uint32_t i = 0; i < context->argumentCount; i++) {
    luax_pushvariant(L, &context->arguments[i]);
    lovrVariantDestroy(&context->arguments[i]);
  }

  if (lua_pcall(L, context->argumentCount, LUA_MULTRET, base + 1) != LUA_OK) {
    lovrSetError(lua_tostring(L, -1));
    lua_settop(L, base);
    return false;
  }

  int n = lua_gettop(L) - base - 1;

  if (n > 0) {
    context->resultCount = n;
    context->results = lovrRealloc(context->arguments, n * sizeof(Variant));
    context->argumentCount = 0;
    context->arguments = NULL;
    for (int i = 0; i < n; i++) {
      luax_checkvariant(L, base + 2 + i, &context->results[i]);
    }
  }

  lua_settop(L, base);

  return true;
}

static int luax_pushresults(lua_State* L, void* arg) {
  RunContext* context = arg;

  if (context->error) {
    lua_pushstring(L, context->error);
    lovrFree(context->error);
    lovrFree(context->arguments);
    context->next = contextPool;
    contextPool = context;
    return lua_error(L);
  }

  int n = context->resultCount;

  for (int i = 0; i < n; i++) {
    luax_pushvariant(L, &context->results[i]);
    lovrVariantDestroy(&context->results[i]);
  }

  lovrFree(context->arguments);
  lovrFree(context->results);
  context->next = contextPool;
  contextPool = context;
  return n;
}

static int writer(lua_State* L, const void* data, size_t size, void* userdata) {
  RunContext* context = userdata;
  arr_append(&context->code, data, size);
  return 0;
}

int luax_callthread(lua_State* L, int n) {
  RunContext* context = contextPool;

  if (context) {
    contextPool = context->next;
    arr_clear(&context->code);
    context->function = NULL;
    context->argumentCount = 0;
    context->resultCount = 0;
    context->error = NULL;
  } else {
    context = lovrCalloc(sizeof(RunContext));
    arr_init(&context->code);
  }

  int function = lua_gettop(L) - n;

  if (lua_iscfunction(L, function)) {
    context->function = lua_tocfunction(L, function);
  } else {
    luaL_checktype(L, function, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "_lovrbytecode");
    lua_pushvalue(L, function);
    lua_rawget(L, -2);

    if (lua_isnil(L, -1)) {
      lua_pushvalue(L, function);
      luax_check(L, !lua_dump(L, writer, context), "Failed to dump function to bytecode");
      lua_pushlstring(L, context->code.data, context->code.length);
      lua_rawset(L, -4);
      lua_pop(L, 2);
    } else {
      size_t length;
      const char* code = lua_tolstring(L, -1, &length);
      arr_append(&context->code, code, length);
      lua_pop(L, 2);
    }
  }

  if (n > 0) {
    context->argumentCount = n;
    context->arguments = lovrMalloc(n * sizeof(Variant));
    for (int i = 0; i < n; i++) {
      luax_checkvariant(L, i + 2, &context->arguments[i]);
    }
  }

  return luax_runasync(L, luax_runlua, luax_pushresults, context);
}

static int l_lovrThreadRun(lua_State* L) {
  return luax_callthread(L, lua_gettop(L) - 1);
}

static const luaL_Reg lovrThreadModule[] = {
  { "newThread", l_lovrThreadNewThread },
  { "newChannel", l_lovrThreadNewChannel },
  { "getChannel", l_lovrThreadGetChannel },
  { "run", l_lovrThreadRun },
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

  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, "_lovrbytecode");

  lovrThreadModuleInit(workers, onWorkerQuit);
  luax_atexit(L, lovrThreadModuleDestroy);
  return 1;
}
