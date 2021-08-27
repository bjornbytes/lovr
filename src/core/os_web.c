#include "os.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/dom_pk_codes.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define CANVAS "#canvas"

static struct {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
  fn_quit* onQuitRequest;
  fn_focus* onWindowFocus;
  fn_resize* onWindowResize;
  fn_key* onKeyboardEvent;
  bool keyMap[KEY_COUNT];
  bool mouseMap[2];
  os_mouse_mode mouseMode;
  long mouseX;
  long mouseY;
  int width;
  int height;
  int framebufferWidth;
  int framebufferHeight;
} state;

static const char* onBeforeUnload(int type, const void* unused, void* userdata) {
  if (state.onQuitRequest) {
    state.onQuitRequest();
  }
  return NULL;
}

static EM_BOOL onFocusChanged(int type, const EmscriptenFocusEvent* data, void* userdata) {
  if (state.onWindowFocus) {
    state.onWindowFocus(type == EMSCRIPTEN_EVENT_FOCUS);
    return true;
  }
  return false;
}

static EM_BOOL onResize(int type, const EmscriptenUiEvent* data, void* userdata) {
  emscripten_webgl_get_drawing_buffer_size(state.context, &state.framebufferWidth, &state.framebufferHeight);

  int newWidth, newHeight;
  emscripten_get_canvas_element_size(CANVAS, &newWidth, &newHeight);
  if (state.width != newWidth || state.height != newHeight) {
    state.width = newWidth;
    state.height = newHeight;
    if (state.onWindowResize) {
      state.onWindowResize(state.width, state.height);
      return true;
    }
  }

  return false;
}

static EM_BOOL onMouseButton(int type, const EmscriptenMouseEvent* data, void* userdata) {
  os_mouse_button button;
  switch (data->button) {
    case 0: button = MOUSE_LEFT; break;
    case 2: button = MOUSE_RIGHT; break;
    default: return false;
  }

  state.mouseMap[button] = type == EMSCRIPTEN_EVENT_MOUSEDOWN;
  return false;
}

static EM_BOOL onMouseMove(int type, const EmscriptenMouseEvent* data, void* userdata) {
  if (state.mouseMode == MOUSE_MODE_GRABBED) {
    state.mouseX += data->movementX;
    state.mouseY += data->movementY;
  } else {
    state.mouseX = data->clientX;
    state.mouseY = data->clientY;
  }
  return false;
}

