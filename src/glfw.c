#include "glfw.h"
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

void initGlfw(GLFWerrorfun onError, GLFWwindowclosefun onClose) {
  glfwSetErrorCallback(onError);

  if (!glfwInit()) {
    error("Error initializing glfw");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);

  int count;
  GLFWmonitor** monitors = glfwGetMonitors(&count);

  for (int i = 0; i < count; i++) {
    const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
    if (mode->refreshRate == 90) {
      glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      window = glfwCreateWindow(mode->width, mode->height, "Window", monitors[i], NULL);
      break;
    }
  }

  if (!window) {
    glfwTerminate();
    error("Could not create window");
  }

  glfwSetWindowCloseCallback(window, onClose);
  glfwMakeContextCurrent(window);

#ifdef WIN32
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    error("Could not initialize GLEW");
  } else if (!GLEW_VERSION_2_1) {
    error("Geez your OpenGL is old");
  }
#endif

  glfwSetTime(0);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
}
