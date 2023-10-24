#include "system/system.h"
#include "event/event.h"
#include "graphics/graphics.h"
#include "headset/headset.h"
#include "core/os.h"
#include "util.h"
#include <string.h>

static struct {
  bool initialized;
  bool keyRepeat;
  bool prevKeyState[OS_KEY_COUNT];
  bool keyState[OS_KEY_COUNT];
  bool mouseState[8];
  double mouseX;
  double mouseY;
} state;

extern void lovrGraphicsOnResize(uint32_t width, uint32_t height);
extern void lovrSimulatorOnFocus(bool focused);
extern void lovrSimulatorOnResize(uint32_t width, uint32_t height);
extern void lovrSimulatorOnScroll(double dx, double dy);

static void onEvent(os_event_type type, os_event_data* data) {
  switch (type) {
    case OS_EVENT_QUIT:
      lovrEventPush((Event) {
        .type = EVENT_QUIT,
        .data.quit.exitCode = 0
      });
      break;

    case OS_EVENT_FOCUS:
#ifndef LOVR_DISABLE_HEADSET
      if (lovrHeadsetInterface && lovrHeadsetInterface->driverType == DRIVER_SIMULATOR) {
        lovrSimulatorOnFocus(data->focus.focused);
        lovrEventPush((Event) {
          .type = EVENT_FOCUS,
          .data.boolean.value = data->focus.focused
        });
      }
#endif
      break;

    case OS_EVENT_RESIZE:
#ifndef LOVR_DISABLE_GRAPHICS
      lovrGraphicsOnResize(data->resize.width, data->resize.height);
#endif
#ifndef LOVR_DISABLE_HEADSET
      if (lovrHeadsetInterface && lovrHeadsetInterface->driverType == DRIVER_SIMULATOR) {
        lovrSimulatorOnResize(data->resize.width, data->resize.height);
      }
#endif
      lovrEventPush((Event) {
        .type = EVENT_RESIZE,
        .data.resize.width = data->resize.width,
        .data.resize.height = data->resize.height
      });
      break;

    case OS_EVENT_KEY_PRESSED:
    case OS_EVENT_KEY_RELEASED:
      if (data->key.repeat && !state.keyRepeat) return;
      state.keyState[data->key.code] = (type == OS_EVENT_KEY_PRESSED);
      lovrEventPush((Event) {
        .type = OS_EVENT_KEY_PRESSED ? EVENT_KEYPRESSED : EVENT_KEYRELEASED,
        .data.key.code = data->key.code,
        .data.key.scancode = data->key.scancode,
        .data.key.repeat = data->key.repeat
      });
      break;

    case OS_EVENT_TEXT_INPUT: {
      Event event;
      event.type = EVENT_TEXTINPUT;
      event.data.text.codepoint = data->text.codepoint;
      memset(&event.data.text.utf8, 0, sizeof(event.data.text.utf8));
      utf8_encode(data->text.codepoint, event.data.text.utf8);
      lovrEventPush(event);
      break;
    }

    case OS_EVENT_MOUSE_PRESSED:
    case OS_EVENT_MOUSE_RELEASED:
      if ((size_t) data->mouse.button < COUNTOF(state.mouseState)) {
        state.mouseState[data->mouse.button] = type == OS_EVENT_MOUSE_PRESSED;
      }
      lovrEventPush((Event) {
        .type = type == OS_EVENT_MOUSE_PRESSED ? EVENT_MOUSEPRESSED : EVENT_MOUSERELEASED,
        .data.mouse.x = state.mouseX,
        .data.mouse.y = state.mouseY,
        .data.mouse.button = data->mouse.button
      });
      break;

    case OS_EVENT_MOUSE_MOVED:
      lovrEventPush((Event) {
        .type = EVENT_MOUSEMOVED,
        .data.mouse.x = data->move.x,
        .data.mouse.y = data->move.y,
        .data.mouse.dx = data->move.x - state.mouseX,
        .data.mouse.dy = data->move.y - state.mouseY
      });
      state.mouseX = data->move.x;
      state.mouseY = data->move.y;
      break;

    case OS_EVENT_WHEEL_MOVED:
#ifndef LOVR_DISABLE_HEADSET
      if (lovrHeadsetInterface && lovrHeadsetInterface->driverType == DRIVER_SIMULATOR) {
        lovrSimulatorOnScroll(data->scroll.dx, data->scroll.dy);
      }
#endif
      lovrEventPush((Event) {
        .type = EVENT_MOUSEWHEELMOVED,
        .data.wheel.x = data->scroll.dx,
        .data.wheel.y = data->scroll.dy,
      });
      break;

    case OS_EVENT_PERMISSION:
      lovrEventPush((Event) {
        .type = EVENT_PERMISSION,
        .data.permission.permission = data->permission.type,
        .data.permission.granted = data->permission.granted
      });
      break;

    default:
      lovrUnreachable();
  }
}

bool lovrSystemInit(void) {
  if (state.initialized) return false;
  state.initialized = true;
  os_on_event(onEvent);
  os_get_mouse_position(&state.mouseX, &state.mouseY);
  return true;
}

void lovrSystemDestroy(void) {
  if (!state.initialized) return;
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

void lovrSystemPollEvents(void) {
  memcpy(state.prevKeyState, state.keyState, sizeof(state.keyState));
  os_poll_events();
}

void lovrSystemOpenWindow(os_window_config* window) {
  lovrAssert(os_window_open(window), "Could not open window");
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
