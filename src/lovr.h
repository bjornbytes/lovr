#include "luax.h"

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 8
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Cyber Pigeon"

void lovrInit(lua_State* L, int argc, char** argv);
void lovrDestroy(int exitCode);
void lovrRun(lua_State* L);
