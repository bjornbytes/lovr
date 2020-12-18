#include "api.h"
#include "graphics/graphics.h"
#include "data/blob.h"
#include "data/image.h"
#include "core/os.h"
#include "core/util.h"
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

StringEntry lovrBufferUsage[] = {
  [BUFFER_VERTEX] = ENTRY("vertex"),
  [BUFFER_INDEX] = ENTRY("index"),
  [BUFFER_UNIFORM] = ENTRY("uniform"),
  [BUFFER_COMPUTE] = ENTRY("compute"),
  [BUFFER_ARGUMENT] = ENTRY("argument"),
  [BUFFER_UPLOAD] = ENTRY("upload"),
  [BUFFER_DOWNLOAD] = ENTRY("download"),
  { 0 }
};

StringEntry lovrTextureType[] = {
  [TEXTURE_2D] = ENTRY("2d"),
  [TEXTURE_CUBE] = ENTRY("cube"),
  [TEXTURE_VOLUME] = ENTRY("volume"),
  [TEXTURE_ARRAY] = ENTRY("array"),
  { 0 }
};

StringEntry lovrTextureUsage[] = {
  [TEXTURE_SAMPLE] = ENTRY("sample"),
  [TEXTURE_RENDER] = ENTRY("render"),
  [TEXTURE_COMPUTE] = ENTRY("compute"),
  [TEXTURE_UPLOAD] = ENTRY("upload"),
  [TEXTURE_DOWNLOAD] = ENTRY("download"),
  { 0 }
};

// Must be released when done
static Image* luax_checkimage(lua_State* L, int index, bool flip) {
  Image* image = luax_totype(L, index, Image);

  if (image) {
    lovrRetain(image);
  } else {
    Blob* blob = luax_readblob(L, index, "Texture");
    image = lovrImageCreateFromBlob(blob, flip);
    lovrRelease(blob, lovrBlobDestroy);
  }

  return image;
}

static void luax_readcanvas(lua_State* L, int index, Canvas* canvas) {
  switch (lua_type(L, index)) {
    case LUA_TUSERDATA:
      canvas->color[0].texture = luax_checktype(L, index, Texture);
      canvas->depth.enabled = true;
      canvas->depth.format = FORMAT_D16;
      canvas->depth.load = LOAD_CLEAR;
      canvas->depth.save = SAVE_DISCARD;
      canvas->depth.clear = 1.f;
      break;
    case LUA_TTABLE: {
      int count = luax_len(L, index);
      for (int i = 0; i < count; i++) {
        lua_rawgeti(L, index, i + 1);
        canvas->color[i].texture = luax_totype(L, -1, Texture);
        lovrAssert(canvas->color[i].texture, "The numeric keys of a render target table must be Textures");

      }
      break;
    }
    default:
      luaL_argerror(L, index, "Expected a Texture or table");
  }
}

static int l_lovrGraphicsCreateWindow(lua_State* L) {
  os_window_config window;
  memset(&window, 0, sizeof(window));

  if (!lua_toboolean(L, 1)) {
    return 0;
  }

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "width");
  window.width = luaL_optinteger(L, -1, 1080);
  lua_pop(L, 1);

  lua_getfield(L, 1, "height");
  window.height = luaL_optinteger(L, -1, 600);
  lua_pop(L, 1);

  lua_getfield(L, 1, "fullscreen");
  window.fullscreen = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "resizable");
  window.resizable = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "msaa");
  window.msaa = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "title");
  window.title = luaL_optstring(L, -1, "LÖVR");
  lua_pop(L, 1);

  lua_getfield(L, 1, "icon");
  Image* image = NULL;
  if (!lua_isnil(L, -1)) {
    image = luax_checkimage(L, -1, false);
    window.icon.data = image->blob->data;
    window.icon.width = image->width;
    window.icon.height = image->height;
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "vsync");
  window.vsync = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lovrGraphicsCreateWindow(&window);
  luax_atexit(L, lovrGraphicsDestroy); // The lua_State that creates the window shall be the one to destroy it
  lovrRelease(image, lovrImageDestroy);
  return 0;
}

static int l_lovrGraphicsHasWindow(lua_State* L) {
  bool window = lovrGraphicsHasWindow();
  lua_pushboolean(L, window);
  return 1;
}

static int l_lovrGraphicsGetWidth(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  return 1;
}

static int l_lovrGraphicsGetHeight(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 1;
}

static int l_lovrGraphicsGetDimensions(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetWidth());
  lua_pushnumber(L, lovrGraphicsGetHeight());
  return 2;
}

