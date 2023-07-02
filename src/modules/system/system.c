#include "system/system.h"
#include "event/event.h"
#include "core/os.h"
#include "util.h"
#include <string.h>

static struct {
  bool initialized;
  bool keyRepeat;
  bool prevKeyState[KEY_COUNT];
  bool keyState[KEY_COUNT];
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

bool lovrSystemInit(void) {
  if (state.initialized) return false;
  os_on_key(onKey);
  os_on_text(onText);
  os_on_mouse_button(onMouseButton);
  os_on_mouse_move(onMouseMove);
  os_on_mousewheel_move(onWheelMove);
  os_on_permission(onPermission);
  state.initialized = true;
  os_get_mouse_position(&state.mouseX, &state.mouseY);
  return true;
}

void lovrSystemDestroy(void) {
  if (!state.initialized) return;
  os_on_key(NULL);
  os_on_text(NULL);
  os_on_permission(NULL);
  memset(&state, 0, sizeof(state));
}

const char* lovrSystemGetOS(void) {
  return os_get_name();
}

uint32_t lovrSystemGetCoreCount(void) {
  return os_get_core_count();
}

void lovrSystemRequestPermission(Permission permission) {
  os_request_permission((os_permission) permission);
}

void lovrSystemOpenWindow(os_window_config* window) {
  lovrAssert(os_window_open(window), "Could not open window");
  os_on_quit(onQuit);
}

bool lovrSystemIsWindowOpen(void) {
  return os_window_is_open();
}

void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height) {
  os_window_get_size(width, height);
}

float lovrSystemGetWindowDensity(void) {
  return os_window_get_pixel_density();
}

void lovrSystemPollEvents(void) {
  memcpy(state.prevKeyState, state.keyState, sizeof(state.keyState));
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

// This is kind of a hacky thing for the simulator, since we're kinda bad at event dispatch
float lovrSystemGetScrollDelta(void) {
  return state.scrollDelta;
}
