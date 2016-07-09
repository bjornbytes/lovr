#include "glfw.h"
#include <stdlib.h>
#include "lovr.h"
#include "util.h"

void initGlfw() {
  glfwSetErrorCallback(lovrOnError);

  if (!glfwInit()) {
    error("Error initializing glfw");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  // TODO make configurable
  window = glfwCreateWindow(800, 600, "Window", NULL, NULL);

  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetWindowCloseCallback(window, lovrOnClose);
  glfwMakeContextCurrent(window);
}
