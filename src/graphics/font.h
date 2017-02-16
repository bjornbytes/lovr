#include "util.h"
#include "graphics/texture.h"
#include "math/math.h"
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
  map_int_t kerning;
  vec_float_t vertices;
  int lineHeight;
} Font;

Font* lovrFontCreate(FontData* fontData);
void lovrFontDestroy(const Ref* ref);
void lovrFontPrint(Font* font, const char* str, mat4 transform);
int lovrFontGetWidth(Font* font, const char* str);
int lovrFontGetHeight(Font* font);
int lovrFontGetAscent(Font* font);
int lovrFontGetDescent(Font* font);
int lovrFontGetBaseline(Font* font);
float lovrFontGetLineHeight(Font* font);
void lovrFontSetLineHeight(Font* font, float lineHeight);
int lovrFontGetKerning(Font* font, unsigned int a, unsigned int b);
Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint);
void lovrFontAddGlyph(Font* font, Glyph* glyph);
void lovrFontExpandTexture(Font* font);
