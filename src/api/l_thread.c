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

typedef struct CallContext {
  struct CallContext* next;
  arr_t(char) code;
  lua_CFunction function;
  Variant table;
  uint32_t count;
  uint32_t stride;
  atomic_uint cursor;
  uint32_t argumentCount;
  uint32_t resultCount;
  Variant* arguments;
  Variant* results;
} CallContext;

static thread_local CallContext* contextPool;
static thread_local lua_State* workerState;

static void onWorkerQuit(void) {
  if (workerState) {
    lua_close(workerState);
    workerState = NULL;
  }

  while (contextPool) {
    CallContext* context = contextPool;
    contextPool = context->next;
    arr_free(&context->code);
    lovrFree(context);
  }
}

static bool luax_callthread(void** arg) {
  CallContext* context = *arg;
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
  }

  if (context->count > 0) {
    uint32_t start = atomic_fetch_add(&context->cursor, context->stride);
    uint32_t end = MIN(start + context->stride, context->count);
    bool hasTable = context->table.type == TYPE_TABLE;
    int argumentCount = 1 + hasTable + context->argumentCount;

    for (uint32_t i = start; i < end; i++) {
      lua_pushvalue(L, base + 2); // Function
      lua_pushinteger(L, i + 1);

      // When iterating over a table, pass a value from the table as the 2nd callback argument
      if (hasTable) {
        luax_pushvariant(L, &context->table.table.pairs[2 * i + 1]);
      }

      // Push copies of the arguments
      for (int j = 0; j < context->argumentCount; j++) {
        lua_pushvalue(L, base + 3 + j);
      }

      if (lua_pcall(L, argumentCount, context->results ? 1 : 0, base + 1) != LUA_OK) {
        lovrSetError(lua_tostring(L, -1));
        lua_settop(L, base);
        return false;
      }

      if (context->results) {
        luax_checkvariant(L, -1, &context->results[i]);
        lua_pop(L, 1);
      }
    }
  } else {
    if (lua_pcall(L, context->argumentCount, LUA_MULTRET, base + 1) != LUA_OK) {
      lovrSetError(lua_tostring(L, -1));
      lua_settop(L, base);
      return false;
    }

    int n = lua_gettop(L) - base - 1;

    if (n > 0) {
      context->resultCount = n;
      context->results = lovrMalloc(n * sizeof(Variant));
      for (int i = 0; i < n; i++) {
        luax_checkvariant(L, base + 2 + i, &context->results[i]);
      }
    }
  }

  lua_settop(L, base);
  return true;
}

static int luax_pushcallresults(lua_State* L, bool success, void* arg) {
  CallContext* context = arg;
  context->next = contextPool;
  contextPool = context;

  for (uint32_t i = 0; i < context->argumentCount; i++) {
    lovrVariantDestroy(&context->arguments[i]);
  }

  int n = 0;

  if (success && context->results) {
    // Map wraps results in a table, call pushes them as-is
    if (context->count > 0) {
      lua_createtable(L, context->count, 0);
      for (int i = 0; i < context->count; i++) {
        luax_pushvariant(L, &context->results[i]);
        lovrVariantDestroy(&context->results[i]);
        lua_rawseti(L, -2, i + 1);
      }
      n = 1;
    } else {
      for (uint32_t i = 0; i < context->resultCount; i++) {
        luax_pushvariant(L, &context->results[i]);
        lovrVariantDestroy(&context->results[i]);
      }
      n = context->resultCount;
    }
  }

  lovrFree(context->arguments);
  lovrFree(context->results);

  if (context->table.type == TYPE_TABLE) {
    lovrVariantDestroy(&context->table);
  }

  return n;
}

static int luax_dump(lua_State* L, const void* data, size_t size, void* userdata) {
  CallContext* context = userdata;
  arr_append(&context->code, data, size);
  return 0;
}

static CallContext* luax_newcontext(lua_State* L, int index) {
  CallContext* context = contextPool;

  if (context) {
    contextPool = context->next;
    arr_clear(&context->code);
    context->function = NULL;
    context->count = 0;
    context->table.type = TYPE_NIL;
    atomic_store(&context->cursor, 0);
    context->argumentCount = 0;
    context->resultCount = 0;
    context->arguments = NULL;
    context->results = NULL;
  } else {
    context = lovrCalloc(sizeof(CallContext));
    arr_init(&context->code);
  }

  if (lua_iscfunction(L, index)) {
    context->function = lua_tocfunction(L, index);
  } else {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "_lovrbytecode");
    lua_pushvalue(L, index);
    lua_rawget(L, -2);

    if (lua_isnil(L, -1)) {
      lua_pushvalue(L, index);
      luax_check(L, !lua_dump(L, luax_dump, context), "Failed to dump function to bytecode");
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

  return context;
}

static int l_lovrThreadCall(lua_State* L) {
  CallContext* context = luax_newcontext(L, 1);

  int n = lua_gettop(L) - 1;

  if (n > 0) {
    context->argumentCount = n;
    context->arguments = lovrMalloc(n * sizeof(Variant));
    for (int i = 0; i < n; i++) {
      luax_checkvariant(L, 2 + i, &context->arguments[i]);
    }
  }

  return luax_yieldjob(L, luax_callthread, luax_pushcallresults, context, 1);
}

static int l_lovrThreadForeach(lua_State* L) {
  CallContext* context = luax_newcontext(L, 2);
  uint32_t workers = lovrThreadGetWorkerCount();

  if (lua_istable(L, 1)) {
    luax_checkvariant(L, 1, &context->table);
    context->count = context->table.table.count;
  } else {
    context->count = luax_checku32(L, 1);
  }

  int n = lua_gettop(L) - 2;

  if (n > 0) {
    context->argumentCount = n;
    context->arguments = lovrMalloc(n * sizeof(Variant));
    for (uint32_t i = 0; i < n; i++) {
      luax_checkvariant(L, 3 + i, &context->arguments[i]);
    }
  }

  context->stride = (context->count + workers - 1) / workers;
  uint32_t jobs = MIN(context->count, workers);

  return luax_yieldjob(L, luax_callthread, luax_pushcallresults, context, jobs);
}

static int l_lovrThreadMap(lua_State* L) {
  CallContext* context = luax_newcontext(L, 2);
  uint32_t workers = lovrThreadGetWorkerCount();

  if (lua_istable(L, 1)) {
    luax_checkvariant(L, 1, &context->table);
    context->count = context->table.table.count;
  } else {
    context->count = luax_checku32(L, 1);
  }

  int n = lua_gettop(L) - 2;

  if (n > 0) {
    context->argumentCount = n;
    context->arguments = lovrMalloc(n * sizeof(Variant));
    for (uint32_t i = 0; i < n; i++) {
      luax_checkvariant(L, 3 + i, &context->arguments[i]);
    }
  }

  context->resultCount = context->count;
  context->results = lovrMalloc(context->count * sizeof(Variant));

  context->stride = (context->count + workers - 1) / workers;
  uint32_t jobs = MIN(context->count, workers);

  return luax_yieldjob(L, luax_callthread, luax_pushcallresults, context, jobs);
}

static const luaL_Reg lovrThreadModule[] = {
  { "newThread", l_lovrThreadNewThread },
  { "newChannel", l_lovrThreadNewChannel },
  { "getChannel", l_lovrThreadGetChannel },
  { "call", l_lovrThreadCall },
  { "foreach", l_lovrThreadForeach },
  { "map", l_lovrThreadMap },
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
