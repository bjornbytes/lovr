#include "luax.h"
#include "print_override.h"
#include "platform.h"
#include "lib/sds/sds.h"

// THIS FUNCTION IS SUBSTANTIALLY BASED ON luaB_print FROM LUA SOURCE AND IS LIKELY LICENSE ENCUMBERED
int lovr_luaB_print_override (lua_State *L) {
  sds str = sdsempty();
  int n = lua_gettop(L);  /* number of arguments */
  int i;

  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
                           LUA_QL("print"));
    if (i>1) str = sdscat(str, "\t");
    str = sdscat(str, s);
    lua_pop(L, 1);  /* pop result */
  }
  lovrLog("%s", str);

  sdsfree(str);
  return 0;
}
