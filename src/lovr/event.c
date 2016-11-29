#include "lovr/event.h"
#include "event/event.h"
#include "util.h"

static int nextEvent(lua_State* L) {
  Event* event = lovrEventPoll();

  if (!event) {
    return 0;
  }

  switch (event->type) {
    case EVENT_QUIT:
      lua_pushstring(L, "quit");
      lua_pushnumber(L, event->data.quit.exitCode);
      return 2;

    default:
      return 0;
  }
}

const luaL_Reg lovrEvent[] = {
  { "clear", l_lovrEventClear },
  { "poll", l_lovrEventPoll },
  { "pump", l_lovrEventPump },
  { "push", l_lovrEventPush },
  { "quit", l_lovrEventQuit },
  { NULL, NULL }
};

int l_lovrEventInit(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrEvent);
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
  lua_pushcclosure(L, nextEvent, 0);
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
  }

  Event event = { .type = type, .data = data };
  lovrEventPush(event);
  return 0;
}

int l_lovrEventQuit(lua_State* L) {
  int exitCode = luaL_optint(L, 1, 0);
  lua_settop(L, 0);
  lua_pushliteral(L, "quit");
  lua_pushinteger(L, exitCode);
  return l_lovrEventPush(L);
}
