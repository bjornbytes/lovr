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

void luax_checkvariant(lua_State* L, int index, Variant* variant) {
  int type = lua_type(L, index);
  switch (type) {
    case LUA_TNIL:
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
      strcpy(variant->value.string, string);
      break;

    case LUA_TUSERDATA:
      variant->type = TYPE_OBJECT;
      variant->value.ref = lua_touserdata(L, index);
      lovrRetain(variant->value.ref);
      break;

    default:
      lovrThrow("Bad type for Channel:push: %s", lua_typename(L, type));
      return;
  }
}

int luax_pushvariant(lua_State* L, Variant* variant) {
  switch (variant->type) {
    case TYPE_NIL: lua_pushnil(L); return 1;
    case TYPE_BOOLEAN: lua_pushboolean(L, variant->value.boolean); return 1;
    case TYPE_NUMBER: lua_pushnumber(L, variant->value.number); return 1;
    case TYPE_STRING: lua_pushstring(L, variant->value.string); free(variant->value.string); return 1;
    case TYPE_OBJECT: luax_pushobject(L, variant->value.ref); lovrRelease(variant->value.ref); return 1;
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
    lua_pushstring(L, EventTypes[event.type]);
  }

  switch (event.type) {
    case EVENT_QUIT:
      if (event.data.quit.restart) {
        lua_pushstring(L, "restart");
      } else {
        lua_pushnumber(L, event.data.quit.exitCode);
      }
      return 2;

    case EVENT_FOCUS:
    case EVENT_MOUNT:
      lua_pushboolean(L, event.data.boolean.value);
      return 2;

    case EVENT_THREAD_ERROR:
      luax_pushobject(L, event.data.thread.thread);
      lua_pushstring(L, event.data.thread.error);
      free((void*) event.data.thread.error);
      return 3;

    case EVENT_CONTROLLER_ADDED:
    case EVENT_CONTROLLER_REMOVED:
      luax_pushobject(L, event.data.controller.controller);
      lovrRelease(event.data.controller.controller);
      return 2;

    case EVENT_CONTROLLER_PRESSED:
    case EVENT_CONTROLLER_RELEASED:
      luax_pushobject(L, event.data.controller.controller);
      lua_pushstring(L, ControllerButtons[event.data.controller.button]);
      return 3;

    case EVENT_CUSTOM:
      for (int i = 0; i < event.data.custom.count; i++) {
        luax_pushvariant(L, &event.data.custom.data[i]);
      }
      return event.data.custom.count + 1;

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
  CustomEvent eventData;
  const char* name = luaL_checkstring(L, 1);
  strncpy(eventData.name, name, MAX_EVENT_NAME_LENGTH - 1);
  eventData.count = MIN(lua_gettop(L) - 1, 4);
  for (int i = 0; i < eventData.count; i++) {
    luax_checkvariant(L, 2 + i, &eventData.data[i]);
  }

  lovrEventPush((Event) { .type = EVENT_CUSTOM, .data.custom = eventData });
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
