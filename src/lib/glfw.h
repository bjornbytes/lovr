#pragma once

#if LOVR_WEB
#define GLFW_INCLUDE_ES3
#elif __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#elif _WIN32
#define APIENTRY __stdcall
#endif

#include "glad/glad.h"
#include <GLFW/glfw3.h>
