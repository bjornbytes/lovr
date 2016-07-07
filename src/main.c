#define GLFW_INCLUDE_GLCOREARB

#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>
#include <luajit.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "util.h"
#include "lovr.h"

GLFWwindow* window;

void onError(int code, const char* description) {
  error(description);
}

void initGlfw() {
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
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);
}

int main(int argc, char* argv[]) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  lovrInit(L);
  initGlfw();

  lovrRun(L);

  return 0;
}
