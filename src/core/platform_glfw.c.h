#include "util.h"

// Include this in ONE translation unit

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifndef EMSCRIPTEN
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif
#include <GLFW/glfw3native.h>
#endif

getProcAddressProc lovrGetProcAddress = glfwGetProcAddress;

static struct {
  GLFWwindow* window;
  windowCloseCallback onWindowClose;
  windowResizeCallback onWindowResize;
  mouseButtonCallback onMouseButton;
  keyboardCallback onKeyboardEvent;
} state;

static void onWindowClose(GLFWwindow* window) {
  if (state.onWindowClose) {
    state.onWindowClose();
  }
}

static void onWindowResize(GLFWwindow* window, int width, int height) {
  if (state.onWindowResize) {
    glfwGetFramebufferSize(window, &width, &height);
    state.onWindowResize(width, height);
  }
}

static void onMouseButton(GLFWwindow* window, int b, int a, int mods) {
  if (state.onMouseButton && (b == GLFW_MOUSE_BUTTON_LEFT || b == GLFW_MOUSE_BUTTON_RIGHT)) {
    MouseButton button = (b == GLFW_MOUSE_BUTTON_LEFT) ? MOUSE_LEFT : MOUSE_RIGHT;
    ButtonAction action = (a == GLFW_PRESS) ? BUTTON_PRESSED : BUTTON_RELEASED;
    state.onMouseButton(button, action);
  }
}

static void onKeyboardEvent(GLFWwindow* window, int k, int scancode, int a, int mods) {
  if (state.onKeyboardEvent) {
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
    state.onKeyboardEvent(key, action);
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

static void onGlfwError(int code, const char* description) {
  lovrThrow(description);
}

bool lovrPlatformInit() {
  glfwSetErrorCallback(onGlfwError);
  glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
  return glfwInit();
}

void lovrPlatformDestroy() {
  glfwTerminate();
}

void lovrPlatformPollEvents() {
  glfwPollEvents();
}

double lovrPlatformGetTime() {
  return glfwGetTime();
}

void lovrPlatformSetTime(double t) {
  glfwSetTime(t);
}

bool lovrPlatformCreateWindow(WindowFlags* flags) {
  if (state.window) {
    return true;
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

  state.window = glfwCreateWindow(width, height, flags->title, flags->fullscreen ? monitor : NULL, NULL);

  if (!state.window) {
    return false;
  }

  if (flags->icon.data) {
    glfwSetWindowIcon(state.window, 1, &(GLFWimage) {
      .pixels = flags->icon.data,
      .width = flags->icon.width,
      .height = flags->icon.height
    });
  }

  glfwMakeContextCurrent(state.window);
  glfwSetWindowCloseCallback(state.window, onWindowClose);
  glfwSetWindowSizeCallback(state.window, onWindowResize);
  glfwSetMouseButtonCallback(state.window, onMouseButton);
  glfwSetKeyCallback(state.window, onKeyboardEvent);
  lovrPlatformSetSwapInterval(flags->vsync);
  return true;
}

bool lovrPlatformHasWindow() {
  return state.window;
}

void lovrPlatformGetWindowSize(int* width, int* height) {
  if (state.window) {
    glfwGetWindowSize(state.window, width, height);
  } else {
    if (*width) *width = 0;
    if (*height) *height = 0;
  }
}

void lovrPlatformGetFramebufferSize(int* width, int* height) {
  if (state.window) {
    glfwGetFramebufferSize(state.window, width, height);
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
  glfwSwapBuffers(state.window);
}

void lovrPlatformOnWindowClose(windowCloseCallback callback) {
  state.onWindowClose = callback;
}

void lovrPlatformOnWindowResize(windowResizeCallback callback) {
  state.onWindowResize = callback;
}

void lovrPlatformOnMouseButton(mouseButtonCallback callback) {
  state.onMouseButton = callback;
}

void lovrPlatformOnKeyboardEvent(keyboardCallback callback) {
  state.onKeyboardEvent = callback;
}

void lovrPlatformGetMousePosition(double* x, double* y) {
  if (state.window) {
    glfwGetCursorPos(state.window, x, y);
  } else {
    *x = *y = 0.;
  }
}

void lovrPlatformSetMouseMode(MouseMode mode) {
  if (state.window) {
    int m = (mode == MOUSE_MODE_GRABBED) ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(state.window, GLFW_CURSOR, m);
  }
}

bool lovrPlatformIsMouseDown(MouseButton button) {
  return state.window ? glfwGetMouseButton(state.window, convertMouseButton(button)) == GLFW_PRESS : false;
}

bool lovrPlatformIsKeyDown(KeyCode key) {
  return state.window ? glfwGetKey(state.window, convertKeyCode(key)) == GLFW_PRESS : false;
}

#ifdef _WIN32
HANDLE lovrPlatformGetWindow() {
  return (HANDLE) glfwGetWin32Window(state.window);
}

HGLRC lovrPlatformGetContext() {
  return glfwGetWGLContext(state.window);
}
#endif
