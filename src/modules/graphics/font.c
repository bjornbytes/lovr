#include "graphics/font.h"
#include "graphics/texture.h"
#include "data/rasterizer.h"
#include "data/image.h"
#include "core/map.h"
#include <string.h>
#include <stdlib.h>

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
  uint32_t ref;
  Rasterizer* rasterizer;
  Texture* texture;
  FontAtlas atlas;
  map_t kerning;
  double spread;
  uint32_t padding;
  float lineHeight;
  float pixelDensity;
  FilterMode filterMode;
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

Font* lovrFontCreate(Rasterizer* rasterizer, uint32_t padding, double spread, FilterMode filterMode) {
  Font* font = calloc(1, sizeof(Font));
  lovrAssert(font, "Out of memory");
  font->ref = 1;

  lovrRetain(rasterizer);
  font->rasterizer = rasterizer;
  font->padding = padding;
  font->spread = spread;
  font->lineHeight = 1.f;
  font->pixelDensity = (float) lovrRasterizerGetHeight(rasterizer);
  font->filterMode = filterMode;
  map_init(&font->kerning, 0);

  // Atlas
  // The atlas padding affects the padding of the edges of the atlas and the space between rows.
  // It is different from the main font->padding, which is the padding on each individual glyph.
  uint32_t atlasPadding = 1;
  font->atlas.x = atlasPadding;
  font->atlas.y = atlasPadding;
  font->atlas.width = 256;
  font->atlas.height = 256;
  font->atlas.padding = atlasPadding;
  arr_init(&font->atlas.glyphs, arr_alloc);
  map_init(&font->atlas.glyphMap, 0);

  // Set initial atlas size
  while (font->atlas.height < 4 * lovrRasterizerGetSize(rasterizer)) {
    lovrFontExpandTexture(font);
  }

  // Create the texture
  lovrFontCreateTexture(font);

  return font;
}

void lovrFontDestroy(void* ref) {
  Font* font = ref;
  lovrRelease(font->rasterizer, lovrRasterizerDestroy);
  lovrRelease(font->texture, lovrTextureDestroy);
  for (size_t i = 0; i < font->atlas.glyphs.length; i++) {
    lovrRelease(font->atlas.glyphs.data[i].data, lovrImageDestroy);
  }
  arr_free(&font->atlas.glyphs);
  map_free(&font->atlas.glyphMap);
  map_free(&font->kerning);
  free(font);
}

Rasterizer* lovrFontGetRasterizer(Font* font) {
  return font->rasterizer;
}

Texture* lovrFontGetTexture(Font* font) {
  return font->texture;
}

void lovrFontRender(Font* font, const char* str, size_t length, float wrap, HorizontalAlign halign, float* vertices, uint16_t* indices, uint16_t baseVertex) {
  FontAtlas* atlas = &font->atlas;

  int height = lovrRasterizerGetHeight(font->rasterizer);

  float cx = 0.f;
  float cy = -height * .8f;
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
    if (codepoint == '\n' || (wrap && cx * scale > wrap && (codepoint == ' ' || previous == ' '))) {
      lineStart = lovrFontAlignLine(lineStart, vertexCursor, cx, halign);
      cx = 0.f;
      cy -= height * font->lineHeight;
      previous = '\0';
      if (codepoint == ' ' || codepoint == '\n') {
        str += bytes;
        continue;
      }
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
      int32_t padding = font->padding;
      float x1 = cx + glyph->dx - padding;
      float y1 = cy + (glyph->dy + padding);
      float x2 = x1 + glyph->tw;
      float y2 = y1 - glyph->th;
      float s1 = glyph->x / u;
      float t1 = (glyph->y + glyph->th) / v;
      float s2 = (glyph->x + glyph->tw) / u;
      float t2 = glyph->y / v;
      
      if (font->flip) {
        float tmp = y1;
        y1 = -y2; y2 = -tmp;
        tmp = t1;
        t1 = t2; t2 = tmp;
      }

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
  wrap *= font->pixelDensity;
  lovrRasterizerMeasure(font->rasterizer, str, length, wrap, width, height, lineCount, glyphCount);
  *width /= font->pixelDensity;
  *height *= font->lineHeight * (font->flip ? -1 : 1);
}

uint32_t lovrFontGetPadding(Font* font) {
  return font->padding;
}

double lovrFontGetSpread(Font* font) {
  return font->spread;
}

float lovrFontGetHeight(Font* font) {
  return lovrRasterizerGetHeight(font->rasterizer) / font->pixelDensity;
}

float lovrFontGetAscent(Font* font) {
  return lovrRasterizerGetAscent(font->rasterizer) / font->pixelDensity;
}

float lovrFontGetDescent(Font* font) {
  return lovrRasterizerGetDescent(font->rasterizer) / font->pixelDensity;
}

float lovrFontGetBaseline(Font* font) {
  return lovrRasterizerGetHeight(font->rasterizer) * .8f / font->pixelDensity;
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
    pixelDensity = lovrRasterizerGetHeight(font->rasterizer);
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
    lovrRasterizerLoadGlyph(font->rasterizer, codepoint, font->padding, font->spread, &atlas->glyphs.data[atlas->glyphs.length++]);
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

// TODO we only need the Image here to clear the texture, but it's a big waste of memory.
// Could look into using glClearTexImage when supported to make this more efficient.
static void lovrFontCreateTexture(Font* font) {
  lovrRelease(font->texture, lovrTextureDestroy);
  Image* image = lovrImageCreate(font->atlas.width, font->atlas.height, NULL, 0x0, FORMAT_RGBA16F);
  font->texture = lovrTextureCreate(TEXTURE_2D, &image, 1, false, false, 0);
  lovrTextureSetFilter(font->texture, (TextureFilter) { .mode = font->filterMode });
  lovrTextureSetWrap(font->texture, (TextureWrap) { .s = WRAP_CLAMP, .t = WRAP_CLAMP });
  lovrRelease(image, lovrImageDestroy);
}