static EM_BOOL onKeyEvent(int type, const EmscriptenKeyboardEvent* data, void* userdata) {
  os_key key;
  DOM_PK_CODE_TYPE scancode = emscripten_compute_dom_pk_code(data->code);
  switch (scancode) {
    case DOM_PK_ESCAPE: key = KEY_ESCAPE; break;
    case DOM_PK_0: key = KEY_0; break;
    case DOM_PK_1: key = KEY_1; break;
    case DOM_PK_2: key = KEY_2; break;
    case DOM_PK_3: key = KEY_3; break;
    case DOM_PK_4: key = KEY_4; break;
    case DOM_PK_5: key = KEY_5; break;
    case DOM_PK_6: key = KEY_6; break;
    case DOM_PK_7: key = KEY_7; break;
    case DOM_PK_8: key = KEY_8; break;
    case DOM_PK_9: key = KEY_9; break;
    case DOM_PK_MINUS: key = KEY_MINUS; break;
    case DOM_PK_EQUAL: key = KEY_EQUALS; break;
    case DOM_PK_BACKSPACE: key = KEY_BACKSPACE; break;
    case DOM_PK_TAB: key = KEY_TAB; break;
    case DOM_PK_Q: key = KEY_Q; break;
    case DOM_PK_W: key = KEY_W; break;
    case DOM_PK_E: key = KEY_E; break;
    case DOM_PK_R: key = KEY_R; break;
    case DOM_PK_T: key = KEY_T; break;
    case DOM_PK_Y: key = KEY_Y; break;
    case DOM_PK_U: key = KEY_U; break;
    case DOM_PK_I: key = KEY_I; break;
    case DOM_PK_O: key = KEY_O; break;
    case DOM_PK_P: key = KEY_P; break;
    case DOM_PK_BRACKET_LEFT: key = KEY_LEFT_BRACKET; break;
    case DOM_PK_BRACKET_RIGHT: key = KEY_RIGHT_BRACKET; break;
    case DOM_PK_ENTER: key = KEY_ENTER; break;
    case DOM_PK_CONTROL_LEFT: key = KEY_LEFT_CONTROL; break;
    case DOM_PK_A: key = KEY_A; break;
    case DOM_PK_S: key = KEY_S; break;
    case DOM_PK_D: key = KEY_D; break;
    case DOM_PK_F: key = KEY_F; break;
    case DOM_PK_G: key = KEY_G; break;
    case DOM_PK_H: key = KEY_H; break;
    case DOM_PK_J: key = KEY_J; break;
    case DOM_PK_K: key = KEY_K; break;
    case DOM_PK_L: key = KEY_L; break;
    case DOM_PK_SEMICOLON: key = KEY_SEMICOLON; break;
    case DOM_PK_QUOTE: key = KEY_APOSTROPHE; break;
    case DOM_PK_BACKQUOTE: key = KEY_BACKTICK; break;
    case DOM_PK_SHIFT_LEFT: key = KEY_LEFT_SHIFT; break;
    case DOM_PK_BACKSLASH: key = KEY_BACKSLASH; break;
    case DOM_PK_Z: key = KEY_Z; break;
    case DOM_PK_X: key = KEY_X; break;
    case DOM_PK_C: key = KEY_C; break;
    case DOM_PK_V: key = KEY_V; break;
    case DOM_PK_B: key = KEY_B; break;
    case DOM_PK_N: key = KEY_N; break;
    case DOM_PK_M: key = KEY_M; break;
    case DOM_PK_COMMA: key = KEY_COMMA; break;
    case DOM_PK_PERIOD: key = KEY_PERIOD; break;
    case DOM_PK_SLASH: key = KEY_SLASH; break;
    case DOM_PK_SHIFT_RIGHT: key = KEY_RIGHT_SHIFT; break;
    case DOM_PK_ALT_LEFT: key = KEY_LEFT_ALT; break;
    case DOM_PK_SPACE: key = KEY_SPACE; break;
    case DOM_PK_CAPS_LOCK: key = KEY_CAPS_LOCK; break;
    case DOM_PK_F1: key = KEY_F1; break;
    case DOM_PK_F2: key = KEY_F2; break;
    case DOM_PK_F3: key = KEY_F3; break;
    case DOM_PK_F4: key = KEY_F4; break;
    case DOM_PK_F5: key = KEY_F5; break;
    case DOM_PK_F6: key = KEY_F6; break;
    case DOM_PK_F7: key = KEY_F7; break;
    case DOM_PK_F8: key = KEY_F8; break;
    case DOM_PK_F9: key = KEY_F9; break;
    case DOM_PK_SCROLL_LOCK: key = KEY_SCROLL_LOCK; break;
    case DOM_PK_F11: key = KEY_F11; break;
    case DOM_PK_F12: key = KEY_F12; break;
    case DOM_PK_CONTROL_RIGHT: key = KEY_RIGHT_CONTROL; break;
    case DOM_PK_ALT_RIGHT: key = KEY_RIGHT_ALT; break;
    case DOM_PK_NUM_LOCK: key = KEY_NUM_LOCK; break;
    case DOM_PK_HOME: key = KEY_HOME; break;
    case DOM_PK_ARROW_UP: key = KEY_UP; break;
    case DOM_PK_PAGE_UP: key = KEY_PAGE_UP; break;
    case DOM_PK_ARROW_LEFT: key = KEY_LEFT; break;
    case DOM_PK_ARROW_RIGHT: key = KEY_RIGHT; break;
    case DOM_PK_END: key = KEY_END; break;
    case DOM_PK_ARROW_DOWN: key = KEY_DOWN; break;
    case DOM_PK_PAGE_DOWN: key = KEY_PAGE_DOWN; break;
    case DOM_PK_INSERT: key = KEY_INSERT; break;
    case DOM_PK_DELETE: key = KEY_DELETE; break;
    case DOM_PK_OS_LEFT: key = KEY_LEFT_OS; break;
    case DOM_PK_OS_RIGHT: key = KEY_RIGHT_OS; break;
    default: return false;
  }

  os_button_action action = type == EMSCRIPTEN_EVENT_KEYDOWN ? BUTTON_PRESSED : BUTTON_RELEASED;
  state.keyMap[key] = action == BUTTON_PRESSED;

  if (state.onKeyboardEvent) {
    state.onKeyboardEvent(action, key, scancode, data->repeat);
  }

  return false;
}

