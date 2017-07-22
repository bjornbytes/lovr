#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "loaders/font.h"
#include "loaders/texture.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int lovrFontAlignLine(vec_float_t* vertices, int index, float width, HorizontalAlign halign) {
  while (index < vertices->length) {
    if (halign == ALIGN_CENTER) {
      vertices->data[index] -= width / 2.f;
    } else if (halign == ALIGN_RIGHT) {
      vertices->data[index] -= width;
    }

    index += 5;
  }

  return index;
}

Font* lovrFontCreate(FontData* fontData) {
  Font* font = lovrAlloc(sizeof(Font), lovrFontDestroy);
  if (!font) return NULL;

  font->fontData = fontData;
  font->texture = NULL;
  font->lineHeight = 1.f;
  font->pixelDensity = font->fontData->height;
  vec_init(&font->vertices);
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
  while (font->atlas.height < 4 * fontData->size) {
    lovrFontExpandTexture(font);
  }

  // Texture
  TextureData* textureData = lovrTextureDataGetBlank(font->atlas.width, font->atlas.height, 0x0, FORMAT_RGB);
  font->texture = lovrTextureCreate(textureData);
  lovrTextureSetWrap(font->texture, WRAP_CLAMP, WRAP_CLAMP);

  return font;
}

void lovrFontDestroy(const Ref* ref) {
  Font* font = containerof(ref, Font);
  lovrFontDataDestroy(font->fontData);
  lovrRelease(&font->texture->ref);
  map_deinit(&font->atlas.glyphs);
  map_deinit(&font->kerning);
  vec_deinit(&font->vertices);
  free(font);
}

void lovrFontPrint(Font* font, const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign) {
  FontAtlas* atlas = &font->atlas;

  float cx = 0;
  float cy = -font->fontData->height * .8;
  float u = atlas->width;
  float v = atlas->height;
  float scale = 1 / font->pixelDensity;

  int len = strlen(str);
  const char* start = str;
  const char* end = str + len;
  unsigned int previous = '\0';
  unsigned int codepoint;
  size_t bytes;

  int linePtr = 0;
  int lineCount = 1;

  vec_reserve(&font->vertices, len * 30);
  vec_clear(&font->vertices);

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {

    // Newlines
    if (codepoint == '\n' || (wrap && cx * scale > wrap && codepoint == ' ')) {
      linePtr = lovrFontAlignLine(&font->vertices, linePtr, cx, halign);
      lineCount++;
      cx = 0;
      cy -= font->fontData->height * font->lineHeight;
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
      lovrFontPrint(font, start, transform, wrap, halign, valign);
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

      float vertices[30] = {
        x1, y1, 0, s1, t1,
        x1, y2, 0, s1, t2,
        x2, y1, 0, s2, t1,
        x2, y1, 0, s2, t1,
        x1, y2, 0, s1, t2,
        x2, y2, 0, s2, t2
      };

      vec_pusharr(&font->vertices, vertices, 30);
    }

    // Advance cursor
    cx += glyph->advance;
    str += bytes;
  }

  // Align the last line
  lovrFontAlignLine(&font->vertices, linePtr, cx, halign);

  // Calculate vertical offset
  float offsety = 0;
  if (valign == ALIGN_MIDDLE) {
    offsety = lineCount * font->fontData->height * font->lineHeight * .5f;
  } else if (valign == ALIGN_BOTTOM) {
    offsety = lineCount * font->fontData->height * font->lineHeight;
  }

  // Render!
  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsScale(scale, scale, scale);
  lovrGraphicsTranslate(0, offsety, 0);
  lovrGraphicsBindTexture(font->texture);
  lovrGraphicsSetShapeData(font->vertices.data, font->vertices.length);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, 0, 1, 0);
  lovrGraphicsPop();
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
  return font->fontData->height / font->pixelDensity;
}

float lovrFontGetAscent(Font* font) {
  return font->fontData->ascent / font->pixelDensity;
}

float lovrFontGetDescent(Font* font) {
  return font->fontData->descent / font->pixelDensity;
}

float lovrFontGetBaseline(Font* font) {
  return font->fontData->height * .8 / font->pixelDensity;
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

  int kerning = lovrFontDataGetKerning(font->fontData, left, right);
  map_set(&font->kerning, key, kerning);
  return kerning;
}

float lovrFontGetPixelDensity(Font* font) {
  return font->pixelDensity;
}

void lovrFontSetPixelDensity(Font* font, float pixelDensity) {
  if (pixelDensity <= 0) {
    pixelDensity = font->fontData->height;
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
    lovrFontDataLoadGlyph(font->fontData, codepoint, &g);
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
  lovrGraphicsBindTexture(font->texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, atlas->x, atlas->y, glyph->tw, glyph->th, FORMAT_RGB.glFormat, GL_UNSIGNED_BYTE, glyph->data);

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

  // Resize the texture storage
  while (glGetError());
  lovrTextureDataResize(font->texture->textureData, atlas->width, atlas->height, 0x0);
  lovrTextureRefresh(font->texture);
  if (glGetError()) {
    error("Problem expanding font texture (out of space?)");
  }

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
