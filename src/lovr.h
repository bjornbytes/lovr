#include "luax.h"

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 5
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Psychotic Octopus"

void lovrInit(lua_State* L, int argc, char** argv);
void lovrDestroy(int exitCode);
void lovrRun(lua_State* L);
