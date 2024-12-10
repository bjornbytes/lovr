#include "shaders/unlit.vert.h"
#include "shaders/unlit.frag.h"
#include "shaders/normal.frag.h"
#include "shaders/font.frag.h"
#include "shaders/cubemap.vert.h"
#include "shaders/cubemap.frag.h"
#include "shaders/equirect.frag.h"
#include "shaders/fill.vert.h"
#include "shaders/fill_array.frag.h"
#include "shaders/mask.vert.h"
#include "shaders/animator.comp.h"
#include "shaders/blender.comp.h"
#include "shaders/tallymerge.comp.h"

#include "shaders/lovr.glsl.h"

#define LOCATION_POSITION 10
#define LOCATION_NORMAL 11
#define LOCATION_UV 12
#define LOCATION_COLOR 13
#define LOCATION_TANGENT 14

#define LAST_BUILTIN_BINDING 3
