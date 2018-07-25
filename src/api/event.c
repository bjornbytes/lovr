#include "api.h"
#include "event/event.h"

const char* EventTypes[] = {
  [EVENT_QUIT] = "quit",
  [EVENT_FOCUS] = "focus",
  [EVENT_MOUNT] = "mount",
  [EVENT_THREAD_ERROR] = "threaderror",
  [EVENT_CONTROLLER_ADDED] = "controlleradded",
  [EVENT_CONTROLLER_REMOVED] = "controllerremoved",
  [EVENT_CONTROLLER_PRESSED] = "controllerpressed",
  [EVENT_CONTROLLER_RELEASED] = "controllerreleased",
};

static _Thread_local int pollRef;

static int nextEvent(lua_State* L) {
  Event event;

  if (!lovrEventPoll(&event)) {
    return 0;
  }

  lua_pushstring(L, EventTypes[event.type]);

  switch (event.type) {
    case EVENT_QUIT:
      if (event.data.quit.restart) {
        lua_pushstring(L, "restart");
      } else {
        lua_pushnumber(L, event.data.quit.exitCode);
      }
      return 2;

    case EVENT_FOCUS:
      lua_pushboolean(L, event.data.focus.focused);
      return 2;

    case EVENT_MOUNT:
      lua_pushboolean(L, event.data.mount.mounted);
      return 2;

#ifndef EMSCRIPTEN
    case EVENT_THREAD_ERROR:
      luax_pushobject(L, event.data.threaderror.thread);
      lua_pushstring(L, event.data.threaderror.error);
      free((void*) event.data.threaderror.error);
      return 3;
#endif

    case EVENT_CONTROLLER_ADDED:
      luax_pushobject(L, event.data.controlleradded.controller);
      lovrRelease(event.data.controlleradded.controller);
      return 2;

    case EVENT_CONTROLLER_REMOVED:
      luax_pushobject(L, event.data.controllerremoved.controller);
      lovrRelease(event.data.controlleradded.controller);
      return 2;

    case EVENT_CONTROLLER_PRESSED:
      luax_pushobject(L, event.data.controllerpressed.controller);
      lua_pushstring(L, ControllerButtons[event.data.controllerpressed.button]);
      return 3;

    case EVENT_CONTROLLER_RELEASED:
      luax_pushobject(L, event.data.controllerreleased.controller);
      lua_pushstring(L, ControllerButtons[event.data.controllerpressed.button]);
      return 3;

    default:
      return 1;
  }
}

int l_lovrEventInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrEvent);

  // Store nextEvent in the registry to avoid creating a closure every time we poll for events.
  lua_pushcfunction(L, nextEvent);
  pollRef = luaL_ref(L, LUA_REGISTRYINDEX);

  lovrEventInit();
  return 1;
}

int l_lovrEventClear(lua_State* L) {
  lovrEventClear();
  return 0;
}

int l_lovrEventPoll(lua_State* L) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, pollRef);
  return 1;
}

int l_lovrEventPump(lua_State* L) {
  lovrEventPump();
  return 0;
}

int l_lovrEventPush(lua_State* L) {
  EventType type = luaL_checkoption(L, 1, NULL, EventTypes);
  EventData data;

  switch (type) {
    case EVENT_QUIT:
      if (lua_type(L, 2) == LUA_TSTRING) {
        data.quit.restart = lua_toboolean(L, 2);
      } else {
        data.quit.exitCode = luaL_optint(L, 2, 0);
      }
      break;

    case EVENT_FOCUS:
      data.focus.focused = lua_toboolean(L, 2);
      break;

    case EVENT_MOUNT:
      data.mount.mounted = lua_toboolean(L, 2);
      break;

#ifdef EMSCRIPTEN
    case EVENT_THREAD_ERROR:
      break;
#else
    case EVENT_THREAD_ERROR:
      data.threaderror.thread = luax_checktype(L, 2, Thread);
      data.threaderror.error = luaL_checkstring(L, 3);
      break;
#endif

    case EVENT_CONTROLLER_ADDED:
      data.controlleradded.controller = luax_checktype(L, 2, Controller);
      break;

    case EVENT_CONTROLLER_REMOVED:
      data.controllerremoved.controller = luax_checktype(L, 2, Controller);
      break;

    case EVENT_CONTROLLER_PRESSED:
      data.controllerpressed.controller = luax_checktype(L, 2, Controller);
      data.controllerpressed.button = luaL_checkoption(L, 3, NULL, ControllerButtons);
      break;

    case EVENT_CONTROLLER_RELEASED:
      data.controllerreleased.controller = luax_checktype(L, 2, Controller);
      data.controllerreleased.button = luaL_checkoption(L, 3, NULL, ControllerButtons);
      break;
  }

  Event event = { .type = type, .data = data };
  lovrEventPush(event);
  return 0;
}

int l_lovrEventQuit(lua_State* L) {
  EventData data;

  int argType = lua_type(L, 1);
  if (argType == LUA_TSTRING && 0 == strcmp("restart", lua_tostring(L, 1))) {
    data.quit.restart = true;
    data.quit.exitCode = 0;
  } else if (argType == LUA_TNUMBER || lua_isnoneornil(L, 1)) {
    data.quit.restart = false;
    data.quit.exitCode = luaL_optint(L, 1, 0);
  } else {
    return luaL_argerror (L, 1, "number, nil or the exact string 'restart' expected");
  }

  EventType type = EVENT_QUIT;
  Event event = { .type = type, .data = data };
  lovrEventPush(event);
  return 0;
}

const luaL_Reg lovrEvent[] = {
  { "clear", l_lovrEventClear },
  { "poll", l_lovrEventPoll },
  { "pump", l_lovrEventPump },
  { "push", l_lovrEventPush },
  { "quit", l_lovrEventQuit },
  { NULL, NULL }
};
