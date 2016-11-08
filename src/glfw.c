#include "glfw.h"
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

void initGlfw(GLFWerrorfun onError, GLFWwindowclosefun onClose, void* userPointer) {
  glfwSetErrorCallback(onError);

  if (!glfwInit()) {
    error("Error initializing glfw");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  //glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  window = glfwCreateWindow(800, 600, "Window", NULL, NULL);

  if (!window) {
    glfwTerminate();
    error("Could not create window");
  }

  glfwSetWindowCloseCallback(window, onClose);
  glfwSetWindowUserPointer(window, userPointer);
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
  glfwSwapInterval(1);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
}
