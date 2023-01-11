#include "system/system.h"
#include "event/event.h"
#include "core/os.h"
#include "util.h"
#include <string.h>

static struct {
  bool initialized;
  bool pressedKeys[KEY_COUNT];
} state;

static void onKey(os_button_action action, os_key key, uint32_t scancode, bool repeat) {
  state.pressedKeys[key] = (action == BUTTON_PRESSED);
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

bool lovrSystemInit() {
  if (state.initialized) return false;
  os_on_key(onKey);
  os_on_text(onText);
  os_on_permission(onPermission);
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

bool lovrSystemIsKeyDown(int keycode) {
  return state.pressedKeys[keycode];
}

void lovrSystemRequestPermission(Permission permission) {
  os_request_permission((os_permission) permission);
}

void lovrSystemOpenWindow(os_window_config* window) {
  lovrAssert(os_window_open(window), "Could not open window");
  os_on_quit(onQuit);
}

bool lovrSystemIsWindowOpen() {
  return os_window_is_open();
}

void lovrSystemGetWindowSize(uint32_t* width, uint32_t* height) {
  os_window_get_size(width, height);
}

float lovrSystemGetWindowDensity() {
  return os_window_get_pixel_density();
}
