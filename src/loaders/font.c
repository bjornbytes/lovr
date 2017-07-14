#include "loaders/font.h"
#include "data/Cabin.ttf.h"
#include "util.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

static FT_Library ft = NULL;

FontData* lovrFontDataCreate(Blob* blob, int size) {
  if (!ft && FT_Init_FreeType(&ft)) {
    error("Error initializing FreeType");
  }

  FT_Face face = NULL;
  FT_Error err = FT_Err_Ok;
  if (blob) {
    err = err || FT_New_Memory_Face(ft, blob->data, blob->size, 0, &face);
    lovrRetain(&blob->ref);
  } else {
    err = err || FT_New_Memory_Face(ft, Cabin_ttf, Cabin_ttf_len, 0, &face);
  }

  err = err || FT_Set_Pixel_Sizes(face, 0, size);

  if (err) {
    error("Problem loading font\n");
  }

  FontData* fontData = malloc(sizeof(FontData));
  fontData->rasterizer = face;
  fontData->blob = blob;
  fontData->size = size;

  FT_Size_Metrics metrics = face->size->metrics;
  fontData->height = metrics.height >> 6;
  fontData->ascent = metrics.ascender >> 6;
  fontData->descent = metrics.descender >> 6;

  return fontData;
}

void lovrFontDataDestroy(FontData* fontData) {
  if (fontData->blob) {
    lovrRelease(&fontData->blob->ref);
  }
  FT_Done_Face(fontData->rasterizer);
  free(fontData);
}

void lovrFontDataLoadGlyph(FontData* fontData, uint32_t character, Glyph* glyph) {
  FT_Face face = fontData->rasterizer;
  FT_Error err = FT_Err_Ok;
  FT_Glyph slot;
  FT_Bitmap bitmap;
  FT_BitmapGlyph bmglyph;
  FT_Glyph_Metrics* metrics;

  // FreeType
  err = err || FT_Load_Glyph(face, FT_Get_Char_Index(face, character), FT_LOAD_DEFAULT);
  err = err || FT_Get_Glyph(face->glyph, &slot);
  err = err || FT_Glyph_To_Bitmap(&slot, FT_RENDER_MODE_NORMAL, 0, 1);

  if (err) {
    error("Error loading glyph\n");
  }

  bmglyph = (FT_BitmapGlyph) slot;
  bitmap = bmglyph->bitmap;
  metrics = &face->glyph->metrics;

  // Initialize glyph
  glyph->x = 0;
  glyph->y = 0;
  glyph->w = metrics->width >> 6;
  glyph->h = metrics->height >> 6;
  glyph->dx = metrics->horiBearingX >> 6;
  glyph->dy = metrics->horiBearingY >> 6;
  glyph->advance = metrics->horiAdvance >> 6;
  glyph->data = malloc(glyph->w * glyph->h * 2 * sizeof(uint8_t));

  // Transform data into an OpenGL-friendly format
  int i = 0;
  uint8_t* row = bitmap.buffer;
  for (int y = 0; y < glyph->h; y++) {
    for (int x = 0; x < glyph->w; x++) {
      glyph->data[i++] = 0xff;
      glyph->data[i++] = row[x];
    }
    row += bitmap.pitch;
  }

  FT_Done_Glyph(slot);
}

int lovrFontDataGetKerning(FontData* fontData, uint32_t left, uint32_t right) {
  FT_Face face = fontData->rasterizer;
  FT_Vector kerning;
  left = FT_Get_Char_Index(face, left);
  right = FT_Get_Char_Index(face, right);
  FT_Get_Kerning(face, left, right, FT_KERNING_DEFAULT, &kerning);
  return kerning.x >> 6;
}
