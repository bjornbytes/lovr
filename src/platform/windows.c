#include "platform.h"
#include <Windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

getProcAddressProc lovrGetProcAddress = glfwGetProcAddress;

static struct {
  GLFWwindow* window;
  windowCloseCallback onWindowClose;
  windowResizeCallback onWindowResize;
} state;

static void onWindowClose(GLFWwindow* window) {
  if (state.onWindowClose) {
    state.onWindowClose();
  }
}

static void onWindowResize(GLFWwindow* window, int width, int height) {
  if (state.onWindowResize) {
    state.onWindowResize(width, height);
  }
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

bool lovrPlatformSetWindow(WindowFlags* flags) {
  if (state.window) {
    return true;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, flags->msaa);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, flags->srgb);

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  uint32_t width = flags->width ? flags->width : mode->width;
  uint32_t height = flags->height ? flags->height : mode->height;

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
  glfwSwapInterval(flags->vsync);
  return true;
}

void lovrPlatformGetWindowSize(uint32_t* width, uint32_t* height) {
  int w, h;
  glfwGetFramebufferSize(state.window, &w, &h);
  *width = w;
  *height = h;
}

void lovrPlatformOnWindowClose(windowCloseCallback callback) {
  glfwSetWindowCloseCallback(state.window, callback);
}

void lovrPlatformOnWindowResize(windowResizeCallback callback) {
  glfwSetWindowSizeCallback(state.window, callback);
}

void lovrPlatformSwapBuffers() {
  glfwSwapBuffers(state.window);
}

void lovrSleep(double seconds) {
  Sleep((unsigned int) (seconds * 1000));
}

int lovrGetExecutablePath(char* dest, uint32_t size) {
  return !GetModuleFileName(NULL, dest, size);
}
