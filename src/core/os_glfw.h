#ifndef LOVR_USE_GLFW

const char* os_get_clipboard_text(void) {
  return NULL;
}

void os_set_clipboard_text(const char* text) {
  //
}

void os_poll_events(double timeout) {
  //
}

bool os_window_open(const os_window_config* config) {
  return false;
}

bool os_window_is_open(void) {
  return false;
}

bool os_window_is_visible(void) {
  return false;
}

bool os_window_is_focused(void) {
  return false;
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  *width = *height = 0;
}

float os_window_get_pixel_density(void) {
  return 0.f;
}

void os_on_quit(fn_quit* callback) {
  //
}

void os_on_focus(fn_focus* callback) {
  //
}

void os_on_resize(fn_resize* callback) {
  //
}

void os_on_key(fn_key* callback) {
  //
}

void os_on_text(fn_text* callback) {
  //
}

void os_on_mouse_button(fn_mouse_button* callback) {
  //
}

void os_on_mouse_move(fn_mouse_move* callback) {
  //
}

void os_on_mousewheel_move(fn_wheel_move* callback)
{
  //
}

void os_get_mouse_position(double* x, double* y) {
  *x = *y = 0.;
}

os_mouse_mode os_get_mouse_mode(void) {
  return OS_MOUSE_NORMAL;
}

void os_set_mouse_mode(os_mouse_mode mode) {
  //
}

uintptr_t os_get_win32_window(void) {
  return 0;
}

uintptr_t os_get_win32_instance(void) {
  return 0;
}

uintptr_t os_get_ca_metal_layer(void) {
  return 0;
}

uintptr_t os_get_xcb_connection(void) {
  return 0;
}

uintptr_t os_get_xcb_window(void) {
  return 0;
}

#else

#include <stdio.h>
#include <math.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <QuartzCore/CAMetalLayer.h>
#endif

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#include <X11/Xlib-xcb.h>
#endif

#include <GLFW/glfw3native.h>

static struct {
  GLFWwindow* window;
  fn_quit* onQuitRequest;
  fn_visible* onWindowVisible;
  fn_focus* onWindowFocus;
  fn_resize* onWindowResize;
  fn_key* onKeyboardEvent;
  fn_text* onTextEvent;
  fn_mouse_button* onMouseButton;
  fn_mouse_move* onMouseMove;
  fn_mousewheel_move* onMouseWheelMove;
  uint32_t width;
  uint32_t height;
  bool fullscreen;
} glfwState;

static void onError(int code, const char* description) {
  printf("GLFW error %d: %s\n", code, description);
}

static void onWindowClose(GLFWwindow* window) {
  if (glfwState.onQuitRequest) {
    glfwState.onQuitRequest();
  }
}

static void onWindowVisible(GLFWwindow* window, int minimized) {
  if (glfwState.onWindowVisible) {
    glfwState.onWindowVisible(!minimized);
  }
}

static void onWindowFocus(GLFWwindow* window, int focused) {
  if (glfwState.onWindowFocus) {
    glfwState.onWindowFocus(focused);
  }
}

static void onWindowResize(GLFWwindow* window, int width, int height) {
  if (glfwState.onWindowResize && width > 0 && height > 0) {
    glfwState.onWindowResize(width, height);
  }
}

