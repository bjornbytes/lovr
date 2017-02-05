#include "util.h"
#include "graphics/texture.h"
#include "vendor/map/map.h"
#include "vendor/vec/vec.h"
#include <stdint.h>

#pragma once

typedef struct {
  void* rasterizer;
} FontData;

typedef struct {
  int x;
  int y;
  int ox;
  int oy;
  int w;
  int h;
  int advance;
  uint8_t* data;
} Glyph;

typedef map_t(Glyph) vec_glyph_t;

typedef struct {
  int x;
  int y;
  int width;
  int height;
  int rowHeight;
  int offsetX;
  int extentX;
  int extentY;
  int padding;
  vec_glyph_t glyphs;
} FontAtlas;

typedef struct {
  Ref ref;
  FontData* fontData;
  Texture* texture;
  FontAtlas atlas;
  vec_float_t vertices;
} Font;

Font* lovrFontCreate(FontData* fontData);
void lovrFontDestroy(const Ref* ref);
void lovrFontDataDestroy(FontData* fontData);
void lovrFontPrint(Font* font, const char* str);
Glyph* lovrFontGetGlyph(Font* font, char character);
void lovrFontCreateTexture(Font* font);
