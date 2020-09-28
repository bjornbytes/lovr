#include "api.h"
#include "event/event.h"
#include "thread/thread.h"
#include "core/os.h"
#include "core/ref.h"
#include "core/util.h"
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
#ifdef LOVR_ENABLE_THREAD
  [EVENT_THREAD_ERROR] = ENTRY("threaderror"),
#endif
  { 0 }
};

StringEntry lovrKeyCode[] = {
  [KEY_A] = ENTRY("a"),
  [KEY_B] = ENTRY("b"),
  [KEY_C] = ENTRY("c"),
  [KEY_D] = ENTRY("d"),
  [KEY_E] = ENTRY("e"),
  [KEY_F] = ENTRY("f"),
  [KEY_G] = ENTRY("g"),
  [KEY_H] = ENTRY("h"),
  [KEY_I] = ENTRY("i"),
  [KEY_J] = ENTRY("j"),
  [KEY_K] = ENTRY("k"),
  [KEY_L] = ENTRY("l"),
  [KEY_M] = ENTRY("m"),
  [KEY_N] = ENTRY("n"),
  [KEY_O] = ENTRY("o"),
  [KEY_P] = ENTRY("p"),
  [KEY_Q] = ENTRY("q"),
  [KEY_R] = ENTRY("r"),
  [KEY_S] = ENTRY("s"),
  [KEY_T] = ENTRY("t"),
  [KEY_U] = ENTRY("u"),
  [KEY_V] = ENTRY("v"),
  [KEY_W] = ENTRY("w"),
  [KEY_X] = ENTRY("x"),
  [KEY_Y] = ENTRY("y"),
  [KEY_Z] = ENTRY("z"),
  [KEY_0] = ENTRY("0"),
  [KEY_1] = ENTRY("1"),
  [KEY_2] = ENTRY("2"),
  [KEY_3] = ENTRY("3"),
  [KEY_4] = ENTRY("4"),
  [KEY_5] = ENTRY("5"),
  [KEY_6] = ENTRY("6"),
  [KEY_7] = ENTRY("7"),
  [KEY_8] = ENTRY("8"),
  [KEY_9] = ENTRY("9"),
  [KEY_SPACE] = ENTRY("space"),
  [KEY_ENTER] = ENTRY("return"),
  [KEY_TAB] = ENTRY("tab"),
  [KEY_ESCAPE] = ENTRY("escape"),
  [KEY_BACKSPACE] = ENTRY("backspace"),
  [KEY_UP] = ENTRY("up"),
  [KEY_DOWN] = ENTRY("down"),
  [KEY_LEFT] = ENTRY("left"),
  [KEY_RIGHT] = ENTRY("right"),
  [KEY_HOME] = ENTRY("home"),
  [KEY_END] = ENTRY("end"),
  [KEY_PAGE_UP] = ENTRY("pageup"),
  [KEY_PAGE_DOWN] = ENTRY("pagedown"),
  [KEY_INSERT] = ENTRY("insert"),
  [KEY_DELETE] = ENTRY("delete"),
  [KEY_F1] = ENTRY("f1"),
  [KEY_F2] = ENTRY("f2"),
  [KEY_F3] = ENTRY("f3"),
  [KEY_F4] = ENTRY("f4"),
  [KEY_F5] = ENTRY("f5"),
  [KEY_F6] = ENTRY("f6"),
  [KEY_F7] = ENTRY("f7"),
  [KEY_F8] = ENTRY("f8"),
  [KEY_F9] = ENTRY("f9"),
  [KEY_F10] = ENTRY("f10"),
  [KEY_F11] = ENTRY("f11"),
  [KEY_F12] = ENTRY("f12"),
  [KEY_BACKTICK] = ENTRY("`"),
  [KEY_MINUS] = ENTRY("-"),
  [KEY_EQUALS] = ENTRY("="),
  [KEY_LEFT_BRACKET] = ENTRY("["),
  [KEY_RIGHT_BRACKET] = ENTRY("]"),
  [KEY_BACKSLASH] = ENTRY("\\"),
  [KEY_SEMICOLON] = ENTRY(";"),
  [KEY_APOSTROPHE] = ENTRY("'"),
  [KEY_COMMA] = ENTRY(","),
  [KEY_PERIOD] = ENTRY("."),
  [KEY_SLASH] = ENTRY("/"),
  [KEY_LEFT_CONTROL] = ENTRY("lctrl"),
  [KEY_LEFT_SHIFT] = ENTRY("lshift"),
  [KEY_LEFT_ALT] = ENTRY("lalt"),
  [KEY_LEFT_OS] = ENTRY("lgui"),
  [KEY_RIGHT_CONTROL] = ENTRY("rctrl"),
  [KEY_RIGHT_SHIFT] = ENTRY("rshift"),
  [KEY_RIGHT_ALT] = ENTRY("ralt"),
  [KEY_RIGHT_OS] = ENTRY("rgui"),
  [KEY_CAPS_LOCK] = ENTRY("capslock"),
  [KEY_SCROLL_LOCK] = ENTRY("scrolllock"),
  [KEY_NUM_LOCK] = ENTRY("numlock"),
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
      luax_pushenum(L, KeyCode, event.data.key.code);
      lua_pushinteger(L, event.data.key.scancode);
      lua_pushboolean(L, event.data.key.repeat);
      return 4;

    case EVENT_KEYRELEASED:
      luax_pushenum(L, KeyCode, event.data.key.code);
      lua_pushinteger(L, event.data.key.scancode);
      return 3;

    case EVENT_TEXTINPUT:
      lua_pushlstring(L, event.data.text.utf8, strnlen(event.data.text.utf8, 4));
      lua_pushinteger(L, event.data.text.codepoint);
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

  data.quit.exitCode = luaL_optinteger(L, 1, 0);

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
  luax_register(L, lovrEvent);

  // Store nextEvent in the registry to avoid creating a closure every time we poll for events.
  lua_pushcfunction(L, nextEvent);
  pollRef = luaL_ref(L, LUA_REGISTRYINDEX);

  if (lovrEventInit()) {
    luax_atexit(L, lovrEventDestroy);
  }

  return 1;
}
