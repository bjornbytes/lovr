#include "api.h"
#include "event/event.h"

map_int_t EventTypes;

static int pollRef;

static int nextEvent(lua_State* L) {
  Event event;

  if (!lovrEventPoll(&event)) {
    return 0;
  }

  switch (event.type) {
    case EVENT_QUIT: {
      lua_pushstring(L, "quit");
      if (event.data.quit.restart)
        lua_pushliteral(L, "restart");
      else
        lua_pushnumber(L, event.data.quit.exitCode);
      return 2;
    }

    case EVENT_FOCUS: {
      lua_pushstring(L, "focus");
      lua_pushboolean(L, event.data.focus.isFocused);
      return 2;
    };

    case EVENT_CONTROLLER_ADDED: {
      lua_pushstring(L, "controlleradded");
      luax_pushtype(L, Controller, event.data.controlleradded.controller);
      lovrRelease(&event.data.controlleradded.controller->ref);
      return 2;
    }

    case EVENT_CONTROLLER_REMOVED: {
      lua_pushstring(L, "controllerremoved");
      luax_pushtype(L, Controller, event.data.controllerremoved.controller);
      lovrRelease(&event.data.controlleradded.controller->ref);
      return 2;
    }

    case EVENT_CONTROLLER_PRESSED: {
      lua_pushstring(L, "controllerpressed");
      luax_pushtype(L, Controller, event.data.controllerpressed.controller);
      luax_pushenum(L, &ControllerButtons, event.data.controllerpressed.button);
      return 3;
    }

    case EVENT_CONTROLLER_RELEASED: {
      lua_pushstring(L, "controllerreleased");
      luax_pushtype(L, Controller, event.data.controllerreleased.controller);
      luax_pushenum(L, &ControllerButtons, event.data.controllerreleased.button);
      return 3;
    }

    default:
      return 0;
  }
}

int l_lovrEventInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrEvent);

  // Store nextEvent in the registry to avoid creating a closure every time we poll for events.
  lua_pushcfunction(L, nextEvent);
  pollRef = luaL_ref(L, LUA_REGISTRYINDEX);

  map_init(&EventTypes);
  map_set(&EventTypes, "quit", EVENT_QUIT);
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
  EventType type = *(EventType*) luax_checkenum(L, 1, &EventTypes, "event type");
  EventData data;

  switch (type) {
    case EVENT_QUIT:
      data.quit.exitCode = luaL_optint(L, 2, 0);
      break;

    case EVENT_FOCUS:
      data.focus.isFocused = lua_toboolean(L, 2);
      break;

    case EVENT_CONTROLLER_ADDED:
      data.controlleradded.controller = luax_checktype(L, 2, Controller);
      break;

    case EVENT_CONTROLLER_REMOVED:
      data.controllerremoved.controller = luax_checktype(L, 2, Controller);
      break;

    case EVENT_CONTROLLER_PRESSED:
      data.controllerpressed.controller = luax_checktype(L, 2, Controller);
      data.controllerpressed.button = *(ControllerButton*) luax_checkenum(L, 3, &ControllerButtons, "button");
      break;

    case EVENT_CONTROLLER_RELEASED:
      data.controllerreleased.controller = luax_checktype(L, 2, Controller);
      data.controllerreleased.button = *(ControllerButton*) luax_checkenum(L, 3, &ControllerButtons, "button");
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
