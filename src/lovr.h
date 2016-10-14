#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "glfw.h"

void lovrInit(lua_State* L);
void lovrDestroy(int exitCode);
void lovrRun(lua_State* L, char* mainPath);
int lovrOnLuaError(lua_State* L);
void lovrOnGlfwError(int code, const char* description);
void lovrOnClose(GLFWwindow* window);