static int l_lovrGraphicsGetPixelDensity(lua_State* L) {
  lua_pushnumber(L, lovrGraphicsGetPixelDensity());
  return 1;
}

static int l_lovrGraphicsGetFeatures(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  GraphicsFeatures features;
  lovrGraphicsGetFeatures(&features);

  lua_pushboolean(L, features.bptc);
  lua_setfield(L, -2, "bptc");
  lua_pushboolean(L, features.astc);
  lua_setfield(L, -2, "astc");
  lua_pushboolean(L, features.pointSize);
  lua_setfield(L, -2, "pointSize");
  lua_pushboolean(L, features.wireframe);
  lua_setfield(L, -2, "wireframe");
  lua_pushboolean(L, features.anisotropy);
  lua_setfield(L, -2, "anisotropy");
  lua_pushboolean(L, features.clipDistance);
  lua_setfield(L, -2, "clipDistance");
  lua_pushboolean(L, features.cullDistance);
  lua_setfield(L, -2, "cullDistance");
  lua_pushboolean(L, features.fullIndexBufferRange);
  lua_setfield(L, -2, "fullIndexBufferRange");
  lua_pushboolean(L, features.indirectDrawCount);
  lua_setfield(L, -2, "indirectDrawCount");
  lua_pushboolean(L, features.indirectDrawFirstInstance);
  lua_setfield(L, -2, "indirectDrawFirstInstance");
  lua_pushboolean(L, features.extraShaderInputs);
  lua_setfield(L, -2, "extraShaderInputs");
  lua_pushboolean(L, features.multiview);
  lua_setfield(L, -2, "multiview");
  return 1;
}

static int l_lovrGraphicsGetLimits(lua_State* L) {
  if (lua_istable(L, 1)) {
    lua_settop(L, 1);
  } else {
    lua_newtable(L);
  }

  GraphicsLimits limits;
  lovrGraphicsGetLimits(&limits);

  lua_pushinteger(L, limits.textureSize2D);
  lua_setfield(L, -2, "textureSize2D");

  lua_pushinteger(L, limits.textureSize3D);
  lua_setfield(L, -2, "textureSize3D");

  lua_pushinteger(L, limits.textureSizeCube);
  lua_setfield(L, -2, "textureSizeCube");

  lua_pushinteger(L, limits.textureLayers);
  lua_setfield(L, -2, "textureLayers");

  lua_createtable(L, 2, 0);
  lua_pushinteger(L, limits.renderSize[0]);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.renderSize[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "renderSize");

  lua_pushinteger(L, limits.renderViews);
  lua_setfield(L, -2, "renderViews");

  lua_pushinteger(L, limits.bundleCount);
  lua_setfield(L, -2, "bundleCount");

  lua_pushinteger(L, limits.bundleSlots);
  lua_setfield(L, -2, "bundleSlots");

  lua_pushinteger(L, limits.uniformBufferRange);
  lua_setfield(L, -2, "uniformBufferRange");

  lua_pushinteger(L, limits.storageBufferRange);
  lua_setfield(L, -2, "storageBufferRange");

  lua_pushinteger(L, limits.uniformBufferAlign);
  lua_setfield(L, -2, "uniformBufferAlign");

  lua_pushinteger(L, limits.storageBufferAlign);
  lua_setfield(L, -2, "storageBufferAlign");

  lua_pushinteger(L, limits.vertexAttributes);
  lua_setfield(L, -2, "vertexAttributes");

  lua_pushinteger(L, limits.vertexAttributeOffset);
  lua_setfield(L, -2, "vertexAttributeOffset");

  lua_pushinteger(L, limits.vertexBuffers);
  lua_setfield(L, -2, "vertexBuffers");

  lua_pushinteger(L, limits.vertexBufferStride);
  lua_setfield(L, -2, "vertexBufferStride");

  lua_pushinteger(L, limits.vertexShaderOutputs);
  lua_setfield(L, -2, "vertexShaderOutputs");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.computeCount[0]);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.computeCount[1]);
  lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.computeCount[2]);
  lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "computeCount");

  lua_createtable(L, 3, 0);
  lua_pushinteger(L, limits.computeGroupSize[0]);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.computeGroupSize[1]);
  lua_rawseti(L, -2, 2);
  lua_pushinteger(L, limits.computeGroupSize[2]);
  lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, "computeGroupSize");

  lua_pushinteger(L, limits.computeGroupVolume);
  lua_setfield(L, -2, "computeGroupVolume");

  lua_pushinteger(L, limits.computeSharedMemory);
  lua_setfield(L, -2, "computeSharedMemory");

  lua_pushinteger(L, limits.indirectDrawCount);
  lua_setfield(L, -2, "indirectDrawCount");

  lua_pushinteger(L, limits.allocationSize);
  lua_setfield(L, -2, "allocationSize");

  lua_pushinteger(L, limits.pointSize[0]);
  lua_setfield(L, -2, "allocationSize");

  lua_createtable(L, 2, 0);
  lua_pushinteger(L, limits.pointSize[0]);
  lua_rawseti(L, -2, 1);
  lua_pushinteger(L, limits.pointSize[1]);
  lua_rawseti(L, -2, 2);
  lua_setfield(L, -2, "pointSize");

  lua_pushnumber(L, limits.anisotropy);
  lua_setfield(L, -2, "anisotropy");
  return 1;
}

