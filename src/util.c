#include "util.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <unistd.h>
#else
#include <io.h>
#endif

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end(args);
  exit(EXIT_FAILURE);
}

int fileExists(char* filename) {
  return access(filename, 0) != -1;
}

char* loadFile(char* filename) {
  struct stat info;

  if (stat(filename, &info)) {
    error("Could not stat '%s'", filename);
  }

  int size = (int)info.st_size;
  char* buffer = malloc(size + 1);

  int fd = open(filename, O_RDONLY);

  if (fd < 0) {
    error("Could not open '%s'", filename);
  }

  if (read(fd, buffer, size) < 0) {
    error("Could not read '%s'", filename);
  }

  buffer[size] = '\0';
  return buffer;
}

int luaPreloadModule(lua_State* L, const char* key, lua_CFunction f) {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_pushcfunction(L, f);
  lua_setfield(L, -2, key);
  lua_pop(L, 2);
  return 0;
}

void luaRegisterModule(lua_State* L, const char* name, const luaL_Reg* module) {

  // Get reference to lovr
  lua_getglobal(L, "lovr");

  // Create a table and fill it with the module functions
  lua_newtable(L);
  luaL_register(L, NULL, module);

  // lovr[name] = module
  lua_setfield(L, -2, name);

  // Pop lovr
  lua_pop(L, 1);
}

void luaRegisterType(lua_State* L, const char* name, const luaL_Reg* functions, lua_CFunction gc) {

  // Push metatable
  luaL_newmetatable(L, name);
  lua_getmetatable(L, -1);

  // m.__index = m
  lua_pushvalue(L, -1);
  lua_setfield(L, -1, "__index");

  // m.__gc = gc
  if (gc) {
    lua_pushcfunction(L, gc);
    lua_setfield(L, -2, "__gc");
  }

  // m.name = name
  lua_pushstring(L, name);
  lua_setfield(L, -2, "name");

  // Register class functions
  if (functions) {
    luaL_register(L, NULL, functions);
  }

  // Pop metatable
  lua_pop(L, 1);
}
