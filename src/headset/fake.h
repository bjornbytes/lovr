#include "headset/headset.h"
#include "graphics/texture.h"
#include "lib/glfw.h"
#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif

#pragma once

void lovrHeadsetRefreshControllers();
Controller* lovrHeadsetAddController(unsigned int id);
