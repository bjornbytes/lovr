#include "data/rasterizer.h"
#include "data/blob.h"
#include "util.h"
#include "VarelaRound.ttf.h"
#include "lib/stb/stb_truetype.h"
#include <msdfgen-c.h>
#include <stdlib.h>
#include <math.h>

struct Rasterizer {
  uint32_t ref;
  struct Blob* blob;
  float size;
  float scale;
  float ascent;
  float descent;
  float lineGap;
  float leading;
  stbtt_fontinfo font;
};

Rasterizer* lovrRasterizerCreate(Blob* blob, float size) {
  Rasterizer* rasterizer = calloc(1, sizeof(Rasterizer));
  lovrAssert(rasterizer, "Out of memory");
  rasterizer->ref = 1;

  stbtt_fontinfo* font = &rasterizer->font;
  const unsigned char* data = blob ? blob->data : etc_VarelaRound_ttf;
  if (!stbtt_InitFont(font, data, stbtt_GetFontOffsetForIndex(data, 0))) {
    lovrThrow("Problem loading font");
  }

  lovrRetain(blob);
  rasterizer->blob = blob;
  rasterizer->size = size;
  rasterizer->scale = stbtt_ScaleForMappingEmToPixels(font, size);

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
  rasterizer->ascent = ascent * rasterizer->scale;
  rasterizer->descent = descent * rasterizer->scale;
  rasterizer->lineGap = lineGap * rasterizer->scale;
  rasterizer->leading = (ascent - descent + lineGap) * rasterizer->scale;

  return rasterizer;
}

void lovrRasterizerDestroy(void* ref) {
  Rasterizer* rasterizer = ref;
  lovrRelease(rasterizer->blob, lovrBlobDestroy);
  free(rasterizer);
}

float lovrRasterizerGetFontSize(Rasterizer* rasterizer) {
  return rasterizer->size;
}

void lovrRasterizerGetBoundingBox(Rasterizer* rasterizer, float box[4]) {
  int x0, y0, x1, y1;
  stbtt_GetFontBoundingBox(&rasterizer->font, &x0, &y0, &x1, &y1);
  box[0] = x0 * rasterizer->scale;
  box[1] = y0 * rasterizer->scale;
  box[2] = x1 * rasterizer->scale;
  box[3] = y1 * rasterizer->scale;
}

uint32_t lovrRasterizerGetGlyphCount(Rasterizer* rasterizer) {
  return rasterizer->font.numGlyphs;
}

bool lovrRasterizerHasGlyph(Rasterizer* rasterizer, uint32_t codepoint) {
  return stbtt_FindGlyphIndex(&rasterizer->font, codepoint) != 0;
}

bool lovrRasterizerHasGlyphs(Rasterizer* rasterizer, const char* str, size_t length) {
  size_t bytes;
  uint32_t codepoint;
  const char* end = str + length;
  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {
    if (!lovrRasterizerHasGlyph(rasterizer, codepoint)) {
      return false;
    }
    str += bytes;
  }
  return true;
}

float lovrRasterizerGetHeight(Rasterizer* rasterizer) {
  return rasterizer->ascent - rasterizer->descent;
}

float lovrRasterizerGetAscent(Rasterizer* rasterizer) {
  return rasterizer->ascent;
}

float lovrRasterizerGetDescent(Rasterizer* rasterizer) {
  return rasterizer->descent;
}

float lovrRasterizerGetLineGap(Rasterizer* rasterizer) {
  return rasterizer->lineGap;
}

float lovrRasterizerGetLeading(Rasterizer* rasterizer) {
  return rasterizer->leading;
}

float lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t prev, uint32_t next) {
  return stbtt_GetCodepointKernAdvance(&rasterizer->font, prev, next) * rasterizer->scale;
}

float lovrRasterizerGetGlyphAdvance(Rasterizer* rasterizer, uint32_t codepoint) {
  int advance, bearing;
  stbtt_GetCodepointHMetrics(&rasterizer->font, codepoint, &advance, &bearing);
  return advance * rasterizer->scale;
}

float lovrRasterizerGetGlyphBearing(Rasterizer* rasterizer, uint32_t codepoint) {
  int advance, bearing;
  stbtt_GetCodepointHMetrics(&rasterizer->font, codepoint, &advance, &bearing);
  return bearing * rasterizer->scale;
}

