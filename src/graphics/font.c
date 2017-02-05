#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "loaders/font.h"
#include "loaders/texture.h"
#include <string.h>

Font* lovrFontCreate(FontData* fontData) {
  Font* font = lovrAlloc(sizeof(Font), lovrFontDestroy);
  if (!font) return NULL;

  int padding = 1;
  font->atlas.x = padding;
  font->atlas.y = padding;
  font->atlas.width = 64;
  font->atlas.height = 64;
  font->atlas.rowHeight = 0;
  font->atlas.offsetX = 0;
  font->atlas.extentX = 0;
  font->atlas.extentY = 0;
  font->atlas.padding = padding;
  map_init(&font->atlas.glyphs);

  font->fontData = fontData;
  font->texture = NULL;
  lovrFontCreateTexture(font);
  vec_init(&font->vertices);

  return font;
}

void lovrFontDestroy(const Ref* ref) {
  Font* font = containerof(ref, Font);
  lovrFontDataDestroy(font->fontData);
  lovrRelease(&font->texture->ref);
  map_deinit(&font->atlas.glyphs);
  vec_deinit(&font->vertices);
  free(font);
}

void lovrFontDataDestroy(FontData* fontData) {
  // TODO
  free(fontData);
}

void lovrFontPrint(Font* font, const char* str) {
  FontAtlas* atlas = &font->atlas;
  vec_reserve(&font->vertices, strlen(str) * 30);
  vec_clear(&font->vertices);

  // Cursor
  float x = 0;
  float y = 0;

  for (unsigned int i = 0; i < strlen(str); i++) {
    Glyph* glyph = lovrFontGetGlyph(font, str[i]);

    if (glyph->w > 0 || glyph->h > 0) {
      float s = glyph->x / (float) atlas->width;
      float t = glyph->y / (float) atlas->height;
      float w = glyph->w;
      float h = glyph->h;
      float s2 = (glyph->x + w) / (float) atlas->width;
      float t2 = (glyph->y + h) / (float) atlas->height;
      float ox = x + glyph->ox;
      float oy = -y - glyph->oy;

      float v[30] = {
        ox, -oy, 0, s, t,
        ox, -oy - h, 0, s, t2,
        ox + w, -oy, 0, s2, t,
        ox + w, -oy, 0, s2, t,
        ox, -oy - h, 0, s, t2,
        ox + w, -oy - h, 0, s2, t2
      };

      vec_pusharr(&font->vertices, v, 30);
    }

    x += glyph->advance;
  }

  lovrGraphicsSetShapeData(font->vertices.data, font->vertices.length);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, font->texture, 0, 1, 0);
}

Glyph* lovrFontGetGlyph(Font* font, char character) {
  char key[2] = { character, '\0' };
  FontAtlas* atlas = &font->atlas;
  vec_glyph_t* glyphs = &atlas->glyphs;
  Glyph* glyph = map_get(glyphs, key);

  if (!glyph) {

    // Load the new glyph and add it to the cache
    Glyph g;
    lovrFontDataLoadGlyph(font->fontData, character, &g);
    map_set(glyphs, key, g);
    glyph = map_get(glyphs, key);

    // Exit early if the glyph is empty to save texture space
    if (glyph->w == 0 && glyph->h == 0) {
      return glyph;
    }

    // If the glyph does not fit, you must acquit
    int tooWide = 0, tooTall = 0;
    do {

      // New row
      if ((tooWide = atlas->x + glyph->w > atlas->width - 2 * atlas->padding) == 1) {
        atlas->x = atlas->offsetX + atlas->padding;
        atlas->y += atlas->rowHeight + atlas->padding;
        atlas->rowHeight = 0;
      }

      // +---+    +---+---+    +---+---+
      // |   | -> |   |   | -> |   |   |
      // +---+    +---+---+    +---+---+
      //                       |       |
      //                       +-------+
      if ((tooTall = atlas->y + glyph->h > atlas->height - 2 * atlas->padding) == 1) {
        if (atlas->width == atlas->height) {
          atlas->offsetX = atlas->extentX;
          atlas->x = atlas->offsetX + atlas->padding;
          atlas->y = atlas->padding;
          atlas->rowHeight = 0;
          atlas->width <<= 1;
        } else {
          atlas->extentX = 0;
          atlas->offsetX = 0;
          atlas->height <<= 1;
        }
      }
    } while (tooWide || tooTall);

    // Keep track of glyph position in atlas
    glyph->x = atlas->x;
    glyph->y = atlas->y;

    // Paste glyph into texture
    lovrGraphicsBindTexture(font->texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, atlas->x, atlas->y, glyph->w, glyph->h, GL_RG, GL_UNSIGNED_BYTE, glyph->data);

    // Advance atlas cursor
    atlas->x += glyph->w + atlas->padding;
    atlas->rowHeight = MAX(atlas->rowHeight, glyph->h);
    atlas->extentX = MAX(atlas->extentX, atlas->x);
    atlas->extentY = MAX(atlas->extentY, atlas->y + atlas->rowHeight);
  }

  return glyph;
}

void lovrFontCreateTexture(Font* font) {
  if (font->texture) {
    lovrRelease(&font->texture->ref);
  }

  FontAtlas* atlas = &font->atlas;
  TextureData* textureData = lovrTextureDataGetBlank(atlas->width * 4, atlas->height * 4, 0x0, FORMAT_RG);
  font->texture = lovrTextureCreate(textureData);
  lovrTextureSetWrap(font->texture, WRAP_CLAMP, WRAP_CLAMP);
  int swizzle[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
}
