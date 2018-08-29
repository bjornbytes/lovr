#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "data/rasterizer.h"
#include "data/textureData.h"
#include "math/math.h"
#include "util.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

static float* lovrFontAlignLine(float* x, float* lineEnd, float width, HorizontalAlign halign) {
  while(x < lineEnd) {
    if (halign == ALIGN_CENTER) {
      *x -= width / 2.f;
    } else if (halign == ALIGN_RIGHT) {
      *x -= width;
    }

    x += 8;
  }

  return x;
}

Font* lovrFontCreate(Rasterizer* rasterizer) {
  Font* font = lovrAlloc(Font, lovrFontDestroy);
  if (!font) return NULL;

  lovrRetain(rasterizer);
  font->rasterizer = rasterizer;
  font->lineHeight = 1.f;
  font->pixelDensity = (float) font->rasterizer->height;
  map_init(&font->kerning);

  // Atlas
  int padding = 1;
  font->atlas.x = padding;
  font->atlas.y = padding;
  font->atlas.width = 128;
  font->atlas.height = 128;
  font->atlas.padding = padding;
  map_init(&font->atlas.glyphs);

  // Set initial atlas size
  while (font->atlas.height < 4 * rasterizer->size) {
    lovrFontExpandTexture(font);
  }

  // Create the texture
  lovrFontCreateTexture(font);

  return font;
}

void lovrFontDestroy(void* ref) {
  Font* font = ref;
  lovrRelease(font->rasterizer);
  lovrRelease(font->texture);
  const char* key;
  map_iter_t iter = map_iter(&font->atlas.glyphs);
  while ((key = map_next(&font->atlas.glyphs, &iter)) != NULL) {
    Glyph* glyph = map_get(&font->atlas.glyphs, key);
    lovrRelease(glyph->data);
  }
  map_deinit(&font->atlas.glyphs);
  map_deinit(&font->kerning);
  free(font);
}

void lovrFontRender(Font* font, const char* str, float wrap, HorizontalAlign halign, VerticalAlign valign, VertexPointer vertices, float* offsety, uint32_t* vertexCount) {
  FontAtlas* atlas = &font->atlas;

  float cx = 0;
  float cy = -font->rasterizer->height * .8;
  float u = atlas->width;
  float v = atlas->height;
  float scale = 1 / font->pixelDensity;

  int len = strlen(str);
  const char* start = str;
  const char* end = str + len;
  unsigned int previous = '\0';
  unsigned int codepoint;
  size_t bytes;

  float* cursor = vertices.floats;
  float* lineStart = vertices.floats;
  int lineCount = 1;
  *vertexCount = 0;

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {

    // Newlines
    if (codepoint == '\n' || (wrap && cx * scale > wrap && codepoint == ' ')) {
      lineStart = lovrFontAlignLine(lineStart, cursor, cx, halign);
      lineCount++;
      cx = 0;
      cy -= font->rasterizer->height * font->lineHeight;
      previous = '\0';
      str += bytes;
      continue;
    }

    // Kerning
    cx += lovrFontGetKerning(font, previous, codepoint);
    previous = codepoint;

    // Get glyph
    Glyph* glyph = lovrFontGetGlyph(font, codepoint);

    // Start over if texture was repacked
    if (u != atlas->width || v != atlas->height) {
      lovrFontRender(font, start, wrap, halign, valign, vertices, offsety, vertexCount);
      return;
    }

    // Triangles
    if (glyph->w > 0 && glyph->h > 0) {
      float x1 = cx + glyph->dx - GLYPH_PADDING;
      float y1 = cy + glyph->dy + GLYPH_PADDING;
      float x2 = x1 + glyph->tw;
      float y2 = y1 - glyph->th;
      float s1 = glyph->x / u;
      float t1 = (glyph->y + glyph->th) / v;
      float s2 = (glyph->x + glyph->tw) / u;
      float t2 = glyph->y / v;

      float quad[48] = {
        x1, y1, 0, 0, 0, 0, s1, t1,
        x1, y2, 0, 0, 0, 0, s1, t2,
        x2, y1, 0, 0, 0, 0, s2, t1,
        x2, y1, 0, 0, 0, 0, s2, t1,
        x1, y2, 0, 0, 0, 0, s1, t2,
        x2, y2, 0, 0, 0, 0, s2, t2
      };

      memcpy(cursor, quad, 6 * 8 * sizeof(float));
      cursor += 48;
      *vertexCount += 6;
    }

    // Advance cursor
    cx += glyph->advance;
    str += bytes;
  }

  // Align the last line
  lovrFontAlignLine(lineStart, cursor, cx, halign);

  // Calculate vertical offset
  if (valign == ALIGN_MIDDLE) {
    *offsety = lineCount * font->rasterizer->height * font->lineHeight * .5f;
  } else if (valign == ALIGN_BOTTOM) {
    *offsety = lineCount * font->rasterizer->height * font->lineHeight;
  } else {
    *offsety = 0;
  }
}