void lovrRasterizerGetGlyphBoundingBox(Rasterizer* rasterizer, uint32_t codepoint, float box[4]) {
  int x0, y0, x1, y1;
  stbtt_GetCodepointBox(&rasterizer->font, codepoint, &x0, &y0, &x1, &y1);
  box[0] = x0 * rasterizer->scale;
  box[1] = y0 * rasterizer->scale;
  box[2] = x1 * rasterizer->scale;
  box[3] = y1 * rasterizer->scale;
}

bool lovrRasterizerIsGlyphEmpty(Rasterizer* rasterizer, uint32_t codepoint) {
  return stbtt_IsGlyphEmpty(&rasterizer->font, stbtt_FindGlyphIndex(&rasterizer->font, codepoint));
}

bool lovrRasterizerGetGlyphCurves(Rasterizer* rasterizer, uint32_t codepoint, void (*fn)(void* context, uint32_t degree, float* points), void* context) {
  uint32_t id = stbtt_FindGlyphIndex(&rasterizer->font, codepoint);

  if (stbtt_IsGlyphEmpty(&rasterizer->font, id)) {
    return false;
  }

  stbtt_vertex* vertices;
  uint32_t count = stbtt_GetGlyphShape(&rasterizer->font, id, &vertices);

  float x = 0.f;
  float y = 0.f;
  for (uint32_t i = 0; i < count; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x * rasterizer->scale;
    float y2 = vertex.y * rasterizer->scale;

    if (vertex.type == STBTT_vline) {
      float points[4] = { x, y, x2, y2 };
      fn(context, 1, points);
    } else if (vertex.type == STBTT_vcurve) {
      float cx = vertex.cx * rasterizer->scale;
      float cy = vertex.cy * rasterizer->scale;
      float points[6] = { x, y, cx, cy, x2, y2 };
      fn(context, 2, points);
    } else if (vertex.type == STBTT_vcubic) {
      float cx1 = vertex.cx * rasterizer->scale;
      float cy1 = vertex.cy * rasterizer->scale;
      float cx2 = vertex.cx1 * rasterizer->scale;
      float cy2 = vertex.cy1 * rasterizer->scale;
      float points[8] = { x, y, cx1, cy1, cx2, cy2, x2, y2 };
      fn(context, 3, points);
    }

    x = x2;
    y = y2;
  }

  stbtt_FreeShape(&rasterizer->font, vertices);
  return true;
}

bool lovrRasterizerGetGlyphPixels(Rasterizer* rasterizer, uint32_t codepoint, float* pixels, uint32_t width, uint32_t height, double spread) {
  int id = stbtt_FindGlyphIndex(&rasterizer->font, codepoint);

  if (stbtt_IsGlyphEmpty(&rasterizer->font, id)) {
    return false;
  }

  stbtt_vertex* vertices;
  int count = stbtt_GetGlyphShape(&rasterizer->font, id, &vertices);

  msShape* shape = msShapeCreate();
  msContour* contour = NULL;

  float x = 0.f;
  float y = 0.f;
  for (int i = 0; i < count; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x;
    float y2 = vertex.y;

    if (vertex.type == STBTT_vmove) {
      contour = msShapeAddContour(shape);
    } else if (vertex.type == STBTT_vline) {
      msContourAddLinearEdge(contour, x, y, x2, y2);
    } else if (vertex.type == STBTT_vcurve) {
      float cx = vertex.cx;
      float cy = vertex.cy;
      msContourAddQuadraticEdge(contour, x, y, cx, cy, x2, y2);
    } else if (vertex.type == STBTT_vcubic) {
      float cx1 = vertex.cx;
      float cy1 = vertex.cy;
      float cx2 = vertex.cx1;
      float cy2 = vertex.cy1;
      msContourAddCubicEdge(contour, x, y, cx1, cy1, cx2, cy2, x2, y2);
    }

    x = x2;
    y = y2;
  }

  stbtt_FreeShape(&rasterizer->font, vertices);

  int x0, y0, x1, y1;
  stbtt_GetGlyphBox(&rasterizer->font, id, &x0, &y0, &x1, &y1);

  float scale = rasterizer->scale;
  uint32_t padding = ceil(spread / 2.);
  float offsetX = -x0 + padding / scale;
  float offsetY = -y1 - padding / scale;

  msShapeNormalize(shape);
  msEdgeColoringSimple(shape, 3., 0);
  msGenerateMTSDF(pixels, width, height, shape, spread / rasterizer->scale, scale, -scale, offsetX, offsetY);
  msShapeDestroy(shape);

  return true;
}
