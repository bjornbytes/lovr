#pragma once

// A helper, currently used only by the Android platform, that acts like print() but directs to lovrLog
int lovr_luaB_print_override (lua_State *L);
