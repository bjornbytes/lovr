#include "api.h"
#include "system/system.h"
#include "data/image.h"
#include "core/os.h"
#include "util.h"
#include <string.h>

StringEntry lovrKeyboardKey[] = {
  [OS_KEY_A] = ENTRY("a"),
  [OS_KEY_B] = ENTRY("b"),
  [OS_KEY_C] = ENTRY("c"),
  [OS_KEY_D] = ENTRY("d"),
  [OS_KEY_E] = ENTRY("e"),
  [OS_KEY_F] = ENTRY("f"),
  [OS_KEY_G] = ENTRY("g"),
  [OS_KEY_H] = ENTRY("h"),
  [OS_KEY_I] = ENTRY("i"),
  [OS_KEY_J] = ENTRY("j"),
  [OS_KEY_K] = ENTRY("k"),
  [OS_KEY_L] = ENTRY("l"),
  [OS_KEY_M] = ENTRY("m"),
  [OS_KEY_N] = ENTRY("n"),
  [OS_KEY_O] = ENTRY("o"),
  [OS_KEY_P] = ENTRY("p"),
  [OS_KEY_Q] = ENTRY("q"),
  [OS_KEY_R] = ENTRY("r"),
  [OS_KEY_S] = ENTRY("s"),
  [OS_KEY_T] = ENTRY("t"),
  [OS_KEY_U] = ENTRY("u"),
  [OS_KEY_V] = ENTRY("v"),
  [OS_KEY_W] = ENTRY("w"),
  [OS_KEY_X] = ENTRY("x"),
  [OS_KEY_Y] = ENTRY("y"),
  [OS_KEY_Z] = ENTRY("z"),
  [OS_KEY_0] = ENTRY("0"),
  [OS_KEY_1] = ENTRY("1"),
  [OS_KEY_2] = ENTRY("2"),
  [OS_KEY_3] = ENTRY("3"),
  [OS_KEY_4] = ENTRY("4"),
  [OS_KEY_5] = ENTRY("5"),
  [OS_KEY_6] = ENTRY("6"),
  [OS_KEY_7] = ENTRY("7"),
  [OS_KEY_8] = ENTRY("8"),
  [OS_KEY_9] = ENTRY("9"),
  [OS_KEY_SPACE] = ENTRY("space"),
  [OS_KEY_ENTER] = ENTRY("return"),
  [OS_KEY_TAB] = ENTRY("tab"),
  [OS_KEY_ESCAPE] = ENTRY("escape"),
  [OS_KEY_BACKSPACE] = ENTRY("backspace"),
  [OS_KEY_UP] = ENTRY("up"),
  [OS_KEY_DOWN] = ENTRY("down"),
  [OS_KEY_LEFT] = ENTRY("left"),
  [OS_KEY_RIGHT] = ENTRY("right"),
  [OS_KEY_HOME] = ENTRY("home"),
  [OS_KEY_END] = ENTRY("end"),
  [OS_KEY_PAGE_UP] = ENTRY("pageup"),
  [OS_KEY_PAGE_DOWN] = ENTRY("pagedown"),
  [OS_KEY_INSERT] = ENTRY("insert"),
  [OS_KEY_DELETE] = ENTRY("delete"),
  [OS_KEY_F1] = ENTRY("f1"),
  [OS_KEY_F2] = ENTRY("f2"),
  [OS_KEY_F3] = ENTRY("f3"),
  [OS_KEY_F4] = ENTRY("f4"),
  [OS_KEY_F5] = ENTRY("f5"),
  [OS_KEY_F6] = ENTRY("f6"),
  [OS_KEY_F7] = ENTRY("f7"),
  [OS_KEY_F8] = ENTRY("f8"),
  [OS_KEY_F9] = ENTRY("f9"),
  [OS_KEY_F10] = ENTRY("f10"),
  [OS_KEY_F11] = ENTRY("f11"),
  [OS_KEY_F12] = ENTRY("f12"),
  [OS_KEY_BACKTICK] = ENTRY("`"),
  [OS_KEY_MINUS] = ENTRY("-"),
  [OS_KEY_EQUALS] = ENTRY("="),
  [OS_KEY_LEFT_BRACKET] = ENTRY("["),
  [OS_KEY_RIGHT_BRACKET] = ENTRY("]"),
  [OS_KEY_BACKSLASH] = ENTRY("\\"),
  [OS_KEY_SEMICOLON] = ENTRY(";"),
  [OS_KEY_APOSTROPHE] = ENTRY("'"),
  [OS_KEY_COMMA] = ENTRY(","),
  [OS_KEY_PERIOD] = ENTRY("."),
  [OS_KEY_SLASH] = ENTRY("/"),
  [OS_KEY_KP_0] = ENTRY("kp0"),
  [OS_KEY_KP_1] = ENTRY("kp1"),
  [OS_KEY_KP_2] = ENTRY("kp2"),
  [OS_KEY_KP_3] = ENTRY("kp3"),
  [OS_KEY_KP_4] = ENTRY("kp4"),
  [OS_KEY_KP_5] = ENTRY("kp5"),
  [OS_KEY_KP_6] = ENTRY("kp6"),
  [OS_KEY_KP_7] = ENTRY("kp7"),
  [OS_KEY_KP_8] = ENTRY("kp8"),
  [OS_KEY_KP_9] = ENTRY("kp9"),
  [OS_KEY_KP_DECIMAL] = ENTRY("kp."),
  [OS_KEY_KP_DIVIDE] = ENTRY("kp/"),
  [OS_KEY_KP_MULTIPLY] = ENTRY("kp*"),
  [OS_KEY_KP_SUBTRACT] = ENTRY("kp-"),
  [OS_KEY_KP_ADD] = ENTRY("kp+"),
  [OS_KEY_KP_ENTER] = ENTRY("kpenter"),
  [OS_KEY_KP_EQUALS] = ENTRY("kp="),
  [OS_KEY_LEFT_CONTROL] = ENTRY("lctrl"),
  [OS_KEY_LEFT_SHIFT] = ENTRY("lshift"),
  [OS_KEY_LEFT_ALT] = ENTRY("lalt"),
  [OS_KEY_LEFT_OS] = ENTRY("lgui"),
  [OS_KEY_RIGHT_CONTROL] = ENTRY("rctrl"),
  [OS_KEY_RIGHT_SHIFT] = ENTRY("rshift"),
  [OS_KEY_RIGHT_ALT] = ENTRY("ralt"),
  [OS_KEY_RIGHT_OS] = ENTRY("rgui"),
  [OS_KEY_CAPS_LOCK] = ENTRY("capslock"),
  [OS_KEY_SCROLL_LOCK] = ENTRY("scrolllock"),
  [OS_KEY_NUM_LOCK] = ENTRY("numlock"),
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

static int l_lovrSystemOpenConsole(lua_State* L) {
  lovrSystemOpenConsole();
  return 0;
}

static int l_lovrSystemRequestPermission(lua_State* L) {
  Permission permission = luax_checkenum(L, 1, Permission, NULL);
  lovrSystemRequestPermission(permission);
  return 0;
}

static int l_lovrSystemOpenWindow(lua_State* L) {
  os_window_config window;
  memset(&window, 0, sizeof(window));

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "width");
  window.width = luaL_optinteger(L, -1, 720);
  lua_pop(L, 1);

  lua_getfield(L, 1, "height");
  window.height = luaL_optinteger(L, -1, 800);
  lua_pop(L, 1);

  lua_getfield(L, 1, "fullscreen");
  window.fullscreen = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "resizable");
  window.resizable = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "title");
  window.title = luaL_optstring(L, -1, "LÖVR");
  lua_pop(L, 1);

  lua_getfield(L, 1, "icon");
  Image* image = NULL;
  if (!lua_isnil(L, -1)) {
    image = luax_checkimage(L, -1);
    window.icon.data = lovrImageGetLayerData(image, 0, 0);
    window.icon.width = lovrImageGetWidth(image, 0);
    window.icon.height = lovrImageGetHeight(image, 0);
  }
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
  uint32_t width, height;
  lovrSystemGetWindowSize(&width, &height);
  lua_pushnumber(L, width);
  return 1;
}

