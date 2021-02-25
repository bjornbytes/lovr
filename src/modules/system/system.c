#include "system/system.h"
#include "event/event.h"
#include "core/os.h"
#include "core/util.h"
#include <string.h>

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

static struct {
  bool initialized;
} state;

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