static void onKeyboardEvent(GLFWwindow* window, int k, int scancode, int a, int mods) {
  if (glfwState.onKeyboardEvent) {
    os_key key;
    switch (k) {
      case GLFW_KEY_A: key = OS_KEY_A; break;
      case GLFW_KEY_B: key = OS_KEY_B; break;
      case GLFW_KEY_C: key = OS_KEY_C; break;
      case GLFW_KEY_D: key = OS_KEY_D; break;
      case GLFW_KEY_E: key = OS_KEY_E; break;
      case GLFW_KEY_F: key = OS_KEY_F; break;
      case GLFW_KEY_G: key = OS_KEY_G; break;
      case GLFW_KEY_H: key = OS_KEY_H; break;
      case GLFW_KEY_I: key = OS_KEY_I; break;
      case GLFW_KEY_J: key = OS_KEY_J; break;
      case GLFW_KEY_K: key = OS_KEY_K; break;
      case GLFW_KEY_L: key = OS_KEY_L; break;
      case GLFW_KEY_M: key = OS_KEY_M; break;
      case GLFW_KEY_N: key = OS_KEY_N; break;
      case GLFW_KEY_O: key = OS_KEY_O; break;
      case GLFW_KEY_P: key = OS_KEY_P; break;
      case GLFW_KEY_Q: key = OS_KEY_Q; break;
      case GLFW_KEY_R: key = OS_KEY_R; break;
      case GLFW_KEY_S: key = OS_KEY_S; break;
      case GLFW_KEY_T: key = OS_KEY_T; break;
      case GLFW_KEY_U: key = OS_KEY_U; break;
      case GLFW_KEY_V: key = OS_KEY_V; break;
      case GLFW_KEY_W: key = OS_KEY_W; break;
      case GLFW_KEY_X: key = OS_KEY_X; break;
      case GLFW_KEY_Y: key = OS_KEY_Y; break;
      case GLFW_KEY_Z: key = OS_KEY_Z; break;
      case GLFW_KEY_0: key = OS_KEY_0; break;
      case GLFW_KEY_1: key = OS_KEY_1; break;
      case GLFW_KEY_2: key = OS_KEY_2; break;
      case GLFW_KEY_3: key = OS_KEY_3; break;
      case GLFW_KEY_4: key = OS_KEY_4; break;
      case GLFW_KEY_5: key = OS_KEY_5; break;
      case GLFW_KEY_6: key = OS_KEY_6; break;
      case GLFW_KEY_7: key = OS_KEY_7; break;
      case GLFW_KEY_8: key = OS_KEY_8; break;
      case GLFW_KEY_9: key = OS_KEY_9; break;

      case GLFW_KEY_SPACE: key = OS_KEY_SPACE; break;
      case GLFW_KEY_ENTER: key = OS_KEY_ENTER; break;
      case GLFW_KEY_TAB: key = OS_KEY_TAB; break;
      case GLFW_KEY_ESCAPE: key = OS_KEY_ESCAPE; break;
      case GLFW_KEY_BACKSPACE: key = OS_KEY_BACKSPACE; break;
      case GLFW_KEY_UP: key = OS_KEY_UP; break;
      case GLFW_KEY_DOWN: key = OS_KEY_DOWN; break;
      case GLFW_KEY_LEFT: key = OS_KEY_LEFT; break;
      case GLFW_KEY_RIGHT: key = OS_KEY_RIGHT; break;
      case GLFW_KEY_HOME: key = OS_KEY_HOME; break;
      case GLFW_KEY_END: key = OS_KEY_END; break;
      case GLFW_KEY_PAGE_UP: key = OS_KEY_PAGE_UP; break;
      case GLFW_KEY_PAGE_DOWN: key = OS_KEY_PAGE_DOWN; break;
      case GLFW_KEY_INSERT: key = OS_KEY_INSERT; break;
      case GLFW_KEY_DELETE: key = OS_KEY_DELETE; break;
      case GLFW_KEY_F1: key = OS_KEY_F1; break;
      case GLFW_KEY_F2: key = OS_KEY_F2; break;
      case GLFW_KEY_F3: key = OS_KEY_F3; break;
      case GLFW_KEY_F4: key = OS_KEY_F4; break;
      case GLFW_KEY_F5: key = OS_KEY_F5; break;
      case GLFW_KEY_F6: key = OS_KEY_F6; break;
      case GLFW_KEY_F7: key = OS_KEY_F7; break;
      case GLFW_KEY_F8: key = OS_KEY_F8; break;
      case GLFW_KEY_F9: key = OS_KEY_F9; break;
      case GLFW_KEY_F10: key = OS_KEY_F10; break;
      case GLFW_KEY_F11: key = OS_KEY_F11; break;
      case GLFW_KEY_F12: key = OS_KEY_F12; break;

      case GLFW_KEY_GRAVE_ACCENT: key = OS_KEY_BACKTICK; break;
      case GLFW_KEY_MINUS: key = OS_KEY_MINUS; break;
      case GLFW_KEY_EQUAL: key = OS_KEY_EQUALS; break;
      case GLFW_KEY_LEFT_BRACKET: key = OS_KEY_LEFT_BRACKET; break;
      case GLFW_KEY_RIGHT_BRACKET: key = OS_KEY_RIGHT_BRACKET; break;
      case GLFW_KEY_BACKSLASH: key = OS_KEY_BACKSLASH; break;
      case GLFW_KEY_SEMICOLON: key = OS_KEY_SEMICOLON; break;
      case GLFW_KEY_APOSTROPHE: key = OS_KEY_APOSTROPHE; break;
      case GLFW_KEY_COMMA: key = OS_KEY_COMMA; break;
      case GLFW_KEY_PERIOD: key = OS_KEY_PERIOD; break;
      case GLFW_KEY_SLASH: key = OS_KEY_SLASH; break;

      case GLFW_KEY_KP_0: key = OS_KEY_KP_0; break;
      case GLFW_KEY_KP_1: key = OS_KEY_KP_1; break;
      case GLFW_KEY_KP_2: key = OS_KEY_KP_2; break;
      case GLFW_KEY_KP_3: key = OS_KEY_KP_3; break;
      case GLFW_KEY_KP_4: key = OS_KEY_KP_4; break;
      case GLFW_KEY_KP_5: key = OS_KEY_KP_5; break;
      case GLFW_KEY_KP_6: key = OS_KEY_KP_6; break;
      case GLFW_KEY_KP_7: key = OS_KEY_KP_7; break;
      case GLFW_KEY_KP_8: key = OS_KEY_KP_8; break;
      case GLFW_KEY_KP_9: key = OS_KEY_KP_9; break;
      case GLFW_KEY_KP_DECIMAL: key = OS_KEY_KP_DECIMAL; break;
      case GLFW_KEY_KP_DIVIDE: key = OS_KEY_KP_DIVIDE; break;
      case GLFW_KEY_KP_MULTIPLY: key = OS_KEY_KP_MULTIPLY; break;
      case GLFW_KEY_KP_SUBTRACT: key = OS_KEY_KP_SUBTRACT; break;
      case GLFW_KEY_KP_ADD: key = OS_KEY_KP_ADD; break;
      case GLFW_KEY_KP_ENTER: key = OS_KEY_KP_ENTER; break;
      case GLFW_KEY_KP_EQUAL: key = OS_KEY_KP_EQUALS; break;

      case GLFW_KEY_LEFT_CONTROL: key = OS_KEY_LEFT_CONTROL; break;
      case GLFW_KEY_LEFT_SHIFT: key = OS_KEY_LEFT_SHIFT; break;
      case GLFW_KEY_LEFT_ALT: key = OS_KEY_LEFT_ALT; break;
      case GLFW_KEY_LEFT_SUPER: key = OS_KEY_LEFT_OS; break;
      case GLFW_KEY_RIGHT_CONTROL: key = OS_KEY_RIGHT_CONTROL; break;
      case GLFW_KEY_RIGHT_SHIFT: key = OS_KEY_RIGHT_SHIFT; break;
      case GLFW_KEY_RIGHT_ALT: key = OS_KEY_RIGHT_ALT; break;
      case GLFW_KEY_RIGHT_SUPER: key = OS_KEY_RIGHT_OS; break;

      case GLFW_KEY_CAPS_LOCK: key = OS_KEY_CAPS_LOCK; break;
      case GLFW_KEY_SCROLL_LOCK: key = OS_KEY_SCROLL_LOCK; break;
      case GLFW_KEY_NUM_LOCK: key = OS_KEY_NUM_LOCK; break;

      default: return;
    }
    os_button_action action = (a == GLFW_RELEASE) ? BUTTON_RELEASED : BUTTON_PRESSED;
    bool repeat = (a == GLFW_REPEAT);
    glfwState.onKeyboardEvent(action, key, scancode, repeat);
  }
}