static int l_lovrSystemGetWindowHeight(lua_State* L) {
  uint32_t width, height;
  lovrSystemGetWindowSize(&width, &height);
  lua_pushnumber(L, height);
  return 1;
}

static int l_lovrSystemGetWindowDimensions(lua_State* L) {
  uint32_t width, height;
  lovrSystemGetWindowSize(&width, &height);
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}

static int l_lovrSystemGetWindowDensity(lua_State* L) {
  lua_pushnumber(L, lovrSystemGetWindowDensity());
  return 1;
}

static int l_lovrSystemPollEvents(lua_State* L) {
  lovrSystemPollEvents();
  return 0;
}

static int l_lovrSystemIsKeyDown(lua_State* L) {
  int count = lua_gettop(L);
  for (int i = 0; i < count; i++) {
    os_key key = luax_checkenum(L, i + 1, KeyboardKey, NULL);
    if (lovrSystemIsKeyDown(key)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrSystemWasKeyPressed(lua_State* L) {
  int count = lua_gettop(L);
  for (int i = 0; i < count; i++) {
    os_key key = luax_checkenum(L, i + 1, KeyboardKey, NULL);
    if (lovrSystemWasKeyPressed(key)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrSystemWasKeyReleased(lua_State* L) {
  int count = lua_gettop(L);
  for (int i = 0; i < count; i++) {
    os_key key = luax_checkenum(L, i + 1, KeyboardKey, NULL);
    if (lovrSystemWasKeyReleased(key)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int l_lovrSystemHasKeyRepeat(lua_State* L) {
  lua_pushboolean(L, lovrSystemHasKeyRepeat());
  return 1;
}

static int l_lovrSystemSetKeyRepeat(lua_State* L) {
  bool repeat = lua_toboolean(L, 1);
  lovrSystemSetKeyRepeat(repeat);
  return 0;
}

static int l_lovrSystemGetMouseX(lua_State* L) {
  double x, y;
  lovrSystemGetMousePosition(&x, &y);
  lua_pushnumber(L, x);
  return 1;
}

static int l_lovrSystemGetMouseY(lua_State* L) {
  double x, y;
  lovrSystemGetMousePosition(&x, &y);
  lua_pushnumber(L, y);
  return 1;
}

static int l_lovrSystemGetMousePosition(lua_State* L) {
  double x, y;
  lovrSystemGetMousePosition(&x, &y);
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  return 2;
}

static int l_lovrSystemIsMouseDown(lua_State* L) {
  int button = luaL_checkint(L, 1) - 1;
  bool down = lovrSystemIsMouseDown(button);
  lua_pushboolean(L, down);
  return 1;
}

static int l_lovrSystemWasMousePressed(lua_State* L) {
  int button = luaL_checkint(L, 1) - 1;
  bool pressed = lovrSystemWasMousePressed(button);
  lua_pushboolean(L, pressed);
  return 1;
}

static int l_lovrSystemWasMouseReleased(lua_State* L) {
  int button = luaL_checkint(L, 1) - 1;
  bool released = lovrSystemWasMouseReleased(button);
  lua_pushboolean(L, released);
  return 1;
}

static int l_lovrSystemGetClipboardText(lua_State* L) {
  const char* text = lovrSystemGetClipboardText();
  lua_pushstring(L, text);
  return 1;
}

static int l_lovrSystemSetClipboardText(lua_State* L) {
  const char* text = luaL_checkstring(L, 1);
  lovrSystemSetClipboardText(text);
  return 0;
}

static const luaL_Reg lovrSystem[] = {
  { "getOS", l_lovrSystemGetOS },
  { "getCoreCount", l_lovrSystemGetCoreCount },
  { "openConsole", l_lovrSystemOpenConsole },
  { "requestPermission", l_lovrSystemRequestPermission },
  { "openWindow", l_lovrSystemOpenWindow },
  { "isWindowOpen", l_lovrSystemIsWindowOpen },
  { "getWindowWidth", l_lovrSystemGetWindowWidth },
  { "getWindowHeight", l_lovrSystemGetWindowHeight },
  { "getWindowDimensions", l_lovrSystemGetWindowDimensions },
  { "getWindowDensity", l_lovrSystemGetWindowDensity },
  { "pollEvents", l_lovrSystemPollEvents },
  { "isKeyDown", l_lovrSystemIsKeyDown },
  { "wasKeyPressed", l_lovrSystemWasKeyPressed },
  { "wasKeyReleased", l_lovrSystemWasKeyReleased },
  { "hasKeyRepeat", l_lovrSystemHasKeyRepeat },
  { "setKeyRepeat", l_lovrSystemSetKeyRepeat },
  { "getMouseX", l_lovrSystemGetMouseX },
  { "getMouseY", l_lovrSystemGetMouseY },
  { "getMousePosition", l_lovrSystemGetMousePosition },
  { "isMouseDown", l_lovrSystemIsMouseDown },
  { "wasMousePressed", l_lovrSystemWasMousePressed },
  { "wasMouseReleased", l_lovrSystemWasMouseReleased },
  { "getClipboardText", l_lovrSystemGetClipboardText },
  { "setClipboardText", l_lovrSystemSetClipboardText },
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
