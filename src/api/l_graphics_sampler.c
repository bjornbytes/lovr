#include "api.h"
#include "graphics/graphics.h"
#include "util.h"
#include <lua.h>
#include <lauxlib.h>

static int l_lovrSamplerGetFilter(lua_State* L) {
  Sampler* sampler = luax_checktype(L, 1, Sampler);
  const SamplerInfo* info = lovrSamplerGetInfo(sampler);
  luax_pushenum(L, FilterMode, info->min);
  luax_pushenum(L, FilterMode, info->mag);
  luax_pushenum(L, FilterMode, info->mip);
  return 3;
}

static int l_lovrSamplerGetWrap(lua_State* L) {
  Sampler* sampler = luax_checktype(L, 1, Sampler);
  const SamplerInfo* info = lovrSamplerGetInfo(sampler);
  luax_pushenum(L, WrapMode, info->wrap[0]);
  luax_pushenum(L, WrapMode, info->wrap[1]);
  luax_pushenum(L, WrapMode, info->wrap[2]);
  return 3;
}

static int l_lovrSamplerGetCompareMode(lua_State* L) {
  Sampler* sampler = luax_checktype(L, 1, Sampler);
  const SamplerInfo* info = lovrSamplerGetInfo(sampler);
  luax_pushenum(L, CompareMode, info->compare);
  return 1;
}

static int l_lovrSamplerGetAnisotropy(lua_State* L) {
  Sampler* sampler = luax_checktype(L, 1, Sampler);
  const SamplerInfo* info = lovrSamplerGetInfo(sampler);
  lua_pushnumber(L, info->anisotropy);
  return 1;
}

static int l_lovrSamplerGetMipmapRange(lua_State* L) {
  Sampler* sampler = luax_checktype(L, 1, Sampler);
  const SamplerInfo* info = lovrSamplerGetInfo(sampler);
  lua_pushnumber(L, info->range[0]);
  lua_pushnumber(L, info->range[1]);
  return 2;
}

const luaL_Reg lovrSampler[] = {
  { "getFilter", l_lovrSamplerGetFilter },
  { "getWrap", l_lovrSamplerGetWrap },
  { "getCompareMode", l_lovrSamplerGetCompareMode },
  { "getAnisotropy", l_lovrSamplerGetAnisotropy },
  { "getMipmapRange", l_lovrSamplerGetMipmapRange },
  { NULL, NULL }
};