float lovrFontGetWidth(Font* font, const char* str, float wrap) {
  float width = 0;
  float x = 0;
  const char* end = str + strlen(str);
  size_t bytes;
  unsigned int previous = '\0';
  unsigned int codepoint;
  float scale = 1 / font->pixelDensity;

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
    if (codepoint == '\n' || (wrap && x * scale > wrap && codepoint == ' ')) {
      width = MAX(width, x * scale);
      x = 0;
      previous = '\0';
      str += bytes;
      continue;
    }

    Glyph* glyph = lovrFontGetGlyph(font, codepoint);
    x += glyph->advance + lovrFontGetKerning(font, previous, codepoint);
    previous = codepoint;
    str += bytes;
  }

  return MAX(width, x * scale);
}

float lovrFontGetHeight(Font* font) {
  return font->rasterizer->height / font->pixelDensity;
}

float lovrFontGetAscent(Font* font) {
  return font->rasterizer->ascent / font->pixelDensity;
}

float lovrFontGetDescent(Font* font) {
  return font->rasterizer->descent / font->pixelDensity;
}

float lovrFontGetBaseline(Font* font) {
  return font->rasterizer->height * .8 / font->pixelDensity;
}

float lovrFontGetLineHeight(Font* font) {
  return font->lineHeight;
}

void lovrFontSetLineHeight(Font* font, float lineHeight) {
  font->lineHeight = lineHeight;
}

int lovrFontGetKerning(Font* font, unsigned int left, unsigned int right) {
  char key[12];
  snprintf(key, 12, "%d,%d", left, right);

  int* entry = map_get(&font->kerning, key);
  if (entry) {
    return *entry;
  }

  int kerning = lovrRasterizerGetKerning(font->rasterizer, left, right);
  map_set(&font->kerning, key, kerning);
  return kerning;
}

float lovrFontGetPixelDensity(Font* font) {
  return font->pixelDensity;
}

void lovrFontSetPixelDensity(Font* font, float pixelDensity) {
  if (pixelDensity <= 0) {
    pixelDensity = font->rasterizer->height;
  }

  font->pixelDensity = pixelDensity;
}

Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint) {
  char key[6];
  snprintf(key, 6, "%d", codepoint);

  FontAtlas* atlas = &font->atlas;
  map_glyph_t* glyphs = &atlas->glyphs;
  Glyph* glyph = map_get(glyphs, key);

  // Add the glyph to the atlas if it isn't there
  if (!glyph) {
    Glyph g;
    lovrRasterizerLoadGlyph(font->rasterizer, codepoint, &g);
    map_set(glyphs, key, g);
    glyph = map_get(glyphs, key);
    lovrFontAddGlyph(font, glyph);
  }

  return glyph;
}

void lovrFontAddGlyph(Font* font, Glyph* glyph) {
  FontAtlas* atlas = &font->atlas;

  // Don't waste space on empty glyphs
  if (glyph->w == 0 && glyph->h == 0) {
    return;
  }

  // If the glyph does not fit, you must acquit (new row)
  if (atlas->x + glyph->tw > atlas->width - 2 * atlas->padding) {
    atlas->x = atlas->padding;
    atlas->y += atlas->rowHeight + atlas->padding;
    atlas->rowHeight = 0;
  }

  // Expand the texture if needed. Expanding the texture re-adds all the glyphs, so we can return.
  if (atlas->y + glyph->th > atlas->height - 2 * atlas->padding) {
    lovrFontExpandTexture(font);
    return;
  }

  // Keep track of glyph's position in the atlas
  glyph->x = atlas->x;
  glyph->y = atlas->y;

  // Paste glyph into texture
  lovrTextureReplacePixels(font->texture, glyph->data, atlas->x, atlas->y, 0, 0);

  // Advance atlas cursor
  atlas->x += glyph->tw + atlas->padding;
  atlas->rowHeight = MAX(atlas->rowHeight, glyph->th);
}

void lovrFontExpandTexture(Font* font) {
  FontAtlas* atlas = &font->atlas;

  if (atlas->width == atlas->height) {
    atlas->width *= 2;
  } else {
    atlas->height *= 2;
  }

  if (!font->texture) {
    return;
  }

  // Recreate the texture
  lovrFontCreateTexture(font);

  // Reset the cursor
  atlas->x = atlas->padding;
  atlas->y = atlas->padding;
  atlas->rowHeight = 0;

  // Re-pack all the glyphs
  const char* key;
  map_iter_t iter = map_iter(&atlas->glyphs);
  while ((key = map_next(&atlas->glyphs, &iter)) != NULL) {
    Glyph* glyph = map_get(&atlas->glyphs, key);
    lovrFontAddGlyph(font, glyph);
  }
}

// TODO we only need the TextureData here to clear the texture, but it's a big waste of memory.
// Could look into using glClearTexImage when supported to make this more efficient.
void lovrFontCreateTexture(Font* font) {
  lovrRelease(font->texture);
  TextureData* textureData = lovrTextureDataCreate(font->atlas.width, font->atlas.height, 0x0, FORMAT_RGB);
  font->texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, false, false, 0);
  lovrTextureSetFilter(font->texture, (TextureFilter) { .mode = FILTER_BILINEAR });
  lovrTextureSetWrap(font->texture, (TextureWrap) { .s = WRAP_CLAMP, .t = WRAP_CLAMP });
  lovrRelease(textureData);
}
