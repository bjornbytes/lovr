/* vim: set ts=2 sts=2 sw=2: */
#include "headset/headset.h"
#include "graphics/texture.h"
#include "lib/glfw.h"
#include <stdbool.h>
#ifndef _WIN32
#define __stdcall
#endif

#pragma once


/* A default fake headset to stand in for a missing VR one.
 * uses mouse to look around, WASD or cursor keys to move.
 * Left shift and Space move up and down.
 */

void lovrHeadsetRefreshControllers();
Controller* lovrHeadsetAddController(unsigned int id);
