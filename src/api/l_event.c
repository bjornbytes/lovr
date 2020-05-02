#include "api.h"
#include "event/event.h"
#include "thread/thread.h"
#include "core/os.h"
#include "core/ref.h"
#include "core/util.h"
#include <stdlib.h>
#include <string.h>

StringEntry EventTypes[] = {
  [EVENT_QUIT] = ENTRY("quit"),
  [EVENT_RESTART] = ENTRY("restart"),
  [EVENT_FOCUS] = ENTRY("focus"),
  [EVENT_RESIZE] = ENTRY("resize"),
#ifdef LOVR_ENABLE_THREAD
  [EVENT_THREAD_ERROR] = ENTRY("threaderror"),
#endif
  { 0 }
};

static LOVR_THREAD_LOCAL int pollRef;

void luax_checkvariant(lua_State* L, int index, Variant* variant) {
  int type = lua_type(L, index);
  switch (type) {
    case LUA_TNIL:
    case LUA_TNONE:
      variant->type = TYPE_NIL;
      break;

    case LUA_TBOOLEAN:
      variant->type = TYPE_BOOLEAN;
      variant->value.boolean = lua_toboolean(L, index);
      break;

    case LUA_TNUMBER:
      variant->type = TYPE_NUMBER;
      variant->value.number = lua_tonumber(L, index);
      break;

    case LUA_TSTRING:
      variant->type = TYPE_STRING;
      size_t length;
      const char* string = lua_tolstring(L, index, &length);
      variant->value.string = malloc(length + 1);
      lovrAssert(variant->value.string, "Out of memory");
      memcpy(variant->value.string, string, length);
      variant->value.string[length] = '\0';
      break;

    case LUA_TUSERDATA:
      variant->type = TYPE_OBJECT;
      Proxy* proxy = lua_touserdata(L, index);
      lua_getmetatable(L, index);

      lua_pushliteral(L, "__name");
      lua_rawget(L, -2);
      variant->value.object.type = (const char*) lua_touserdata(L, -1);
      lua_pop(L, 1);

      lua_pushliteral(L, "__destructor");
      lua_rawget(L, -2);
      variant->value.object.destructor = (void (*)(void*)) lua_tocfunction(L, -1);
      lua_pop(L, 1);

      variant->value.object.pointer = proxy->object;
      lovrRetain(proxy->object);
      lua_pop(L, 1);
      break;

    default:
      lovrThrow("Bad variant type for argument %d: %s", index, lua_typename(L, type));
      return;
  }
}

int luax_pushvariant(lua_State* L, Variant* variant) {
  switch (variant->type) {
    case TYPE_NIL: lua_pushnil(L); return 1;
    case TYPE_BOOLEAN: lua_pushboolean(L, variant->value.boolean); return 1;
    case TYPE_NUMBER: lua_pushnumber(L, variant->value.number); return 1;
    case TYPE_STRING: lua_pushstring(L, variant->value.string); return 1;
    case TYPE_OBJECT: _luax_pushtype(L, variant->value.object.type, hash64(variant->value.object.type, strlen(variant->value.object.type)), variant->value.object.pointer); return 1;
    default: return 0;
  }
}

static int nextEvent(lua_State* L) {
  Event event;

  if (!lovrEventPoll(&event)) {
    return 0;
  }

  if (event.type == EVENT_CUSTOM) {
    lua_pushstring(L, event.data.custom.name);
  } else {
    luax_pushenum(L, EventTypes, event.type);
  }

  switch (event.type) {
    case EVENT_QUIT:
      lua_pushnumber(L, event.data.quit.exitCode);
      return 2;

    case EVENT_FOCUS:
      lua_pushboolean(L, event.data.boolean.value);
      return 2;

    case EVENT_RESIZE:
      lua_pushinteger(L, event.data.resize.width);
      lua_pushinteger(L, event.data.resize.height);
      return 3;

#ifdef LOVR_ENABLE_THREAD
    case EVENT_THREAD_ERROR:
      luax_pushtype(L, Thread, event.data.thread.thread);
      lua_pushstring(L, event.data.thread.error);
      lovrRelease(Thread, event.data.thread.thread);
      return 3;
#endif

    case EVENT_CUSTOM:
      for (uint32_t i = 0; i < event.data.custom.count; i++) {
        Variant* variant = &event.data.custom.data[i];
        luax_pushvariant(L, variant);
        lovrVariantDestroy(variant);
      }
      return event.data.custom.count + 1;

    default:
      return 1;
  }
}

static void hotkeyHandler(KeyCode key, ButtonAction action) {
  if (action != BUTTON_PRESSED) {
    return;
  }

  if (key == KEY_ESCAPE) {
    lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit.exitCode = 0 });
  } else if (key == KEY_F5) {
    lovrEventPush((Event) { .type = EVENT_RESTART });
  }
}

static int l_lovrEventClear(lua_State* L) {
  lovrEventClear();
  return 0;
}

static int l_lovrEventPoll(lua_State* L) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, pollRef);
  return 1;
}

static int l_lovrEventPump(lua_State* L) {
  lovrEventPump();
  return 0;
}

static int l_lovrEventPush(lua_State* L) {
  CustomEvent eventData;
  const char* name = luaL_checkstring(L, 1);
  strncpy(eventData.name, name, MAX_EVENT_NAME_LENGTH - 1);
  eventData.count = MIN(lua_gettop(L) - 1, 4);
  for (uint32_t i = 0; i < eventData.count; i++) {
    luax_checkvariant(L, 2 + i, &eventData.data[i]);
  }

  lovrEventPush((Event) { .type = EVENT_CUSTOM, .data.custom = eventData });
  return 0;
}

static int l_lovrEventQuit(lua_State* L) {
  EventData data;

  data.quit.exitCode = luaL_optint(L, 1, 0);

  EventType type = EVENT_QUIT;
  Event event = { .type = type, .data = data };
  lovrEventPush(event);
  return 0;
}

static int l_lovrEventRestart(lua_State* L) {
  EventType type = EVENT_RESTART;
  Event event = { .type = type };
  lovrEventPush(event);
  return 0;
}

static const luaL_Reg lovrEvent[] = {
  { "clear", l_lovrEventClear },
  { "poll", l_lovrEventPoll },
  { "pump", l_lovrEventPump },
  { "push", l_lovrEventPush },
  { "quit", l_lovrEventQuit },
  { "restart", l_lovrEventRestart },
  { NULL, NULL }
};

int luaopen_lovr_event(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrEvent);

  // Store nextEvent in the registry to avoid creating a closure every time we poll for events.
  lua_pushcfunction(L, nextEvent);
  pollRef = luaL_ref(L, LUA_REGISTRYINDEX);

  if (lovrEventInit()) {
    luax_atexit(L, lovrEventDestroy);
  }

  luax_pushconf(L);
  lua_getfield(L, -1, "hotkeys");
  if (lua_toboolean(L, -1)) {
    lovrPlatformOnKeyboardEvent(hotkeyHandler);
  }
  lua_pop(L, 2);
  return 1;
}
