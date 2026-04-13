#include "api.h"
#include "event/event.h"
#include "thread/thread.h"
#include "util.h"
#include "core/threads.h"
#include <stdlib.h>
#include <string.h>

StringEntry lovrDisplayType[] = {
  [DISPLAY_HEADSET] = ENTRY("headset"),
  [DISPLAY_WINDOW] = ENTRY("window"),
  { 0 }
};

StringEntry lovrEventType[] = {
  [EVENT_QUIT] = ENTRY("quit"),
  [EVENT_RESTART] = ENTRY("restart"),
  [EVENT_VISIBLE] = ENTRY("visible"),
  [EVENT_FOCUS] = ENTRY("focus"),
  [EVENT_MOUNT] = ENTRY("mount"),
  [EVENT_RECENTER] = ENTRY("recenter"),
  [EVENT_MODELSCHANGED] = ENTRY("modelschanged"),
  [EVENT_RESIZE] = ENTRY("resize"),
  [EVENT_KEYPRESSED] = ENTRY("keypressed"),
  [EVENT_KEYRELEASED] = ENTRY("keyreleased"),
  [EVENT_TEXTINPUT] = ENTRY("textinput"),
  [EVENT_MOUSEPRESSED] = ENTRY("mousepressed"),
  [EVENT_MOUSERELEASED] = ENTRY("mousereleased"),
  [EVENT_MOUSEMOVED] = ENTRY("mousemoved"),
  [EVENT_MOUSEWHEELMOVED] = ENTRY("wheelmoved"),
#ifndef LOVR_DISABLE_THREAD
  [EVENT_THREAD_ERROR] = ENTRY("threaderror"),
#endif
  [EVENT_FILECHANGED] = ENTRY("filechanged"),
  [EVENT_PERMISSION] = ENTRY("permission"),
  { 0 }
};

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

    case EVENT_VISIBLE:
      lua_pushboolean(L, event.data.visible.visible);
      luax_pushenum(L, DisplayType, event.data.visible.display);
      return 3;

    case EVENT_FOCUS:
      lua_pushboolean(L, event.data.focus.focused);
      luax_pushenum(L, DisplayType, event.data.focus.display);
      return 3;

    case EVENT_MOUNT:
      lua_pushboolean(L, event.data.mount.mounted);
      return 2;

    case EVENT_RECENTER:
      return 1;

    case EVENT_MODELSCHANGED:
      return 1;

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

    case EVENT_MOUSEPRESSED:
    case EVENT_MOUSERELEASED:
      lua_pushnumber(L, event.data.mouse.x);
      lua_pushnumber(L, event.data.mouse.y);
      lua_pushinteger(L, event.data.mouse.button + 1);
      return 4;

    case EVENT_MOUSEMOVED:
      lua_pushnumber(L, event.data.mouse.x);
      lua_pushnumber(L, event.data.mouse.y);
      lua_pushnumber(L, event.data.mouse.dx);
      lua_pushnumber(L, event.data.mouse.dy);
      return 5;

    case EVENT_MOUSEWHEELMOVED:
      lua_pushnumber(L, event.data.wheel.x);
      lua_pushnumber(L, event.data.wheel.y);
      return 3;

#ifndef LOVR_DISABLE_THREAD
    case EVENT_THREAD_ERROR:
      luax_pushtype(L, Thread, event.data.thread.thread);
      lua_pushstring(L, event.data.thread.error);
      lovrRelease(event.data.thread.thread, lovrThreadDestroy);
      lovrFree(event.data.thread.error);
      return 3;
#endif

    case EVENT_FILECHANGED:
      lua_pushstring(L, event.data.file.path);
      luax_pushenum(L, FileAction, event.data.file.action);
      lua_pushstring(L, event.data.file.oldpath);
      lovrFree(event.data.file.path);
      lovrFree(event.data.file.oldpath);
      return 4;

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
  lua_pushvalue(L, lua_upvalueindex(1));
  return 1;
}

static int l_lovrEventPush(lua_State* L) {
  CustomEvent event;

  size_t length;
  const char* name = luaL_checklstring(L, 1, &length);
  luax_check(L, length < sizeof(event.name), "Custom event name is too long");
  memcpy(event.name, name, length + 1);

  event.count = lua_gettop(L) - 1;
  event.data = lovrMalloc(event.count * sizeof(Variant));
  for (uint32_t i = 0; i < event.count; i++) {
    luax_checkvariant(L, 2 + i, &event.data[i]);
  }

  lovrEventPush((Event) { .type = EVENT_CUSTOM, .data.custom = event });
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
  { "push", l_lovrEventPush },
  { "quit", l_lovrEventQuit },
  { "restart", l_lovrEventRestart },
  { NULL, NULL }
};

int luaopen_lovr_event(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrEvent);

  lua_pushcfunction(L, nextEvent);
  lua_pushcclosure(L, l_lovrEventPoll, 1);
  lua_setfield(L, -2, "poll");

  luax_assert(L, lovrEventInit());
  luax_atexit(L, lovrEventDestroy);
  return 1;
}
