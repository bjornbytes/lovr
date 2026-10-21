#include "api.h"
#include "headset/headset.h"
#include "data/image.h"
#include "graphics/graphics.h"
#include "core/maf.h"
#include "util.h"

static int l_lovrWindowIsOpen(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool open = lovrWindowIsOpen(window);
  lua_pushboolean(L, open);
  return 1;
}

static int l_lovrWindowClose(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  lovrWindowClose(window);
  return 0;
}

static int l_lovrWindowIsVisible(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool visible = lovrWindowIsVisible(window);
  lua_pushboolean(L, visible);
  return 1;
}

static int l_lovrWindowSetVisible(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool visible = lua_toboolean(L, 2);
  luax_assert(L, lovrWindowSetVisible(window, visible));
  return 0;
}

static int l_lovrWindowIsFocused(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool focused = lovrWindowIsFocused(window);
  lua_pushboolean(L, focused);
  return 1;
}

static int l_lovrWindowIsFullscreen(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool fullscreen = lovrWindowIsFullscreen(window);
  lua_pushboolean(L, fullscreen);
  return 1;
}

static int l_lovrWindowSetFullscreen(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool fullscreen = lua_toboolean(L, 2);
  luax_assert(L, lovrWindowSetFullscreen(window, fullscreen));
  return 0;
}

static int l_lovrWindowGetWidth(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float bounds[3];
  luax_assert(L, lovrWindowGetBounds(window, bounds));
  lua_pushnumber(L, bounds[0]);
  return 1;
}

static int l_lovrWindowGetHeight(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float bounds[3];
  luax_assert(L, lovrWindowGetBounds(window, bounds));
  lua_pushnumber(L, bounds[1]);
  return 1;
}

static int l_lovrWindowGetDepth(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float bounds[3];
  luax_assert(L, lovrWindowGetBounds(window, bounds));
  lua_pushnumber(L, bounds[2]);
  return 1;
}

static int l_lovrWindowGetDimensions(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float bounds[3];
  luax_assert(L, lovrWindowGetBounds(window, bounds));
  lua_pushnumber(L, bounds[0]);
  lua_pushnumber(L, bounds[1]);
  lua_pushnumber(L, bounds[2]);
  return 3;
}

static int l_lovrWindowGetPosition(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float position[3], orientation[4];
  if (lovrWindowGetPose(window, position, orientation)) {
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
  } else {
    lua_pushnumber(L, 0.);
    lua_pushnumber(L, 0.);
    lua_pushnumber(L, 0.);
  }
  return 3;
}

static int l_lovrWindowGetOrientation(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float position[3], orientation[4], angle, ax, ay, az;
  if (lovrWindowGetPose(window, position, orientation)) {
    quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  } else {
    lua_pushnumber(L, 0.);
    lua_pushnumber(L, 0.);
    lua_pushnumber(L, 0.);
    lua_pushnumber(L, 0.);
  }
  return 4;
}

static int l_lovrWindowGetPose(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float position[3], orientation[4], angle, ax, ay, az;
  if (lovrWindowGetPose(window, position, orientation)) {
    lua_pushnumber(L, position[0]);
    lua_pushnumber(L, position[1]);
    lua_pushnumber(L, position[2]);
    quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
    lua_pushnumber(L, angle);
    lua_pushnumber(L, ax);
    lua_pushnumber(L, ay);
    lua_pushnumber(L, az);
  } else {
    for (int i = 0; i < 7; i++) {
      lua_pushnumber(L, 0.);
    }
  }
  return 7;
}

static int l_lovrWindowGetViewPose(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float position[3], orientation[4];
  uint32_t view = luax_checku32(L, 2) - 1;
  if (!lovrWindowGetViewPose(window, view, position, orientation)) {
    lua_pushnil(L);
    return 1;
  }
  float angle, ax, ay, az;
  quat_getAngleAxis(orientation, &angle, &ax, &ay, &az);
  lua_pushnumber(L, position[0]);
  lua_pushnumber(L, position[1]);
  lua_pushnumber(L, position[2]);
  lua_pushnumber(L, angle);
  lua_pushnumber(L, ax);
  lua_pushnumber(L, ay);
  lua_pushnumber(L, az);
  return 7;
}

static int l_lovrWindowGetViewAngles(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  float left, right, up, down;
  uint32_t view = luax_checku32(L, 2) - 1;
  if (!lovrWindowGetViewAngles(window, view, &left, &right, &up, &down)) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushnumber(L, left);
  lua_pushnumber(L, right);
  lua_pushnumber(L, up);
  lua_pushnumber(L, down);
  return 4;
}

static int l_lovrWindowGetTexture(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  Texture* texture = NULL;
  luax_assert(L, lovrWindowGetTexture(window, &texture));
  luax_pushtype(L, Texture, texture);
  return 1;
}

static int l_lovrWindowGetPass(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  Pass* pass = NULL;
  luax_assert(L, lovrWindowGetPass(window, &pass));
  luax_pushtype(L, Pass, pass);
  return 1;
}

