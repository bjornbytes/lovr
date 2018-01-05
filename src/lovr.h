#include "luax.h"
#include <stdbool.h>

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 9
#define LOVR_VERSION_PATCH 1
#define LOVR_VERSION_ALIAS "Fluffy Cuttlefish"

// If true, monitor source files for and allow resetting (for example if they change)
extern bool lovrReloadEnable;
// Is set true when a no-exit reset occurs and set false once that reset is complete
extern bool lovrReloadPending;

void lovrInit(lua_State* L, int argc, char** argv);
void lovrDestroy(int exitCode);
void lovrRun(lua_State* L);
