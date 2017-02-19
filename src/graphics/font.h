#include "loaders/font.h"
#include "util.h"
#include "graphics/texture.h"
#include "vendor/map/map.h"
#include "vendor/vec/vec.h"
#include <stdint.h>

#pragma once

typedef struct {
  int x;
  int y;
  int width;
  int height;
  int rowHeight;
  int padding;
  map_glyph_t glyphs;
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
void lovrFontPrint(Font* font, const char* str, float x, float y, float z, float w, float h, float angle, float ax, float ay, float az);
float lovrFontGetLineHeight(Font* font);
void lovrFontSetLineHeight(Font* font, float lineHeight);
int lovrFontGetKerning(Font* font, unsigned int a, unsigned int b);
Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint);
void lovrFontAddGlyph(Font* font, Glyph* glyph);
void lovrFontExpandTexture(Font* font);
