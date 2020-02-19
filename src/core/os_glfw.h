#include "util.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifndef EMSCRIPTEN
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif
#include <GLFW/glfw3native.h>
#endif

static struct {
  GLFWwindow* window;
  windowCloseCallback onWindowClose;
  windowFocusCallback onWindowFocus;
  windowResizeCallback onWindowResize;
  mouseButtonCallback onMouseButton;
  keyboardCallback onKeyboardEvent;
} glfwState;

static void onError(int code, const char* description) {
  lovrThrow(description);
}

static void onWindowClose(GLFWwindow* window) {
  if (glfwState.onWindowClose) {
    glfwState.onWindowClose();
  }
}

static void onWindowFocus(GLFWwindow* window, int focused) {
  if (glfwState.onWindowFocus) {
    glfwState.onWindowFocus(focused);
  }
}

static void onWindowResize(GLFWwindow* window, int width, int height) {
  if (glfwState.onWindowResize) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwState.onWindowResize(width, height);
  }
}

static void onMouseButton(GLFWwindow* window, int b, int a, int mods) {
  if (glfwState.onMouseButton && (b == GLFW_MOUSE_BUTTON_LEFT || b == GLFW_MOUSE_BUTTON_RIGHT)) {
    MouseButton button = (b == GLFW_MOUSE_BUTTON_LEFT) ? MOUSE_LEFT : MOUSE_RIGHT;
    ButtonAction action = (a == GLFW_PRESS) ? BUTTON_PRESSED : BUTTON_RELEASED;
    glfwState.onMouseButton(button, action);
  }
}

static void onKeyboardEvent(GLFWwindow* window, int k, int scancode, int a, int mods) {
  if (glfwState.onKeyboardEvent) {
    KeyCode key;
    switch (k) {
      case GLFW_KEY_W: key = KEY_W; break;
      case GLFW_KEY_A: key = KEY_A; break;
      case GLFW_KEY_S: key = KEY_S; break;
      case GLFW_KEY_D: key = KEY_D; break;
      case GLFW_KEY_Q: key = KEY_Q; break;
      case GLFW_KEY_E: key = KEY_E; break;
      case GLFW_KEY_UP: key = KEY_UP; break;
      case GLFW_KEY_DOWN: key = KEY_DOWN; break;
      case GLFW_KEY_LEFT: key = KEY_LEFT; break;
      case GLFW_KEY_RIGHT: key = KEY_RIGHT; break;
      case GLFW_KEY_ESCAPE: key = KEY_ESCAPE; break;
      case GLFW_KEY_F5: key = KEY_F5; break;
      default: return;
    }
    ButtonAction action = (a == GLFW_PRESS) ? BUTTON_PRESSED : BUTTON_RELEASED;
    glfwState.onKeyboardEvent(key, action);
  }
}

static int convertMouseButton(MouseButton button) {
  switch (button) {
    case MOUSE_LEFT: return GLFW_MOUSE_BUTTON_LEFT;
    case MOUSE_RIGHT: return GLFW_MOUSE_BUTTON_RIGHT;
    default: lovrThrow("Unreachable");
  }
}

static int convertKeyCode(KeyCode key) {
  switch (key) {
    case KEY_W: return GLFW_KEY_W;
    case KEY_A: return GLFW_KEY_A;
    case KEY_S: return GLFW_KEY_S;
    case KEY_D: return GLFW_KEY_D;
    case KEY_Q: return GLFW_KEY_Q;
    case KEY_E: return GLFW_KEY_E;
    case KEY_UP: return GLFW_KEY_UP;
    case KEY_DOWN: return GLFW_KEY_DOWN;
    case KEY_LEFT: return GLFW_KEY_LEFT;
    case KEY_RIGHT: return GLFW_KEY_RIGHT;
    case KEY_ESCAPE: return GLFW_KEY_ESCAPE;
    case KEY_F5: return GLFW_KEY_F5;
    default: lovrThrow("Unreachable");
  }
}

void lovrPlatformPollEvents() {
  if (glfwState.window) {
    glfwPollEvents();
  }
}

