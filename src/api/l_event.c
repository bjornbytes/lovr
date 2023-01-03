#include "api.h"
#include "event/event.h"
#include "thread/thread.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

StringEntry lovrEventType[] = {
  [EVENT_QUIT] = ENTRY("quit"),
  [EVENT_RESTART] = ENTRY("restart"),
  [EVENT_FOCUS] = ENTRY("focus"),
  [EVENT_RESIZE] = ENTRY("resize"),
  [EVENT_KEYPRESSED] = ENTRY("keypressed"),
  [EVENT_KEYRELEASED] = ENTRY("keyreleased"),
  [EVENT_TEXTINPUT] = ENTRY("textinput"),
#ifndef LOVR_DISABLE_THREAD
  [EVENT_THREAD_ERROR] = ENTRY("threaderror"),
#endif
  [EVENT_PERMISSION] = ENTRY("permission"),
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

    case LUA_TSTRING: {
      size_t length;
      const char* string = lua_tolstring(L, index, &length);
      if (length <= sizeof(variant->value.ministring.data)) {
        variant->type = TYPE_MINISTRING;
        variant->value.ministring.length = (uint8_t) length;
        memcpy(variant->value.ministring.data, string, length);
      } else {
        variant->type = TYPE_STRING;
        variant->value.string.pointer = malloc(length + 1);
        lovrAssert(variant->value.string.pointer, "Out of memory");
        memcpy(variant->value.string.pointer, string, length);
        variant->value.string.pointer[length] = '\0';
        variant->value.string.length = length;
      }
      break;
    }

    case LUA_TUSERDATA:
      variant->type = TYPE_OBJECT;
      Proxy* proxy = lua_touserdata(L, index);
      lua_getmetatable(L, index);

      lua_pushliteral(L, "__info");
      lua_rawget(L, -2);
      TypeInfo* info = lua_touserdata(L, -1);
      variant->value.object.type = info->name;
      variant->value.object.destructor = info->destructor;
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
    case TYPE_STRING: lua_pushlstring(L, variant->value.string.pointer, variant->value.string.length); return 1;
    case TYPE_MINISTRING: lua_pushlstring(L, variant->value.ministring.data, variant->value.ministring.length); return 1;
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
    luax_pushenum(L, EventType, event.type);
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

    case EVENT_KEYPRESSED:
      luax_pushenum(L, KeyboardKey, event.data.key.code);
      lua_pushinteger(L, event.data.key.scancode);
      lua_pushboolean(L, event.data.key.repeat);
      return 4;

    case EVENT_KEYRELEASED:
      luax_pushenum(L, KeyboardKey, event.data.key.code);
      lua_pushinteger(L, event.data.key.scancode);
      return 3;

    case EVENT_TEXTINPUT:
      lua_pushlstring(L, event.data.text.utf8, strnlen(event.data.text.utf8, 4));
      lua_pushinteger(L, event.data.text.codepoint);
      return 3;

#ifndef LOVR_DISABLE_THREAD
    case EVENT_THREAD_ERROR:
      luax_pushtype(L, Thread, event.data.thread.thread);
      lua_pushstring(L, event.data.thread.error);
      lovrRelease(event.data.thread.thread, lovrThreadDestroy);
      free(event.data.thread.error);
      return 3;
#endif

    case EVENT_PERMISSION:
      luax_pushenum(L, Permission, event.data.permission.permission);
      lua_pushboolean(L, event.data.permission.granted);
      return 3;

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
  int exitCode = luaL_optinteger(L, 1, 0);
  Event event = { .type = EVENT_QUIT, .data.quit.exitCode = exitCode };
  lovrEventPush(event);
  return 0;
}

static int l_lovrEventRestart(lua_State* L) {
  Event event = { .type = EVENT_RESTART };
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
  luax_register(L, lovrEvent);

  // Store nextEvent in the registry to avoid creating a closure every time we poll for events.
  lua_pushcfunction(L, nextEvent);
  pollRef = luaL_ref(L, LUA_REGISTRYINDEX);

  if (lovrEventInit()) {
    luax_atexit(L, lovrEventDestroy);
  }

  return 1;
}
