#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "loaders/font.h"
#include "loaders/texture.h"
#include "vendor/vec/vec.h"
#include <string.h>

static vec_float_t scratch;

Font* lovrFontCreate(FontData* fontData) {
  Font* font = lovrAlloc(sizeof(Font), lovrFontDestroy);
  if (!font) return NULL;

  font->fontData = fontData;
  font->padding = 1;
  font->tx = font->padding;
  font->ty = font->padding;
  font->tw = 256;
  font->th = 256;
  TextureData* textureData = lovrTextureDataGetBlank(font->tw, font->th, 0x00, FORMAT_RG);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
  font->texture = lovrTextureCreate(textureData);
  lovrTextureSetWrap(font->texture, WRAP_CLAMP, WRAP_CLAMP);
  int swizzle[] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
  map_init(&font->glyphs);

  return font;
}

void lovrFontDestroy(const Ref* ref) {
  Font* font = containerof(ref, Font);
  lovrFontDataDestroy(font->fontData);
  free(font);
}

void lovrFontDataDestroy(FontData* fontData) {
  // TODO
  free(fontData);
}

void lovrFontPrint(Font* font, const char* str) {
  vec_reserve(&scratch, strlen(str) * 30);
  vec_clear(&scratch);

  // Cursor
  float x = 0;
  float y = 0;

  for (unsigned int i = 0; i < strlen(str); i++) {
    Glyph* glyph = lovrFontGetGlyph(font, str[i]);
    float s = glyph->s;
    float t = glyph->t;
    float w = glyph->glyphData->w;
    float h = glyph->glyphData->h;

    float xx = x + glyph->glyphData->x;
    float yy = y - glyph->glyphData->y;

    float v[30] = {
      xx, yy, -5, s, t + h / font->th,
      xx, yy + h, -5, s, t,
      xx + w, yy, -5, s + w / font->tw, t + h / font->th,
      xx + w, yy, -5, s + w / font->tw, t + h / font->th,
      xx, yy + h, -5, s, t,
      xx + w, yy + h, -5, s + w / font->tw, t
    };

    x += glyph->glyphData->advance;

    vec_pusharr(&scratch, v, 30);
  }

  lovrGraphicsSetShapeData(scratch.data, scratch.length);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, font->texture, 0, 1, 0);
}

Glyph* lovrFontGetGlyph(Font* font, char character) {

  // Return the glyph if it's already loaded
  char key[2] = { character, '\0' };
  Glyph** g = (Glyph**) map_get(&font->glyphs, key);
  if (g) return *g;

  // Otherwise, load it (3 mallocs isn't great FIXME)
  Glyph* glyph = malloc(sizeof(Glyph));
  glyph->glyphData = lovrFontDataCreateGlyph(font->fontData, character);

  // Put the glyph somewhere, expanding texture as necessary
  while (font->tx + glyph->glyphData->w > font->tw - 2 * font->padding) {
    while (font->ty + glyph->glyphData->h > font->th - 2 * font->padding) {
      // Expand texture
    }
    font->tx = font->padding;
    font->ty += font->rowHeight;
    font->rowHeight = 0;
  }

  // Calculate texture coordinate
  glyph->s = font->tx / (float) font->tw;
  glyph->t = font->ty / (float) font->th;

  // Upload glyph to texture
  lovrGraphicsBindTexture(font->texture);
  glTexSubImage2D(
    GL_TEXTURE_2D, 0,
    font->tx, font->ty, glyph->glyphData->w, glyph->glyphData->h,
    GL_RG, GL_UNSIGNED_BYTE, glyph->glyphData->data
  );

  // Advance the texture cursor
  font->tx += glyph->glyphData->w;
  font->rowHeight = MAX(font->rowHeight, glyph->glyphData->h);

  // Write glyph to cache
  map_set(&font->glyphs, key, glyph);

  return glyph;
}