static int l_lovrGraphicsBegin(lua_State* L) {
  lovrGraphicsBegin();
  return 0;
}

static int l_lovrGraphicsFlush(lua_State* L) {
  lovrGraphicsFlush();
  return 0;
}

static int l_lovrGraphicsRender(lua_State* L) {
  Canvas canvas;
  memset(&canvas, 0, sizeof(canvas));
  luax_readcanvas(L, 1, &canvas);
  lovrGraphicsRender(&canvas);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_settop(L, 2);
  lua_call(L, 0, 0);
  lovrGraphicsEndPass();
  return 0;
}

static int l_lovrGraphicsCompute(lua_State* L) {
  lovrGraphicsCompute();
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_settop(L, 1);
  lua_call(L, 0, 0);
  return 0;
}

static int l_lovrGraphicsNewBuffer(lua_State* L) {
  BufferInfo info = {
    .usage = ~0u
  };

  info.size = luaL_checkinteger(L, 1);

  if (lua_istable(L, 2)) {
    lua_getfield(L, 2, "usage");
    switch (lua_type(L, -1)) {
      case LUA_TSTRING:
        info.usage = 1 << luax_checkenum(L, -1, BufferUsage, NULL);
        break;
      case LUA_TTABLE:
        info.usage = 0;
        int length = luax_len(L, -1);
        for (int i = 0; i < length; i++) {
          lua_rawgeti(L, -1, i + 1);
          info.usage |= 1 << luax_checkenum(L, -1, BufferUsage, NULL);
          lua_pop(L, 1);
        }
        break;
      case LUA_TNIL:
        break;
      default:
        return luaL_error(L, "Buffer usage flags must be a string or a table of strings");
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  Buffer* buffer = lovrBufferCreate(&info);
  luax_pushtype(L, Buffer, buffer);
  lovrRelease(buffer, lovrBufferDestroy);
  return 1;
}

static int l_lovrGraphicsNewTexture(lua_State* L) {
  Texture* source = luax_totype(L, 1, Texture);

  if (source) {
    TextureView view = { .source = source };
    view.type = luax_checkenum(L, 2, TextureType, NULL);
    view.layerIndex = luaL_optinteger(L, 3, 1) - 1;
    view.layerCount = luaL_optinteger(L, 4, 1);
    view.mipmapIndex = luaL_optinteger(L, 5, 1) - 1;
    view.mipmapCount = luaL_optinteger(L, 6, 0);
    Texture* texture = lovrTextureCreateView(&view);
    luax_pushtype(L, Texture, texture);
    lovrRelease(texture, lovrTextureDestroy);
    return 1;
  }

  int index = 1;
  int argType = lua_type(L, index);
  bool blank = argType == LUA_TNUMBER;

  TextureInfo info = {
    .type = TEXTURE_2D,
    .format = FORMAT_RGBA8,
    .mipmaps = ~0u,
    .samples = 1,
    .usage = ~0u,
    .srgb = !blank
  };

  if (blank) {
    info.size[0] = lua_tointeger(L, index++);
    info.size[1] = luaL_checkinteger(L, index++);
    info.size[2] = lua_type(L, index) == LUA_TNUMBER ? lua_tointeger(L, index++) : 0;
  } else if (argType != LUA_TTABLE) {
    lua_createtable(L, 1, 0);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);
    lua_replace(L, 1);
    info.size[2] = 1;
    index++;
  } else {
    info.size[2] = luax_len(L, index++);
    info.type = info.size[2] > 0 ? TEXTURE_ARRAY : TEXTURE_CUBE;
  }

  if (lua_istable(L, index)) {
    lua_getfield(L, index, "linear");
    info.srgb = lua_isnil(L, -1) ? info.srgb : !lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "mipmaps");
    if (lua_type(L, -1) == LUA_TNUMBER) {
      info.mipmaps = lua_tonumber(L, -1);
    } else {
      info.mipmaps = (lua_isnil(L, -1) || lua_toboolean(L, -1)) ? ~0u : 1;
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "type");
    info.type = lua_isnil(L, -1) ? info.type : luax_checkenum(L, -1, TextureType, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "format");
    info.format = lua_isnil(L, -1) ? info.format : luax_checkenum(L, -1, TextureFormat, NULL);
    lua_pop(L, 1);

    lua_getfield(L, index, "samples");
    info.samples = lua_isnil(L, -1) ? info.samples : luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, "usage");
    switch (lua_type(L, -1)) {
      case LUA_TSTRING:
        info.usage = 1 << luax_checkenum(L, -1, TextureUsage, NULL);
        break;
      case LUA_TTABLE:
        info.usage = 0;
        int length = luax_len(L, -1);
        for (int i = 0; i < length; i++) {
          lua_rawgeti(L, -1, i + 1);
          info.usage |= 1 << luax_checkenum(L, -1, TextureUsage, NULL);
          lua_pop(L, 1);
        }
        break;
      case LUA_TNIL:
        break;
      default:
        return luaL_error(L, "Texture usage flags must be a string or a table of strings");
    }
    lua_pop(L, 1);

    lua_getfield(L, index, "label");
    info.label = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  Texture* texture;

  if (blank) {
    info.size[2] = info.size[2] > 0 ? info.size[2] : (info.type == TEXTURE_CUBE ? 6 : 1);
    texture = lovrTextureCreate(&info);
  } else {
    if (info.type == TEXTURE_CUBE && info.size[2] == 0) {
      info.size[2] = 6;
      const char* faces[6] = { "right", "left", "top", "bottom", "back", "front" };
      for (int i = 0; i < 6; i++) {
        lua_pushstring(L, faces[i]);
        lua_rawget(L, 1);
        lovrAssert(!lua_isnil(L, -1), "Could not load cubemap texture: missing '%s' face", faces[i]);
        lua_rawseti(L, 1, i + 1);
      }
    }

    lovrAssert(info.size[2] > 0, "No texture images specified");

    for (uint32_t i = 0; i < info.size[2]; i++) {
      lua_rawgeti(L, 1, i + 1);
      Image* image = luax_checkimage(L, -1, info.type != TEXTURE_CUBE);
      if (i == 0) {
        info.size[0] = image->width;
        info.size[1] = image->height;
        info.format = image->format;
        texture = lovrTextureCreate(&info);
      }
      //lovrTextureReplacePixels(texture, image, 0, 0, i, 0);
      lovrRelease(image, lovrImageDestroy);
      lua_pop(L, 1);
    }
  }

  luax_pushtype(L, Texture, texture);
  lovrRelease(texture, lovrTextureDestroy);
  return 1;
}

