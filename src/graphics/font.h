#include "util.h"
#include "graphics/texture.h"
#include "vendor/map/map.h"
#include "vendor/vec/vec.h"
#include <stdint.h>

#pragma once

typedef struct {
  void* rasterizer;
  int size;
  int height;
  int ascent;
  int descent;
} FontData;

typedef struct {
  int x;
  int y;
  int w;
  int h;
  int dx;
  int dy;
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
void lovrFontPrint(Font* font, const char* str);
int lovrFontGetHeight(Font* font);
int lovrFontGetAscent(Font* font);
int lovrFontGetDescent(Font* font);
Glyph* lovrFontGetGlyph(Font* font, char character);
void lovrFontAddGlyph(Font* font, Glyph* glyph);
void lovrFontExpandTexture(Font* font);
