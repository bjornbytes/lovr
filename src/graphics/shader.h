#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../glfw.h"

typedef struct {
  GLuint id;
} Shader;

void luax_pushshader(lua_State* L, Shader* shader);
Shader* luax_checkshader(lua_State* L, int index);
int luax_destroyshader(lua_State* L);

GLuint compileShader(GLuint type, const char* filename);
GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader);

extern const luaL_Reg lovrShader[];