static void onTextEvent(GLFWwindow* window, unsigned int codepoint) {
  if (glfwState.onTextEvent) {
    glfwState.onTextEvent(codepoint);
  }
}

static void onMouseButton(GLFWwindow* window, int button, int action, int mods) {
  if (glfwState.onMouseButton) {
    glfwState.onMouseButton(button, action == GLFW_PRESS);
  }
}

static void onMouseMove(GLFWwindow* window, double x, double y) {
  if (glfwState.onMouseMove) {
    glfwState.onMouseMove(x, y);
  }
}

static void onMouseWheelMove(GLFWwindow* window, double deltaX, double deltaY) {
  if (glfwState.onMouseWheelMove) {
    deltaX = (deltaX == 0.0) ? 0.0 : -deltaX;
    glfwState.onMouseWheelMove(deltaX, deltaY);
  }
}

static int convertKey(os_key key) {
  switch (key) {
    case OS_KEY_W: return GLFW_KEY_W;
    case OS_KEY_A: return GLFW_KEY_A;
    case OS_KEY_S: return GLFW_KEY_S;
    case OS_KEY_D: return GLFW_KEY_D;
    case OS_KEY_Q: return GLFW_KEY_Q;
    case OS_KEY_E: return GLFW_KEY_E;
    case OS_KEY_UP: return GLFW_KEY_UP;
    case OS_KEY_DOWN: return GLFW_KEY_DOWN;
    case OS_KEY_LEFT: return GLFW_KEY_LEFT;
    case OS_KEY_RIGHT: return GLFW_KEY_RIGHT;
    case OS_KEY_LEFT_SHIFT: return GLFW_KEY_LEFT_SHIFT;
    case OS_KEY_RIGHT_SHIFT: return GLFW_KEY_RIGHT_SHIFT;
    case OS_KEY_LEFT_CONTROL: return GLFW_KEY_LEFT_CONTROL;
    case OS_KEY_RIGHT_CONTROL: return GLFW_KEY_RIGHT_CONTROL;
    case OS_KEY_ESCAPE: return GLFW_KEY_ESCAPE;
    case OS_KEY_F5: return GLFW_KEY_F5;
    default: return GLFW_KEY_UNKNOWN;
  }
}