bool os_init() {
  emscripten_set_beforeunload_callback(NULL, onBeforeUnload);
  emscripten_set_focus_callback(CANVAS, NULL, true, onFocusChanged);
  emscripten_set_blur_callback(CANVAS, NULL, true, onFocusChanged);
  emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, onResize);
  emscripten_set_mousedown_callback(CANVAS, NULL, true, onMouseButton);
  emscripten_set_mouseup_callback(CANVAS, NULL, true, onMouseButton);
  emscripten_set_mousemove_callback(CANVAS, NULL, true, onMouseMove);
  emscripten_set_keydown_callback(CANVAS, NULL, true, onKeyEvent);
  emscripten_set_keyup_callback(CANVAS, NULL, true, onKeyEvent);
  return true;
}

void os_destroy() {
  emscripten_set_beforeunload_callback(NULL, NULL);
  emscripten_set_focus_callback(CANVAS, NULL, true, NULL);
  emscripten_set_blur_callback(CANVAS, NULL, true, NULL);
  emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, NULL);
  emscripten_set_mousedown_callback(CANVAS, NULL, true, NULL);
  emscripten_set_mouseup_callback(CANVAS, NULL, true, NULL);
  emscripten_set_mousemove_callback(CANVAS, NULL, true, NULL);
  emscripten_set_keydown_callback(CANVAS, NULL, true, NULL);
  emscripten_set_keyup_callback(CANVAS, NULL, true, NULL);
}

const char* os_get_name() {
  return "Web";
}

uint32_t os_get_core_count() {
  return 1;
}

void os_open_console() {
  //
}

double os_get_time() {
  return emscripten_get_now() / 1000.;
}

void os_sleep(double seconds) {
  emscripten_sleep((unsigned int) (seconds * 1000. + .5));
}

void os_request_permission(os_permission permission) {
  //
}

void os_poll_events() {
  //
}

void os_on_permission(fn_permission* callback) {
  //
}

size_t os_get_home_directory(char* buffer, size_t size) {
  const char* path = getenv("HOME");
  size_t length = strlen(path);
  if (length >= size) { return 0; }
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t os_get_data_directory(char* buffer, size_t size) {
  const char* path = "/home/web_user";
  size_t length = strlen(path);
  if (length >= size) { return 0; }
  memcpy(buffer, path, length);
  buffer[length] = '\0';
  return length;
}

size_t os_get_working_directory(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t os_get_executable_path(char* buffer, size_t size) {
  return 0;
}

size_t os_get_bundle_path(char* buffer, size_t size, const char** root) {
  *root = NULL;
  return 0;
}

bool os_window_open(const os_window_config* flags) {
  if (state.context) {
    return true;
  }

  EmscriptenWebGLContextAttributes attributes;
  emscripten_webgl_init_context_attributes(&attributes);
  attributes.alpha = false;
  attributes.depth = true;
  attributes.stencil = true;
  attributes.preserveDrawingBuffer = false;
  attributes.majorVersion = 2;
  attributes.minorVersion = 0;
  state.context = emscripten_webgl_create_context(CANVAS, &attributes);
  if (state.context < 0) {
    state.context = 0;
    return false;
  }

  emscripten_webgl_make_context_current(state.context);
  emscripten_webgl_get_drawing_buffer_size(state.context, &state.framebufferWidth, &state.framebufferHeight);
  emscripten_get_canvas_element_size(CANVAS, &state.width, &state.height);
  return true;
}

bool os_window_is_open() {
  return state.context > 0;
}

void os_window_get_size(int* width, int* height) {
  *width = state.width;
  *height = state.height;
}

void os_window_get_fbsize(int* width, int* height) {
  *width = state.framebufferWidth;
  *height = state.framebufferHeight;
}

void os_on_quit(fn_quit* callback) {
  state.onQuitRequest = callback;
}

void os_on_focus(fn_focus* callback) {
  state.onWindowFocus = callback;
}

void os_on_resize(fn_resize* callback) {
  state.onWindowResize = callback;
}

void os_on_key(fn_key* callback) {
  state.onKeyboardEvent = callback;
}

void os_on_text(fn_text* callback) {
  //
}

void os_get_mouse_position(double* x, double* y) {
  *x = state.mouseX;
  *y = state.mouseY;
}

void os_set_mouse_mode(os_mouse_mode mode) {
  if (state.mouseMode != mode) {
    state.mouseMode = mode;
    if (mode == MOUSE_MODE_GRABBED) {
      emscripten_run_script("Module['canvas'].requestPointerLock();");
    } else {
      emscripten_run_script("document.exitPointerLock();");
    }
  }
}

bool os_is_mouse_down(os_mouse_button button) {
  return state.mouseMap[button];
}

bool os_is_key_down(os_key key) {
  return state.keyMap[key];
}
