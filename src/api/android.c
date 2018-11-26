#include "api.h"
#include "lib/sds/sds.h"
#include <unistd.h>
#include "platform.h"

static int l_lovrGetApplicationId(lua_State *L) {
  pid_t pid = getpid();
  sds procPath = sdscatfmt(sdsempty(), "/proc/%i/cmdline", (int)pid);
  FILE *procFile = fopen(procPath, "r");
  sdsfree(procPath);
  if (procFile) {
    sds procData = sdsempty();
    char data[64];
    int read;
    while ((read = fread(data, 1, sizeof(data), procFile))) {
      procData = sdscatlen(procData, data, read);
    }
    lua_pushstring(L, procData);
    sdsfree(procData);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static const luaL_Reg lovrAndroid[] = {
  { "getApplicationId", l_lovrGetApplicationId },
};

LOVR_API int luaopen_lovr_android(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, lovrAndroid);
  return 1;
}