int l_lovrWindowSetBackground(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t layers = 0;
  Image* images[6];
  uint32_t imageCount = 0;
  Texture* texture = NULL;

  if (lua_isnoneornil(L, 2)) {
    lovrWindowSetBackground(window, 0, 0, 0);
    return 0;
  } else if ((texture = luax_totype(L, 2, Texture)) != NULL) {
    const TextureInfo* info = lovrTextureGetInfo(texture);
    width = info->width;
    height = info->height;
    layers = info->layers;
  } else {
    luax_checkimages(L, 2, images, COUNTOF(images), &imageCount, &layers);
    luax_check(L, imageCount > 1, "Must have at least 1 image");
    width = lovrImageGetWidth(images[0], 0);
    height = lovrImageGetHeight(images[0], 0);
  }

  luax_check(L, layers == 1 || layers == 6, "Currently, background must have 1 or 6 layers");

  Texture* background = lovrWindowSetBackground(window, width, height, layers);

  if (!background) {
    for (uint32_t i = 0; i < imageCount; i++) {
      lovrRelease(images[i], lovrImageDestroy);
    }
    luax_throw(L);
  }

  if (texture) {
    uint32_t srcOffset[4] = { 0 };
    uint32_t dstOffset[4] = { 0 };
    uint32_t extent[3] = { width, height, layers };
    luax_assert(L, lovrTextureCopy(texture, background, srcOffset, dstOffset, extent));
  } else if (imageCount > 0) {
    for (uint32_t i = 0; i < imageCount; i++) {
      uint32_t texOffset[4] = { 0, 0, i, 0 };
      uint32_t imgOffset[4] = { 0, 0, 0, 0 };
      uint32_t extent[3] = { width, height, lovrImageGetLayerCount(images[i]) };
      luax_assert(L, lovrTextureSetPixels(background, images[i], texOffset, imgOffset, extent));
      lovrRelease(images[i], lovrImageDestroy);
    }
  }

  return 0;
}

int l_lovrWindowGetLayers(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  bool main;
  uint32_t count;
  Layer** layers = lovrWindowGetLayers(window, &count, &main);
  lua_createtable(L, (int) count, 0);
  for (uint32_t i = 0; i < count; i++) {
    luax_pushtype(L, Layer, layers[i]);
    lua_rawseti(L, -2, (int) i + 1);
  }
  lua_pushboolean(L, main);
  lua_setfield(L, -2, "main");
  return 1;
}

int l_lovrWindowSetLayers(lua_State* L) {
  Window* window = luax_checktype(L, 1, Window);
  Layer* layers[MAX_LAYERS];
  uint32_t count = 0;
  bool main = true;
  if (lua_type(L, 2) == LUA_TTABLE) {
    count = luax_len(L, 2);
    luax_check(L, count <= MAX_LAYERS, "Too many layers (max is %d)", MAX_LAYERS);
    for (uint32_t i = 0; i < count; i++) {
      lua_rawgeti(L, 2, (int) i + 1);
      layers[i] = luax_checktype(L, -1, Layer);
      lua_pop(L, 1);
    }
    lua_getfield(L, 2, "main");
    if (!lua_isnil(L, -1)) main = lua_toboolean(L, -1);
    lua_pop(L, 1);
  } else {
    count = lua_gettop(L) - 1;
    luax_check(L, count <= MAX_LAYERS, "Too many layers (max is %d)", MAX_LAYERS);
    for (uint32_t i = 0; i < count; i++) {
      layers[i] = luax_checktype(L, (int) i + 2, Layer);
    }
  }
  bool success = lovrWindowSetLayers(window, layers, count, main);
  luax_assert(L, success);
  return 0;
}

const luaL_Reg lovrWindow[] = {
  { "isOpen", l_lovrWindowIsOpen },
  { "close", l_lovrWindowClose },
  { "isVisible", l_lovrWindowIsVisible },
  { "setVisible", l_lovrWindowSetVisible },
  { "isFocused", l_lovrWindowIsFocused },
  { "isFullscreen", l_lovrWindowIsFullscreen },
  { "setFullscreen", l_lovrWindowSetFullscreen },
  { "getWidth", l_lovrWindowGetWidth },
  { "getHeight", l_lovrWindowGetHeight },
  { "getDepth", l_lovrWindowGetDepth },
  { "getDimensions", l_lovrWindowGetDimensions },
  { "getPosition", l_lovrWindowGetPosition },
  { "getOrientation", l_lovrWindowGetOrientation },
  { "getPose", l_lovrWindowGetPose },
  { "getViewPose", l_lovrWindowGetViewPose },
  { "getViewAngles", l_lovrWindowGetViewAngles },
  { "getTexture", l_lovrWindowGetTexture },
  { "getPass", l_lovrWindowGetPass },
  { "setBackground", l_lovrWindowSetBackground },
  { "getLayers", l_lovrWindowGetLayers },
  { "setLayers", l_lovrWindowSetLayers },
  { NULL, NULL }
};
