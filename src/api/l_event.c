#include "api.h"
#include "event/event.h"
#include "thread/thread.h"
#include "util.h"
#include <threads.h>
#include <stdlib.h>
#include <string.h>

StringEntry lovrEventType[] = {
  [EVENT_QUIT] = ENTRY("quit"),
  [EVENT_RESTART] = ENTRY("restart"),
  [EVENT_VISIBLE] = ENTRY("visible"),
  [EVENT_FOCUS] = ENTRY("focus"),
  [EVENT_RECENTER] = ENTRY("recenter"),
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

static thread_local int pollRef;

static void _luax_checkvariant(lua_State* L, int index, Variant* variant, int depth) {
  lovrAssert(depth <= 128, "depth > 128, please avoid circular references.");
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
        variant->value.string.pointer = lovrMalloc(length + 1);
        memcpy(variant->value.string.pointer, string, length);
        variant->value.string.pointer[length] = '\0';
        variant->value.string.length = length;
      }
      break;
    }

    case LUA_TTABLE:
      if (index < 0) { index += lua_gettop(L) + 1; }
      size_t length = 0;
      lua_pushnil(L);
      while (lua_next(L, index) != 0) { length++; lua_pop(L, 1); }
      variant->type = TYPE_TABLE;
      variant->value.table.length = length;
      Variant* keys;
      Variant* vals;

      if (length > 0) {
        keys = lovrMalloc(length * sizeof(Variant));
        vals = lovrMalloc(length * sizeof(Variant));
        int i = 0;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
          _luax_checkvariant(L, -1, &vals[i], depth + 1);
          lua_pop(L, 1);
          _luax_checkvariant(L, -1, &keys[i], depth + 1);
          i++;
        }
      } else {
        keys = NULL;
        vals = NULL;
      }
      variant->value.table.keys = keys;
      variant->value.table.vals = vals;
      break;

    case LUA_TUSERDATA:
      variant->type = TYPE_OBJECT;
      Proxy* proxy = lua_touserdata(L, index);
      lua_getmetatable(L, index);

      lua_pushliteral(L, "__info");
      lua_rawget(L, -2);
      if (!lua_isnil(L, -1)) {
        TypeInfo* info = lua_touserdata(L, -1);
        variant->value.object.type = info->name;
        variant->value.object.destructor = info->destructor;
        lua_pop(L, 1);

        variant->value.object.pointer = proxy->object;
        lovrRetain(proxy->object);
        lua_pop(L, 1);
        break;
      } else {
        lua_pop(L, 2);
      }
      /* fallthrough */

    case LUA_TLIGHTUSERDATA: {
      VectorType type;
      float* v = luax_tovector(L, index, &type);
      if (v) {
        if (type == V_MAT4) {
          variant->type = TYPE_MATRIX;
          variant->value.matrix.data = lovrMalloc(16 * sizeof(float));
          memcpy(variant->value.matrix.data, v, 16 * sizeof(float));
          break;
        } else {
          variant->type = TYPE_VECTOR;
          variant->value.vector.type = type;
          memcpy(variant->value.vector.data, v, (type == V_VEC2 ? 2 : 4) * sizeof(float));
          break;
        }
      } else if (lua_type(L, index) == LUA_TLIGHTUSERDATA) {
        variant->type = TYPE_POINTER;
        variant->value.pointer = lua_touserdata(L, index);
        break;
      }
      lovrThrow("Bad userdata variant for argument %d (expected object, vector, or lightuserdata)", index);
    }

    default:
      lovrThrow("Bad variant type for argument %d: %s", index, lua_typename(L, type));
      return;
  }
}

void luax_checkvariant(lua_State* L, int index, Variant* variant) {
  _luax_checkvariant(L, index, variant, 0);
}

int luax_pushvariant(lua_State* L, Variant* variant) {
  switch (variant->type) {
    case TYPE_NIL: lua_pushnil(L); return 1;
    case TYPE_BOOLEAN: lua_pushboolean(L, variant->value.boolean); return 1;
    case TYPE_NUMBER: lua_pushnumber(L, variant->value.number); return 1;
    case TYPE_STRING: lua_pushlstring(L, variant->value.string.pointer, variant->value.string.length); return 1;
    case TYPE_TABLE:
      lua_newtable(L);
      Variant* keys = variant->value.table.keys;
      Variant* vals = variant->value.table.vals;
      for (int i = 0; i < variant->value.table.length; i++) {
        luax_pushvariant(L, &keys[i]);
        luax_pushvariant(L, &vals[i]);
        lua_settable(L, -3);
      }
      return 1;
    case TYPE_MINISTRING: lua_pushlstring(L, variant->value.ministring.data, variant->value.ministring.length); return 1;
    case TYPE_POINTER: lua_pushlightuserdata(L, variant->value.pointer); return 1;
    case TYPE_OBJECT: _luax_pushtype(L, variant->value.object.type, hash64(variant->value.object.type, strlen(variant->value.object.type)), variant->value.object.pointer); return 1;
    case TYPE_VECTOR: memcpy(luax_newtempvector(L, variant->value.vector.type), variant->value.vector.data, (variant->value.vector.type == V_VEC2 ? 2 : 4) * sizeof(float)); return 1;
    case TYPE_MATRIX: memcpy(luax_newtempvector(L, V_MAT4), variant->value.vector.data, 16 * sizeof(float)); return 1;
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

    case EVENT_VISIBLE:
      lua_pushboolean(L, event.data.boolean.value);
      return 2;

    case EVENT_FOCUS:
      lua_pushboolean(L, event.data.boolean.value);
      return 2;

    case EVENT_RECENTER:
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
      free(event.data.file.path);
      free(event.data.file.oldpath);
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
  lua_rawgeti(L, LUA_REGISTRYINDEX, pollRef);
  return 1;
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

  lovrEventInit();
  luax_atexit(L, lovrEventDestroy);
  return 1;
}
