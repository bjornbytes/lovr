#ifdef _WIN32
#define APIENTRY __stdcall
#endif

#include "lib/glad/glad.h"
#include <GLFW/glfw3.h>

#pragma once

GLFWwindow* window;

void initGlfw();