const char* os_get_clipboard_text(void) {
  return glfwGetClipboardString(NULL);
}

void os_set_clipboard_text(const char* text) {
  glfwSetClipboardString(NULL, text);
}

void os_poll_events(double timeout) {
  if (glfwState.window) {
    if (timeout == 0.) {
      glfwPollEvents();
    } else if (timeout < 0. || isinf(timeout)) {
      glfwWaitEvents();
    } else {
      glfwWaitEventsTimeout(timeout);
    }
  }
}

bool os_window_open(const os_window_config* config) {
  if (glfwState.window) {
    return true;
  }

  glfwSetErrorCallback(onError);
#ifdef __APPLE__
  glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
#endif
#ifdef __linux__
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
  if (!glfwInit()) {
    return false;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, config->resizable);

  bool center = config->centered && !config->fullscreen;

  if (center) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();

  if (config->fullscreen) {
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    glfwState.window = glfwCreateWindow(mode->width, mode->height, config->title, monitor, NULL);
  } else {
    glfwState.window = glfwCreateWindow(config->width, config->height, config->title, NULL, NULL);
  }

  if (!glfwState.window) {
    return false;
  }

  if (center && !config->fullscreen) {
    int w, h;
    int mx, my, mw, mh;
    glfwGetWindowSize(glfwState.window, &w, &h);
    glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
    glfwSetWindowPos(glfwState.window, mx + (mw - w) / 2, my + (mh - h) / 2);
    glfwShowWindow(glfwState.window);
  }

  if (config->icon.data) {
    glfwSetWindowIcon(glfwState.window, 1, &(GLFWimage) {
      .pixels = config->icon.data,
      .width = config->icon.width,
      .height = config->icon.height
    });
  }

  glfwSetWindowCloseCallback(glfwState.window, onWindowClose);
  glfwSetWindowIconifyCallback(glfwState.window, onWindowVisible);
  glfwSetWindowFocusCallback(glfwState.window, onWindowFocus);
  glfwSetWindowSizeCallback(glfwState.window, onWindowResize);
  glfwSetKeyCallback(glfwState.window, onKeyboardEvent);
  glfwSetCharCallback(glfwState.window, onTextEvent);
  glfwSetMouseButtonCallback(glfwState.window, onMouseButton);
  glfwSetCursorPosCallback(glfwState.window, onMouseMove);
  glfwSetScrollCallback(glfwState.window, onMouseWheelMove);
  glfwState.width = config->width;
  glfwState.height = config->height;
  glfwState.fullscreen = config->fullscreen;
  return true;
}

bool os_window_is_open(void) {
  return glfwState.window;
}

bool os_window_is_visible(void) {
  return glfwState.window && !glfwGetWindowAttrib(glfwState.window, GLFW_ICONIFIED);
}

bool os_window_is_focused(void) {
  return glfwState.window && glfwGetWindowAttrib(glfwState.window, GLFW_FOCUSED);
}

