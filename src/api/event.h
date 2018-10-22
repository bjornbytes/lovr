#include "event/event.h"

void luax_checkvariant(lua_State* L, int index, Variant* variant);
int luax_pushvariant(lua_State* L, Variant* variant);
