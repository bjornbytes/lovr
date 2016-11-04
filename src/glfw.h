#ifdef __APPLE__
#include <GL/glew.h>
#elif _WIN32
#include <GL/glew.h>
#elif EMSCRIPTEN
#define GLFW_INCLUDE_ES2
#endif

#include <GLFW/glfw3.h>

GLFWwindow* window;

void initGlfw();