bool os_window_is_fullscreen(void) {
  return glfwState.fullscreen;
}

void os_window_set_fullscreen(bool fullscreen) {
  if (!glfwState.window) {
    return;
  }

  glfwState.fullscreen = fullscreen;

  if (fullscreen) {
    os_window_get_size(&glfwState.width, &glfwState.height);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(glfwState.window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  } else {
    int x, y, w, h;
    int width = (int) glfwState.width;
    int height = (int) glfwState.height;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
    glfwSetWindowMonitor(glfwState.window, NULL, x + (w - width) / 2, y + (h - height) / 2, width, height, 0);
  }
}

void os_window_get_size(uint32_t* width, uint32_t* height) {
  int w, h;
  glfwGetWindowSize(glfwState.window, &w, &h);
  *width = w;
  *height = h;
}

float os_window_get_pixel_density(void) {
  if (!glfwState.window) {
    return 0.f;
  }

  int w, h, fw, fh;
  glfwGetWindowSize(glfwState.window, &w, &h);
  glfwGetFramebufferSize(glfwState.window, &fw, &fh);
  return (w == 0 || fw == 0) ? 1.f : (float) fw / w;
}

void os_on_quit(fn_quit* callback) {
  glfwState.onQuitRequest = callback;
}

void os_on_visible(fn_focus* callback) {
  glfwState.onWindowVisible = callback;
}

void os_on_focus(fn_focus* callback) {
  glfwState.onWindowFocus = callback;
}

void os_on_resize(fn_resize* callback) {
  glfwState.onWindowResize = callback;
}

void os_on_key(fn_key* callback) {
  glfwState.onKeyboardEvent = callback;
}

void os_on_text(fn_text* callback) {
  glfwState.onTextEvent = callback;
}

void os_on_mouse_button(fn_mouse_button* callback) {
  glfwState.onMouseButton = callback;
}

void os_on_mouse_move(fn_mouse_move* callback) {
  glfwState.onMouseMove = callback;
}

void os_on_mousewheel_move(fn_mousewheel_move* callback) {
  glfwState.onMouseWheelMove = callback;
}

void os_get_mouse_position(double* x, double* y) {
  if (glfwState.window) {
    glfwGetCursorPos(glfwState.window, x, y);
  } else {
    *x = *y = 0.;
  }
}

os_mouse_mode os_get_mouse_mode(void) {
  if (glfwGetInputMode(glfwState.window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
    return OS_MOUSE_RELATIVE;
  } else {
    return OS_MOUSE_NORMAL;
  }
}

void os_set_mouse_mode(os_mouse_mode mode) {
  if (glfwState.window) {
    int m = mode == OS_MOUSE_RELATIVE ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(glfwState.window, GLFW_CURSOR, m);
  }
}

#if defined(_WIN32)
uintptr_t os_get_win32_window(void) {
  return (uintptr_t) glfwGetWin32Window(glfwState.window);
}

uintptr_t os_get_win32_instance(void) {
  return (uintptr_t) GetModuleHandle(NULL);
}
#elif defined(__APPLE__)
uintptr_t os_get_ca_metal_layer(void) {
  id window = glfwGetCocoaWindow(glfwState.window);
  id view = msg(id, window, "contentView");
  id layer = msg(id, cls(CAMetalLayer), "layer");
  CGFloat scale = msg(CGFloat, window, "backingScaleFactor");
  msg1(void, layer, "setContentsScale:", CGFloat, scale);
  msg1(void, view, "setLayer:", id, layer);
  msg1(void, view, "setWantsLayer:", BOOL, YES);
  return (uintptr_t) layer;
}
#elif defined(__linux__) && !defined(__ANDROID__)
uintptr_t os_get_xcb_connection(void) {
  return (uintptr_t) XGetXCBConnection(glfwGetX11Display());
}

uintptr_t os_get_xcb_window(void) {
  return (uintptr_t) glfwGetX11Window(glfwState.window);
}
#endif

#ifdef _WIN32
#define OS_DLL_EXPORT __declspec(dllexport)
#else
#define OS_DLL_EXPORT __attribute__((visibility("default")))
#endif

OS_DLL_EXPORT GLFWwindow* os_get_glfw_window(void) {
  return glfwState.window;
}

#endif
