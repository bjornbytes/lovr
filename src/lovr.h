#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define LOVR_VERSION "0.1.0"

void lovrInit(lua_State* L, int argc, char** argv);
void lovrDestroy(int exitCode);
void lovrRun(lua_State* L);
