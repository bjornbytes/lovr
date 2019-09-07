#include "graphics/font.h"
#include "graphics/texture.h"
#include "data/rasterizer.h"
#include "data/textureData.h"
#include "core/arr.h"
#include "core/hash.h"
#include "core/map.h"
#include "core/ref.h"
#include "core/utf.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t rowHeight;
  uint32_t padding;
  arr_t(Glyph) glyphs;
  map_t glyphMap;
} FontAtlas;

struct Font {
  Rasterizer* rasterizer;
  Texture* texture;
  FontAtlas atlas;
  map_t kerning;
  float lineHeight;
  float pixelDensity;
  bool flip;
};

static float* lovrFontAlignLine(float* x, float* lineEnd, float width, HorizontalAlign halign) {
  while (x < lineEnd) {
    if (halign == ALIGN_CENTER) {
      *x -= width / 2.f;
    } else if (halign == ALIGN_RIGHT) {
      *x -= width;
    }

    x += 8;
  }

  return x;
}

static Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint);
static void lovrFontAddGlyph(Font* font, Glyph* glyph);
static void lovrFontExpandTexture(Font* font);
static void lovrFontCreateTexture(Font* font);

Font* lovrFontCreate(Rasterizer* rasterizer) {
  Font* font = lovrAlloc(Font);
  lovrRetain(rasterizer);
  font->rasterizer = rasterizer;
  font->lineHeight = 1.f;
  font->pixelDensity = (float) font->rasterizer->height;
  map_init(&font->kerning, 0);

  // Atlas
  uint32_t padding = 1;
  font->atlas.x = padding;
  font->atlas.y = padding;
  font->atlas.width = 128;
  font->atlas.height = 128;
  font->atlas.padding = padding;
  arr_init(&font->atlas.glyphs);
  map_init(&font->atlas.glyphMap, 0);

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
  lovrRelease(Rasterizer, font->rasterizer);
  lovrRelease(Texture, font->texture);
  for (size_t i = 0; i < font->atlas.glyphs.length; i++) {
    lovrRelease(TextureData, font->atlas.glyphs.data[i].data);
  }
  map_free(&font->atlas.glyphMap);
  map_free(&font->kerning);
}

Rasterizer* lovrFontGetRasterizer(Font* font) {
  return font->rasterizer;
}

Texture* lovrFontGetTexture(Font* font) {
  return font->texture;
}

void lovrFontRender(Font* font, const char* str, size_t length, float wrap, HorizontalAlign halign, float* vertices, uint16_t* indices, uint16_t baseVertex) {
  FontAtlas* atlas = &font->atlas;
  bool flip = font->flip;

  float cx = 0.f;
  float cy = -font->rasterizer->height * .8f * (flip ? -1.f : 1.f);
  float u = atlas->width;
  float v = atlas->height;
  float scale = 1.f / font->pixelDensity;

  const char* start = str;
  const char* end = str + length;
  unsigned int previous = '\0';
  unsigned int codepoint;
  size_t bytes;

  float* vertexCursor = vertices;
  uint16_t* indexCursor = indices;
  float* lineStart = vertices;
  uint16_t I = baseVertex;

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {

    // Newlines
    if (codepoint == '\n' || (wrap && cx * scale > wrap && codepoint == ' ')) {
      lineStart = lovrFontAlignLine(lineStart, vertexCursor, cx, halign);
      cx = 0.f;
      cy -= font->rasterizer->height * font->lineHeight * (flip ? -1.f : 1.f);
      previous = '\0';
      str += bytes;
      continue;
    }

    // Tabs
    if (codepoint == '\t') {
      Glyph* space = lovrFontGetGlyph(font, ' ');
      cx += space->advance * 4.f;
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
      lovrFontRender(font, start, length, wrap, halign, vertices, indices, baseVertex);
      return;
    }

    // Triangles
    if (glyph->w > 0 && glyph->h > 0) {
      float x1 = cx + glyph->dx - GLYPH_PADDING;
      float y1 = cy + (glyph->dy + GLYPH_PADDING) * (flip ? -1.f : 1.f);
      float x2 = x1 + glyph->tw;
      float y2 = y1 - glyph->th * (flip ? -1.f : 1.f);
      float s1 = glyph->x / u;
      float t1 = (glyph->y + glyph->th) / v;
      float s2 = (glyph->x + glyph->tw) / u;
      float t2 = glyph->y / v;

      memcpy(vertexCursor, (float[32]) {
        x1, y1, 0.f, 0.f, 0.f, 0.f, s1, t1,
        x1, y2, 0.f, 0.f, 0.f, 0.f, s1, t2,
        x2, y1, 0.f, 0.f, 0.f, 0.f, s2, t1,
        x2, y2, 0.f, 0.f, 0.f, 0.f, s2, t2
      }, 32 * sizeof(float));

      memcpy(indexCursor, (uint16_t[6]) { I + 0, I + 1, I + 2, I + 2, I + 1, I + 3 }, 6 * sizeof(uint16_t));

      vertexCursor += 32;
      indexCursor += 6;
      I += 4;
    }

    // Advance cursor
    cx += glyph->advance;
    str += bytes;
  }

  // Align the last line
  lovrFontAlignLine(lineStart, vertexCursor, cx, halign);
}

