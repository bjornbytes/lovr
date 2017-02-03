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
  if (FT_New_Memory_Face(ft, data, size, 0, (FT_Face*)&fontData->rasterizer)) {
    error("Error loading font");
  }

  if (FT_Set_Pixel_Sizes(fontData->rasterizer, 0, 64)) {
    error("Problem setting font size");
  }

  return fontData;
}

GlyphData* lovrFontDataCreateGlyph(FontData* fontData, uint32_t character) {
  FT_Face face = fontData->rasterizer;
  FT_Error err = FT_Err_Ok;
  FT_Glyph ftGlyph;
  FT_Bitmap ftBitmap;
  FT_BitmapGlyph ftBitmapGlyph;

  err |= FT_Load_Glyph(face, FT_Get_Char_Index(face, character), FT_LOAD_DEFAULT);
  err |= FT_Get_Glyph(face->glyph, &ftGlyph);
  err |= FT_Glyph_To_Bitmap(&ftGlyph, FT_RENDER_MODE_NORMAL, 0, 1);

  if (err) {
    error("Error loading glyph\n");
  }

  ftBitmapGlyph = (FT_BitmapGlyph) ftGlyph;
  ftBitmap = ftBitmapGlyph->bitmap;

  GlyphData* glyphData = malloc(sizeof(GlyphData));
  FT_Glyph_Metrics* metrics = &face->glyph->metrics;
  glyphData->x = metrics->horiBearingX >> 6;
  glyphData->y = metrics->horiBearingY >> 6;
  glyphData->w = metrics->width >> 6;
  glyphData->h = metrics->height >> 6;
  glyphData->advance = metrics->horiAdvance >> 6;
  glyphData->data = malloc(2 * glyphData->w * glyphData->h);

  int i = 0;
  uint8_t* row = ftBitmap.buffer;
  for (int y = 0; y < glyphData->h; y++) {
    for (int x = 0; x < glyphData->w; x++) {
      glyphData->data[i++] = 0xff;
      glyphData->data[i++] = row[x];
    }

    row += ftBitmap.pitch;
  }

  FT_Done_Glyph(ftGlyph);
  return glyphData;
}
