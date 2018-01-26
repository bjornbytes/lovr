#include "luax.h"
#include <stdbool.h>

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 9
#define LOVR_VERSION_PATCH 1
#define LOVR_VERSION_ALIAS "Fluffy Cuttlefish"

void lovrInit(lua_State* L, int argc, char** argv);
void lovrDestroy(int exitCode);
bool lovrRun(lua_State* L); // Returns true to request continue