void lovrFontMeasure(Font* font, const char* str, size_t length, float wrap, float* width, float* height, uint32_t* lineCount, uint32_t* glyphCount) {
  float x = 0.f;
  const char* end = str + length;
  size_t bytes;
  unsigned int previous = '\0';
  unsigned int codepoint;
  float scale = 1.f / font->pixelDensity;
  *width = 0.f;
  *lineCount = 0;
  *glyphCount = 0;

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
    if (codepoint == '\n' || (wrap && x * scale > wrap && codepoint == ' ')) {
      *width = MAX(*width, x * scale);
      (*lineCount)++;
      x = 0.f;
      previous = '\0';
      str += bytes;
      continue;
    }

    // Tabs
    if (codepoint == '\t') {
      Glyph* space = lovrFontGetGlyph(font, ' ');
      x += space->advance * 4.f;
      str += bytes;
      continue;
    }

    Glyph* glyph = lovrFontGetGlyph(font, codepoint);

    if (glyph->w > 0 && glyph->h > 0) {
      (*glyphCount)++;
    }

    x += glyph->advance + lovrFontGetKerning(font, previous, codepoint);
    previous = codepoint;
    str += bytes;
  }

  *width = MAX(*width, x * scale);
  *height = ((*lineCount + 1) * font->rasterizer->height * font->lineHeight) * (font->flip ? -1 : 1);
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
  return font->rasterizer->height * .8f / font->pixelDensity;
}

float lovrFontGetLineHeight(Font* font) {
  return font->lineHeight;
}

void lovrFontSetLineHeight(Font* font, float lineHeight) {
  font->lineHeight = lineHeight;
}

bool lovrFontIsFlipEnabled(Font* font) {
  return font->flip;
}

void lovrFontSetFlipEnabled(Font* font, bool flip) {
  font->flip = flip;
}

int32_t lovrFontGetKerning(Font* font, uint32_t left, uint32_t right) {
  uint64_t key = ((uint64_t) left << 32) + right;
  uint64_t hash = hash64(&key, sizeof(key)); // TODO improve number hashing
  uint64_t kerning = map_get(&font->kerning, hash);

  if (kerning == MAP_NIL) {
    kerning = lovrRasterizerGetKerning(font->rasterizer, left, right);
    map_set(&font->kerning, hash, kerning);
  }

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

static Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint) {
  FontAtlas* atlas = &font->atlas;
  uint64_t hash = hash64(&codepoint, sizeof(codepoint));
  uint64_t index = map_get(&atlas->glyphMap, hash);

  // Add the glyph to the atlas if it isn't there
  if (index == MAP_NIL) {
    index = atlas->glyphs.length;
    arr_reserve(&atlas->glyphs, atlas->glyphs.length + 1);
    lovrRasterizerLoadGlyph(font->rasterizer, codepoint, &atlas->glyphs.data[atlas->glyphs.length++]);
    map_set(&atlas->glyphMap, hash, index);
    lovrFontAddGlyph(font, &atlas->glyphs.data[index]);
  }

  return &atlas->glyphs.data[index];
}

static void lovrFontAddGlyph(Font* font, Glyph* glyph) {
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

static void lovrFontExpandTexture(Font* font) {
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
  for (size_t i = 0; i < atlas->glyphs.length; i++) {
    lovrFontAddGlyph(font, &atlas->glyphs.data[i]);
  }
}

// TODO we only need the TextureData here to clear the texture, but it's a big waste of memory.
// Could look into using glClearTexImage when supported to make this more efficient.
static void lovrFontCreateTexture(Font* font) {
  lovrRelease(Texture, font->texture);
  TextureData* textureData = lovrTextureDataCreate(font->atlas.width, font->atlas.height, 0x0, FORMAT_RGB);
  font->texture = lovrTextureCreate(TEXTURE_2D, &textureData, 1, false, false, 0);
  lovrTextureSetFilter(font->texture, (TextureFilter) { .mode = FILTER_BILINEAR });
  lovrTextureSetWrap(font->texture, (TextureWrap) { .s = WRAP_CLAMP, .t = WRAP_CLAMP });
  lovrRelease(TextureData, textureData);
}
