#ifdef __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#elif WIN32
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

GLFWwindow* window;

void initGlfw();