static const luaL_Reg lovrGraphics[] = {
  { "createWindow", l_lovrGraphicsCreateWindow },
  { "hasWindow", l_lovrGraphicsHasWindow },
  { "getWidth", l_lovrGraphicsGetWidth },
  { "getHeight", l_lovrGraphicsGetHeight },
  { "getDimensions", l_lovrGraphicsGetDimensions },
  { "getPixelDensity", l_lovrGraphicsGetPixelDensity },
  { "getFeatures", l_lovrGraphicsGetFeatures },
  { "getLimits", l_lovrGraphicsGetLimits },
  { "begin", l_lovrGraphicsBegin },
  { "flush", l_lovrGraphicsFlush },
  { "render", l_lovrGraphicsRender },
  { "compute", l_lovrGraphicsCompute },
  { "newBuffer", l_lovrGraphicsNewBuffer },
  { "newTexture", l_lovrGraphicsNewTexture },
  { NULL, NULL }
};

extern const luaL_Reg lovrBuffer[];
extern const luaL_Reg lovrTexture[];

int luaopen_lovr_graphics(lua_State* L) {
  lua_newtable(L);
  luax_register(L, lovrGraphics);
  luax_registertype(L, Buffer);
  luax_registertype(L, Texture);

  bool debug = false;
  luax_pushconf(L);
  lua_getfield(L, -1, "graphics");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "debug");
    debug = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lovrGraphicsInit(debug);

  lua_pushcfunction(L, l_lovrGraphicsCreateWindow);
  lua_getfield(L, -2, "window");
  lua_call(L, 1, 0);
  lua_pop(L, 1);
  return 1;
}