bool lovrPlatformCreateWindow(WindowFlags* flags) {
  if (glfwState.window) {
    return true;
  }

  glfwSetErrorCallback(onError);
#ifdef __APPLE__
  glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
#endif
  if (!glfwInit()) {
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, flags->msaa);
  glfwWindowHint(GLFW_RESIZABLE, flags->resizable);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  uint32_t width = flags->width ? flags->width : (uint32_t) mode->width;
  uint32_t height = flags->height ? flags->height : (uint32_t) mode->height;

  if (flags->fullscreen) {
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
  }

  glfwState.window = glfwCreateWindow(width, height, flags->title, flags->fullscreen ? monitor : NULL, NULL);

  if (!glfwState.window) {
    return false;
  }

  if (flags->icon.data) {
    glfwSetWindowIcon(glfwState.window, 1, &(GLFWimage) {
      .pixels = flags->icon.data,
      .width = flags->icon.width,
      .height = flags->icon.height
    });
  }

  glfwMakeContextCurrent(glfwState.window);
  glfwSetWindowCloseCallback(glfwState.window, onWindowClose);
  glfwSetWindowFocusCallback(glfwState.window, onWindowFocus);
  glfwSetWindowSizeCallback(glfwState.window, onWindowResize);
  glfwSetMouseButtonCallback(glfwState.window, onMouseButton);
  glfwSetKeyCallback(glfwState.window, onKeyboardEvent);
  lovrPlatformSetSwapInterval(flags->vsync);
  return true;
}

bool lovrPlatformHasWindow() {
  return glfwState.window;
}

void lovrPlatformGetWindowSize(int* width, int* height) {
  if (glfwState.window) {
    glfwGetWindowSize(glfwState.window, width, height);
  } else {
    if (*width) *width = 0;
    if (*height) *height = 0;
  }
}

void lovrPlatformGetFramebufferSize(int* width, int* height) {
  if (glfwState.window) {
    glfwGetFramebufferSize(glfwState.window, width, height);
  } else {
    if (*width) *width = 0;
    if (*height) *height = 0;
  }
}

void lovrPlatformSetSwapInterval(int interval) {
#if EMSCRIPTEN
  glfwSwapInterval(1);
#else
  glfwSwapInterval(interval);
#endif
}

void lovrPlatformSwapBuffers() {
  glfwSwapBuffers(glfwState.window);
}

void* lovrPlatformGetProcAddress(const char* function) {
  return (void*) glfwGetProcAddress(function);
}

void lovrPlatformOnWindowClose(windowCloseCallback callback) {
  glfwState.onWindowClose = callback;
}

void lovrPlatformOnWindowFocus(windowFocusCallback callback) {
  glfwState.onWindowFocus = callback;
}

void lovrPlatformOnWindowResize(windowResizeCallback callback) {
  glfwState.onWindowResize = callback;
}

void lovrPlatformOnMouseButton(mouseButtonCallback callback) {
  glfwState.onMouseButton = callback;
}

void lovrPlatformOnKeyboardEvent(keyboardCallback callback) {
  glfwState.onKeyboardEvent = callback;
}

void lovrPlatformGetMousePosition(double* x, double* y) {
  if (glfwState.window) {
    glfwGetCursorPos(glfwState.window, x, y);
  } else {
    *x = *y = 0.;
  }
}

void lovrPlatformSetMouseMode(MouseMode mode) {
  if (glfwState.window) {
    int m = (mode == MOUSE_MODE_GRABBED) ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(glfwState.window, GLFW_CURSOR, m);
  }
}

bool lovrPlatformIsMouseDown(MouseButton button) {
  return glfwState.window ? glfwGetMouseButton(glfwState.window, convertMouseButton(button)) == GLFW_PRESS : false;
}

bool lovrPlatformIsKeyDown(KeyCode key) {
  return glfwState.window ? glfwGetKey(glfwState.window, convertKeyCode(key)) == GLFW_PRESS : false;
}

#ifdef _WIN32
HANDLE lovrPlatformGetWindow() {
  return (HANDLE) glfwGetWin32Window(glfwState.window);
}

HGLRC lovrPlatformGetContext() {
  return glfwGetWGLContext(glfwState.window);
}
#endif
