#include "system/system.h"
#include "event/event.h"
#include "core/os.h"
#include "core/util.h"
#include <string.h>

static struct {
  bool initialized;
  int windowWidth;
  int windowHeight;
} state;

static void onKeyboardEvent(os_button_action action, os_key key, uint32_t scancode, bool repeat) {
  lovrEventPush((Event) {
    .type = action == BUTTON_PRESSED ? EVENT_KEYPRESSED : EVENT_KEYRELEASED,
    .data.key.code = key,
    .data.key.scancode = scancode,
    .data.key.repeat = repeat
  });
}

static void onTextEvent(uint32_t codepoint) {
  Event event;
  event.type = EVENT_TEXTINPUT;
  event.data.text.codepoint = codepoint;
  memset(&event.data.text.utf8, 0, sizeof(event.data.text.utf8));
  utf8_encode(codepoint, event.data.text.utf8);
  lovrEventPush(event);
}

static void onPermissionEvent(os_permission permission, bool granted) {
  Event event;
  event.type = EVENT_PERMISSION;
  event.data.permission.permission = permission;
  event.data.permission.granted = granted;
  lovrEventPush(event);
}

static void onQuit() {
  lovrEventPush((Event) { .type = EVENT_QUIT, .data.quit.exitCode = 0 });
}

static void onResize(int width, int height) {
  state.windowWidth = width;
  state.windowHeight = height;
}

bool lovrSystemInit() {
  if (state.initialized) return false;
  os_on_key(onKeyboardEvent);
  os_on_text(onTextEvent);
  os_on_permission(onPermissionEvent);
  state.initialized = true;
  return true;
}

void lovrSystemDestroy() {
  if (!state.initialized) return;
  os_on_key(NULL);
  os_on_text(NULL);
  os_on_permission(NULL);
  memset(&state, 0, sizeof(state));
}

const char* lovrSystemGetOS() {
  return os_get_name();
}

uint32_t lovrSystemGetCoreCount() {
  return os_get_core_count();
}

void lovrSystemRequestPermission(Permission permission) {
  os_request_permission((os_permission) permission);
}

void lovrSystemOpenWindow(os_window_config* window) {
  lovrAssert(os_window_open(window), "Could not open window");
  os_on_resize(onResize);
  os_on_quit(onQuit);
  os_window_get_fbsize(&state.windowWidth, &state.windowHeight);
}

bool lovrSystemIsWindowOpen() {
  return os_window_is_open();
}

uint32_t lovrSystemGetWindowWidth() {
  return state.windowWidth;
}

uint32_t lovrSystemGetWindowHeight() {
  return state.windowHeight;
}

float lovrSystemGetWindowDensity() {
  int width, height, fbwidth, fbheight;
  os_window_get_size(&width, &height);
  os_window_get_fbsize(&fbwidth, &fbheight);
  return (width == 0 || fbwidth == 0) ? 0.f : (float) fbwidth / width;
}
