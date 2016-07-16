#include "glfw.h"
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

  // TODO make configurable
  window = glfwCreateWindow(800, 600, "Window", NULL, NULL);

  if (!window) {
    glfwTerminate();
    error("Could not create window");
  }

  glfwSetWindowCloseCallback(window, onClose);
  glfwMakeContextCurrent(window);

#ifdef WIN32
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    error("Could not initialize GLEW");
  } else if (!GLEW_VERSION_2_1) {
    error("Geez your OpenGL is old");
  }
#endif
}
