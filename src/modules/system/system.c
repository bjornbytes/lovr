#include "system/system.h"
#include "event/event.h"
#include "core/os.h"
#include "util.h"
#include <string.h>

static lovr_atomic_uint ref;

static struct {
  bool keyRepeat;
  bool prevKeyState[OS_KEY_COUNT];
  bool keyState[OS_KEY_COUNT];
  bool prevMouseState[8];
  bool mouseState[8];
  double mouseX;
  double mouseY;
  double scrollDelta;
} state;

static void onKey(os_button_action action, os_key key, uint32_t scancode, bool repeat) {
  if (repeat && !state.keyRepeat) return;
  state.keyState[key] = (action == BUTTON_PRESSED);
  lovrEventPush((Event) {
    .type = action == BUTTON_PRESSED ? EVENT_KEYPRESSED : EVENT_KEYRELEASED,
    .data.key.code = key,
    .data.key.scancode = scancode,
    .data.key.repeat = repeat
  });
}

static void onText(uint32_t codepoint) {
  Event event;
  event.type = EVENT_TEXTINPUT;
  event.data.text.codepoint = codepoint;
  memset(&event.data.text.utf8, 0, sizeof(event.data.text.utf8));
  utf8_encode(codepoint, event.data.text.utf8);
  lovrEventPush(event);
}

static void onMouseButton(int button, bool pressed) {
  if ((size_t) button < COUNTOF(state.mouseState)) state.mouseState[button] = pressed;
  lovrEventPush((Event) {
    .type = pressed ? EVENT_MOUSEPRESSED : EVENT_MOUSERELEASED,
    .data.mouse.x = state.mouseX,
    .data.mouse.y = state.mouseY,
    .data.mouse.button = button
  });
}

static void onMouseMove(double x, double y) {
  lovrEventPush((Event) {
    .type = EVENT_MOUSEMOVED,
    .data.mouse.x = x,
    .data.mouse.y = y,
    .data.mouse.dx = x - state.mouseX,
    .data.mouse.dy = y - state.mouseY
  });

  state.mouseX = x;
  state.mouseY = y;
}

static void onWheelMove(double deltaX, double deltaY) {
  state.scrollDelta += deltaY;
  lovrEventPush((Event) {
    .type = EVENT_MOUSEWHEELMOVED,
    .data.wheel.x = deltaX,
    .data.wheel.y = deltaY,
  });
}

static void onPermission(os_permission permission, bool granted) {
  lovrEventPush((Event) {
    .type = EVENT_PERMISSION,
    .data.permission.permission = permission,
    .data.permission.granted = granted
  });
}

static void onQuit(void) {
  lovrEventPush((Event) {
    .type = EVENT_QUIT,
    .data.quit.exitCode = 0
  });
}

static void onVisible(bool visible) {
  lovrEventPush((Event) {
    .type = EVENT_VISIBLE,
    .data.visible.visible = visible,
    .data.visible.display = DISPLAY_WINDOW
  });
}

static void onFocus(bool focused) {
  lovrEventPush((Event) {
    .type = EVENT_FOCUS,
    .data.focus.focused = focused,
    .data.focus.display = DISPLAY_WINDOW
  });
}

bool lovrSystemInit(void) {
  if (!lovrModuleAcquire(&ref)) return true;
  os_on_key(onKey);
  os_on_text(onText);
  os_on_mouse_button(onMouseButton);
  os_on_mouse_move(onMouseMove);
  os_on_mousewheel_move(onWheelMove);
  os_on_permission(onPermission);
  os_get_mouse_position(&state.mouseX, &state.mouseY);
  lovrModuleReady(&ref);
  return true;
}

void lovrSystemDestroy(void) {
  if (!lovrModuleRelease(&ref)) return;
  os_on_key(NULL);
  os_on_text(NULL);
  os_on_permission(NULL);
  memset(&state, 0, sizeof(state));
  lovrModuleReset(&ref);
}

const char* lovrSystemGetOS(void) {
  return os_get_name();
}

void lovrSystemOpenConsole(void) {
  os_open_console();
}

uint32_t lovrSystemGetCoreCount(void) {
  return os_get_core_count();
}

void lovrSystemRequestPermission(Permission permission) {
  os_request_permission((os_permission) permission);
}

bool lovrSystemOpenWindow(os_window_config* window) {
  lovrAssert(os_window_open(window), "Could not open window");
  os_on_quit(onQuit);
  os_on_visible(onVisible);
  os_on_focus(onFocus);
  return true;
}

bool lovrSystemIsWindowOpen(void) {
  return os_window_is_open();
}

bool lovrSystemIsWindowVisible(void) {
  return os_window_is_visible();
}

bool lovrSystemIsWindowFocused(void) {
  return os_window_is_focused();
}

bool lovrSystemIsWindowFullscreen(void) {
  return os_window_is_fullscreen();
}

void lovrSystemSetWindowFullscreen(bool fullscreen) {
  os_window_set_fullscreen(fullscreen);
}

void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height) {
  os_window_get_size(width, height);
}

float lovrSystemGetWindowDensity(void) {
  return os_window_get_pixel_density();
}

void lovrSystemPollEvents(void) {
  memcpy(state.prevKeyState, state.keyState, sizeof(state.keyState));
  memcpy(state.prevMouseState, state.mouseState, sizeof(state.mouseState));
  state.scrollDelta = 0.;
  os_poll_events();
}

bool lovrSystemIsKeyDown(int keycode) {
  return state.keyState[keycode];
}

bool lovrSystemWasKeyPressed(int keycode) {
  return !state.prevKeyState[keycode] && state.keyState[keycode];
}

bool lovrSystemWasKeyReleased(int keycode) {
  return state.prevKeyState[keycode] && !state.keyState[keycode];
}

bool lovrSystemHasKeyRepeat(void) {
  return state.keyRepeat;
}

void lovrSystemSetKeyRepeat(bool repeat) {
  state.keyRepeat = repeat;
}

void lovrSystemGetMousePosition(double* x, double* y) {
  *x = state.mouseX;
  *y = state.mouseY;
}

bool lovrSystemIsMouseDown(int button) {
  if ((size_t) button > COUNTOF(state.mouseState)) return false;
  return state.mouseState[button];
}

bool lovrSystemWasMousePressed(int button) {
  if ((size_t) button > COUNTOF(state.mouseState)) return false;
  return !state.prevMouseState[button] && state.mouseState[button];
}

bool lovrSystemWasMouseReleased(int button) {
  if ((size_t) button > COUNTOF(state.mouseState)) return false;
  return state.prevMouseState[button] && !state.mouseState[button];
}

MouseMode lovrSystemGetMouseMode(void) {
  switch (os_get_mouse_mode()) {
    case OS_MOUSE_NORMAL: default: return MOUSE_NORMAL;
    case OS_MOUSE_RELATIVE: return MOUSE_RELATIVE;
  }
}

void lovrSystemSetMouseMode(MouseMode mode) {
  os_set_mouse_mode(mode == MOUSE_RELATIVE ? OS_MOUSE_RELATIVE : OS_MOUSE_NORMAL);

  if (mode == MOUSE_NORMAL) {
    os_get_mouse_position(&state.mouseX, &state.mouseY);
  }
}

// This is kind of a hacky thing for the simulator, since we're kinda bad at event dispatch
float lovrSystemGetScrollDelta(void) {
  return state.scrollDelta;
}

const char* lovrSystemGetClipboardText(void) {
  return os_get_clipboard_text();
}

void lovrSystemSetClipboardText(const char* text) {
  os_set_clipboard_text(text);
}
