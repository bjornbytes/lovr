#include "loaders/font.h"
#include "util.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

static FT_Library ft = NULL;

FontData* lovrFontDataCreate(void* data, int size) {
  if (!ft && FT_Init_FreeType(&ft)) {
    error("Error initializing FreeType");
  }

  FontData* fontData = malloc(sizeof(FontData));
  FT_Face face = fontData->face;
  if (FT_New_Memory_Face(ft, data, size, 0, &face)) {
    error("Error loading font");
  }

  if (FT_Set_Pixel_Sizes(face, 16, 16)) {
    error("Problem setting font size");
  }

  return fontData;
}

void lovrFontDataGetGlyph(FontData* fontData, char c, Glyph* glyph) {
  FT_Glyph ftglyph;
  FT_Face face = fontData->face;
  FT_Load_Glyph(face, c, FT_LOAD_DEFAULT);
  FT_Get_Glyph(face->glyph, &ftglyph);

  //
}
