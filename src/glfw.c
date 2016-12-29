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

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

  window = glfwCreateWindow(800, 600, "Window", NULL, NULL);

  if (!window) {
    glfwTerminate();
    error("Could not create window");
  }

  glfwMakeContextCurrent(window);

#ifndef EMSCRIPTEN
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    error("Could not initialize GLEW");
  } else if (!GLEW_VERSION_3_1) {
    error("OpenGL 3 or above is required to use OpenVR functionality.");
  }
#endif

  glfwSetTime(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef EMSCRIPTEN
  glfwSwapInterval(0);
  glEnable(GL_LINE_SMOOTH);
#endif
}
