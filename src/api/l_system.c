#include "api.h"
#include "system/system.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/os.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

StringEntry lovrKeyboardKey[] = {
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

StringEntry lovrPermission[] = {
  [PERMISSION_AUDIO_CAPTURE] = ENTRY("audiocapture"),
  { 0 }
};

static int l_lovrSystemGetOS(lua_State* L) {
  lua_pushstring(L, lovrSystemGetOS());
  return 1;
}

static int l_lovrSystemGetCoreCount(lua_State* L) {
  lua_pushinteger(L, lovrSystemGetCoreCount());
  return 1;
}

static int l_lovrSystemRequestPermission(lua_State* L) {
  Permission permission = luax_checkenum(L, 1, Permission, NULL);
  lovrSystemRequestPermission(permission);
  return 0;
}

static int l_lovrSystemOpenWindow(lua_State* L) {
  os_window_config window;
  memset(&window, 0, sizeof(window));

  if (!lua_toboolean(L, 1)) {
    return 0;
  }

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "width");
  window.width = luaL_optinteger(L, -1, 1080);
  lua_pop(L, 1);

  lua_getfield(L, 1, "height");
  window.height = luaL_optinteger(L, -1, 600);
  lua_pop(L, 1);

  lua_getfield(L, 1, "fullscreen");
  window.fullscreen = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "resizable");
  window.resizable = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "msaa");
  window.msaa = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "title");
  window.title = luaL_optstring(L, -1, "LÖVR");
  lua_pop(L, 1);

  lua_getfield(L, 1, "icon");
  Image* image = NULL;
  if (!lua_isnil(L, -1)) {
    image = luax_checkimage(L, -1, false);
    window.icon.data = image->blob->data;
    window.icon.width = image->width;
    window.icon.height = image->height;
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "vsync");
  window.vsync = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lovrSystemOpenWindow(&window);
  lovrRelease(image, lovrImageDestroy);
  return 0;
}

static int l_lovrSystemIsWindowOpen(lua_State* L) {
  bool open = lovrSystemIsWindowOpen();
  lua_pushboolean(L, open);
  return 1;
}

static int l_lovrSystemGetWindowWidth(lua_State* L) {
  lua_pushnumber(L, lovrSystemGetWindowWidth());
  return 1;
}

static int l_lovrSystemGetWindowHeight(lua_State* L) {
  lua_pushnumber(L, lovrSystemGetWindowHeight());
  return 1;
}

static int l_lovrSystemGetWindowDimensions(lua_State* L) {
  lua_pushnumber(L, lovrSystemGetWindowWidth());
  lua_pushnumber(L, lovrSystemGetWindowHeight());
  return 2;
}

static int l_lovrSystemGetWindowDensity(lua_State* L) {
  lua_pushnumber(L, lovrSystemGetWindowDensity());
  return 1;
}

static const luaL_Reg lovrSystem[] = {
  { "getOS", l_lovrSystemGetOS },
  { "getCoreCount", l_lovrSystemGetCoreCount },
  { "requestPermission", l_lovrSystemRequestPermission },
  { "openWindow", l_lovrSystemOpenWindow },
  { "isWindowOpen", l_lovrSystemIsWindowOpen },
  { "getWindowWidth", l_lovrSystemGetWindowWidth },
  { "getWindowHeight", l_lovrSystemGetWindowHeight },
  { "getWindowDimensions", l_lovrSystemGetWindowDimensions },
  { "getWindowDensity", l_lovrSystemGetWindowDensity },
  { NULL, NULL }
};

int luaopen_lovr_system(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrSystem);
  if (lovrSystemInit()) {
    luax_atexit(L, lovrSystemDestroy);
  }
  return 1;
}
