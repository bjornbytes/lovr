#include "data/rasterizer.h"
#include "data/blob.h"
#include "data/image.h"
#include "resources/VarelaRound.ttf.h"
#include "core/util.h"
#include "lib/stb/stb_truetype.h"
#include <msdfgen-c.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct Rasterizer {
  uint32_t ref;
  stbtt_fontinfo font;
  struct Blob* blob;
  float size;
  float scale;
  int glyphCount;
  int height;
  int advance;
  int ascent;
  int descent;
};

Rasterizer* lovrRasterizerCreate(Blob* blob, float size) {
  Rasterizer* rasterizer = calloc(1, sizeof(Rasterizer));
  lovrAssert(rasterizer, "Out of memory");
  rasterizer->ref = 1;

  stbtt_fontinfo* font = &rasterizer->font;
  const unsigned char* data = blob ? blob->data : src_resources_VarelaRound_ttf;
  if (!stbtt_InitFont(font, data, stbtt_GetFontOffsetForIndex(data, 0))) {
    lovrThrow("Problem loading font");
  }

  lovrRetain(blob);
  rasterizer->blob = blob;
  rasterizer->size = size;
  rasterizer->scale = stbtt_ScaleForMappingEmToPixels(font, size);
  rasterizer->glyphCount = font->numGlyphs;

  int ascent, descent, linegap;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &linegap);
  rasterizer->ascent = roundf(ascent * rasterizer->scale);
  rasterizer->descent = roundf(descent * rasterizer->scale);
  rasterizer->height = roundf((ascent - descent + linegap) * rasterizer->scale);

  int x0, y0, x1, y1;
  stbtt_GetFontBoundingBox(font, &x0, &y0, &x1, &y1);
  rasterizer->advance = roundf(x1 * rasterizer->scale);

  return rasterizer;
}

void lovrRasterizerDestroy(void* ref) {
  Rasterizer* rasterizer = ref;
  lovrRelease(rasterizer->blob, lovrBlobDestroy);
  free(rasterizer);
}

float lovrRasterizerGetSize(Rasterizer* rasterizer) {
  return rasterizer->size;
}

int lovrRasterizerGetGlyphCount(Rasterizer* rasterizer) {
  return rasterizer->glyphCount;
}

int lovrRasterizerGetHeight(Rasterizer* rasterizer) {
  return rasterizer->height;
}

int lovrRasterizerGetAdvance(Rasterizer* rasterizer) {
  return rasterizer->advance;
}

int lovrRasterizerGetAscent(Rasterizer* rasterizer) {
  return rasterizer->ascent;
}

int lovrRasterizerGetDescent(Rasterizer* rasterizer) {
  return rasterizer->descent;
}

bool lovrRasterizerHasGlyph(Rasterizer* rasterizer, uint32_t character) {
  return stbtt_FindGlyphIndex(&rasterizer->font, character) != 0;
}

bool lovrRasterizerHasGlyphs(Rasterizer* rasterizer, const char* str) {
  const char* end = str + strlen(str);
  unsigned int codepoint;
  size_t bytes;

  bool hasGlyphs = true;
  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
    hasGlyphs &= lovrRasterizerHasGlyph(rasterizer, codepoint);
    str += bytes;
  }
  return hasGlyphs;
}

void lovrRasterizerLoadGlyph(Rasterizer* rasterizer, uint32_t character, uint32_t padding, double spread, Glyph* glyph) {
  int glyphIndex = stbtt_FindGlyphIndex(&rasterizer->font, character);
  lovrAssert(glyphIndex, "No font glyph found for character code %d, try using Rasterizer:hasGlyphs", character);

  int advance, bearing;
  stbtt_GetGlyphHMetrics(&rasterizer->font, glyphIndex, &advance, &bearing);

  if (stbtt_IsGlyphEmpty(&rasterizer->font, glyphIndex)) {
    memset(glyph, 0, sizeof(Glyph));
    glyph->advance = roundf(advance * rasterizer->scale);
    return;
  }

  // Trace glyph outline
  stbtt_vertex* vertices;
  int vertexCount = stbtt_GetGlyphShape(&rasterizer->font, glyphIndex, &vertices);
  msShape* shape = msShapeCreate();
  msContour* contour = NULL;
  float x = 0.f;
  float y = 0.f;

  for (int i = 0; i < vertexCount; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x * rasterizer->scale;
    float y2 = vertex.y * rasterizer->scale;

    switch (vertex.type) {
      case STBTT_vmove:
        contour = msShapeAddContour(shape);
        break;

      case STBTT_vline:
        msContourAddLinearEdge(contour, x, y, x2, y2);
        break;

      case STBTT_vcurve: {
        float cx = vertex.cx * rasterizer->scale;
        float cy = vertex.cy * rasterizer->scale;
        msContourAddQuadraticEdge(contour, x, y, cx, cy, x2, y2);
        break;
      }

      case STBTT_vcubic: {
        float cx1 = vertex.cx * rasterizer->scale;
        float cy1 = vertex.cy * rasterizer->scale;
        float cx2 = vertex.cx1 * rasterizer->scale;
        float cy2 = vertex.cy1 * rasterizer->scale;
        msContourAddCubicEdge(contour, x, y, cx1, cy1, cx2, cy2, x2, y2);
        break;
      }
    }

    x = x2;
    y = y2;
  }

  stbtt_FreeShape(&rasterizer->font, vertices);

  int x0, y0, x1, y1;
  stbtt_GetGlyphBox(&rasterizer->font, glyphIndex, &x0, &y0, &x1, &y1);

  // Initialize glyph data
  glyph->x = 0;
  glyph->y = 0;
  glyph->w = ceilf((x1 - x0) * rasterizer->scale);
  glyph->h = ceilf((y1 - y0) * rasterizer->scale);
  glyph->tw = glyph->w + 2 * padding;
  glyph->th = glyph->h + 2 * padding;
  glyph->dx = roundf(bearing * rasterizer->scale);
  glyph->dy = roundf(y1 * rasterizer->scale);
  glyph->advance = roundf(advance * rasterizer->scale);
  glyph->data = lovrImageCreate(glyph->tw, glyph->th, NULL, 0, FORMAT_RGBA32F);

  // Render SDF
  float tx = (float) padding + -glyph->dx;
  float ty = (float) padding + (float) glyph->h - glyph->dy;
  msShapeNormalize(shape);
  msEdgeColoringSimple(shape, 3., 0);
  msGenerateMTSDF(glyph->data->blob->data, glyph->tw, glyph->th, shape, spread, 1.f, 1.f, tx, ty);
  msShapeDestroy(shape);
}

int32_t lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t left, uint32_t right) {
  return stbtt_GetCodepointKernAdvance(&rasterizer->font, left, right) * rasterizer->scale;
}
