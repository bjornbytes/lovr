#include "glfw.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

static void onError(int code, const char* description) {
  error(description);
}

void initGlfw() {
  glfwSetErrorCallback(onError);

  if (!glfwInit()) {
    error("Error initializing glfw");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);

  window = glfwCreateWindow(800, 600, "Window", NULL, NULL);

  if (!window) {
    glfwTerminate();
    error("Could not create window");
  }

  glfwMakeContextCurrent(window);

  gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

  glfwSetTime(0);
  glfwSwapInterval(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_MULTISAMPLE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
}
