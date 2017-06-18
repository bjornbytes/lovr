#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#elif __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#elif LOVR_WEB
#define GLFW_INCLUDE_ES3
#endif

#include <GLFW/glfw3.h>
