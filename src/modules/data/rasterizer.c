#include "data/rasterizer.h"
#include "data/blob.h"
#include "data/image.h"
#include "util.h"
#include "VarelaRound.ttf.h"
#include "lib/stb/stb_truetype.h"
#include <msdfgen-c.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct Rasterizer {
  uint32_t ref;
  float size;
  float scale;
  float ascent;
  float descent;
  float leading;
  map_t kerning;
  Blob* blob;
  Image* atlas;
  stbtt_fontinfo font;
};

static Rasterizer* lovrRasterizerCreateTTF(Blob* blob, float size) {
  unsigned char* data = blob ? blob->data : etc_VarelaRound_ttf;

  int offset = stbtt_GetFontOffsetForIndex(data, 0);
  if (offset == -1) {
    return NULL;
  }

  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, data, offset)) {
    return NULL;
  }

  Rasterizer* rasterizer = lovrCalloc(sizeof(Rasterizer));
  rasterizer->ref = 1;

  lovrRetain(blob);
  rasterizer->blob = blob;
  rasterizer->font = font;
  rasterizer->size = size;
  rasterizer->scale = stbtt_ScaleForMappingEmToPixels(&rasterizer->font, size);

  // Even though line gap is a thing, it's usually zero so we pretend it isn't real
  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(&rasterizer->font, &ascent, &descent, &lineGap);
  rasterizer->ascent = ascent * rasterizer->scale;
  rasterizer->descent = descent * rasterizer->scale;
  rasterizer->leading = (ascent - descent + lineGap) * rasterizer->scale;

  map_init(&rasterizer->kerning, 0);

  return rasterizer;
}

static int64_t parseNumber(const char* line, size_t lineLength, map_t* map, const char* key) {
  uint64_t value = map_get(map, hash64(key, strlen(key)));
  if (value == MAP_NIL) return 0;
  const char* string = (const char*) (uintptr_t) value;
  size_t length = lineLength - (string - line);
  bool negative = *string == '-';
  string += negative;
  length -= negative;
  int64_t number = 0;
  while (length > 0 && *string != ' ' && *string >= '0' && *string <= '9') {
    number *= 10;
    number += *string++ - '0';
    length--;
  }
  return negative ? -number : number;
}

static const char* parseString(const char* line, size_t lineLength, map_t* map, const char* key, size_t* length) {
  uint64_t value = map_get(map, hash64(key, strlen(key)));
  if (value == MAP_NIL) return NULL;
  const char* string = (const char*) (uintptr_t) value;
  size_t maxLength = lineLength - (string - line);
  if (string[0] == '"') {
    char* quote = memchr(string + 1, '"', maxLength - 1);
    if (!quote) return NULL;
    *length = quote - (string + 1);
    return string + 1;
  } else {
    char* space = memchr(string, ' ', maxLength);
    *length = space ? space - string : maxLength;
    return string;
  }
}

static Rasterizer* lovrRasterizerCreateBMF(Blob* blob, RasterizerIO* io) {
  if (!blob || blob->size < 4) return NULL;

  if (!memcmp(blob->data, "info", 4)) {
    Rasterizer* rasterizer = lovrCalloc(sizeof(Rasterizer));
    rasterizer->ref = 1;
    rasterizer->scale = 1.f;
    map_init(&rasterizer->kerning, 0);

    map_t map;
    map_init(&map, 8);

    char* string = blob->data;
    size_t length = blob->size;

    while (length > 0) {
      char* newline = memchr(string, '\n', length);
      size_t lineLength = newline ? newline - string : length;

      // Clear the map
      memset(map.hashes, 0xff, map.size * sizeof(uint64_t));
      memset(map.values, 0xff, map.size * sizeof(uint64_t));
      map.used = 0;

      // Parse the tag
      char* tag = string;
      char* space = memchr(string, ' ', lineLength);
      size_t tagLength = space ? space - tag : lineLength;
      size_t cursor = space ? tagLength + 1 : lineLength;

      // Add fields to the map
      while (cursor < lineLength) {
        char* equals = memchr(string + cursor, '=', lineLength - cursor);

        // Split fields on =, the part before is the key and the part after is the value, add to map
        if (equals) {
          char* key = string + cursor;
          uint64_t hash = hash64(key, equals - key);
          map_set(&map, hash, (uintptr_t) (equals + 1));
        }

        // Fields are separated by spaces
        space = memchr(string + cursor, ' ', lineLength - cursor);
        cursor = space ? space - string + 1 : lineLength;
      }

      if (!memcmp(tag, "info", tagLength)) {
        rasterizer->size = parseNumber(string, lineLength, &map, "size");
      } else if (!memcmp(tag, "common", tagLength)) {
        lovrCheck(parseNumber(string, lineLength, &map, "pages") == 1, "Currently, BMFont files with multiple images are not supported");
        lovrCheck(parseNumber(string, lineLength, &map, "packed") == 0, "Currently, packed BMFont files are not supported");
        rasterizer->leading = parseNumber(string, lineLength, &map, "lineHeight");
        rasterizer->ascent = parseNumber(string, lineLength, &map, "base");
        rasterizer->descent = rasterizer->leading - rasterizer->ascent; // Best effort
      } else if (!memcmp(tag, "page", tagLength)) {
        char fullpath[1024];
        size_t nameLength = strlen(blob->name);
        lovrCheck(nameLength < sizeof(fullpath), "BMFont filename is too long");
        memcpy(fullpath, blob->name, nameLength + 1);
        char* slash = strrchr(fullpath, '/');
        char* filename = slash ? slash + 1 : fullpath;
        size_t maxLength = sizeof(fullpath) - 1 - (filename - fullpath);

        size_t filenameLength;
        const char* file = parseString(string, lineLength, &map, "file", &filenameLength);
        lovrCheck(file, "BMFont is missing image path");
        lovrCheck(filenameLength <= maxLength, "BMFont filename is too long");
        memcpy(filename, file, filenameLength);
        filename[filenameLength] = '\0';

        size_t atlasSize;
        void* atlasData = io(fullpath, &atlasSize);
        lovrCheck(atlasData, "Failed to read BMFont image from %s", fullpath);
        Blob* atlasBlob = lovrBlobCreate(atlasData, atlasSize, "BMFont atlas");
        rasterizer->atlas = lovrImageCreateFromFile(atlasBlob);
      } else if (!memcmp(tag, "char", tagLength)) {
        //uint32_t codepoint = parseNumber(&map, "id");
      } else if (!memcmp(tag, "kerning", tagLength)) {
        uint32_t first = parseNumber(string, lineLength, &map, "first");
        uint32_t second = parseNumber(string, lineLength, &map, "second");
        int32_t kerning = parseNumber(string, lineLength, &map, "amount");
        uint32_t hash = hash64((uint32_t[]) { first, second }, 2 * sizeof(uint32_t));
        map_set(&rasterizer->kerning, hash, kerning);
      }

      // Go to the next line
      if (newline) {
        string += lineLength + 1;
        length -= lineLength + 1;
      } else {
        break;
      }
    }

    return rasterizer;
  } else if (!memcmp(blob, (uint8_t[]) { 'B', 'M', 'F', 3 }, 4)) {
    return NULL; // TODO
  } else {
    return NULL;
  }
}

Rasterizer* lovrRasterizerCreate(Blob* blob, float size, RasterizerIO* io) {
  Rasterizer* rasterizer = NULL;
  if ((rasterizer = lovrRasterizerCreateTTF(blob, size)) != NULL) return rasterizer;
  if ((rasterizer = lovrRasterizerCreateBMF(blob, io)) != NULL) return rasterizer;
  lovrThrow("Problem loading font: not recognized as TTF or BMFont");
  return NULL;
}

void lovrRasterizerDestroy(void* ref) {
  Rasterizer* rasterizer = ref;
  lovrRelease(rasterizer->blob, lovrBlobDestroy);
  lovrRelease(rasterizer->atlas, lovrImageDestroy);
  map_free(&rasterizer->kerning);
  lovrFree(rasterizer);
}

float lovrRasterizerGetFontSize(Rasterizer* rasterizer) {
  return rasterizer->size;
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

bool lovrRasterizerIsGlyphEmpty(Rasterizer* rasterizer, uint32_t codepoint) {
  return stbtt_IsGlyphEmpty(&rasterizer->font, stbtt_FindGlyphIndex(&rasterizer->font, codepoint));
}

float lovrRasterizerGetAscent(Rasterizer* rasterizer) {
  return rasterizer->ascent;
}

float lovrRasterizerGetDescent(Rasterizer* rasterizer) {
  return rasterizer->descent;
}

float lovrRasterizerGetLeading(Rasterizer* rasterizer) {
  return rasterizer->leading;
}

float lovrRasterizerGetAdvance(Rasterizer* rasterizer, uint32_t codepoint) {
  int advance, bearing;
  stbtt_GetCodepointHMetrics(&rasterizer->font, codepoint, &advance, &bearing);
  return advance * rasterizer->scale;
}

float lovrRasterizerGetBearing(Rasterizer* rasterizer, uint32_t codepoint) {
  int advance, bearing;
  stbtt_GetCodepointHMetrics(&rasterizer->font, codepoint, &advance, &bearing);
  return bearing * rasterizer->scale;
}

float lovrRasterizerGetKerning(Rasterizer* rasterizer, uint32_t first, uint32_t second) {
  uint32_t codepoints[] = { first, second };
  uint64_t hash = hash64(codepoints, sizeof(codepoints));
  uint64_t kerning = map_get(&rasterizer->kerning, hash);

  if (kerning == MAP_NIL) {
    kerning = stbtt_GetCodepointKernAdvance(&rasterizer->font, first, second);
    map_set(&rasterizer->kerning, hash, kerning);
  }

  return (int32_t) kerning * rasterizer->scale;
}

void lovrRasterizerGetBoundingBox(Rasterizer* rasterizer, float box[4]) {
  int x0, y0, x1, y1;
  stbtt_GetFontBoundingBox(&rasterizer->font, &x0, &y0, &x1, &y1);
  box[0] = x0 * rasterizer->scale;
  box[1] = y0 * rasterizer->scale;
  box[2] = x1 * rasterizer->scale;
  box[3] = y1 * rasterizer->scale;
}

void lovrRasterizerGetGlyphBoundingBox(Rasterizer* rasterizer, uint32_t codepoint, float box[4]) {
  int x0, y0, x1, y1;
  stbtt_GetCodepointBox(&rasterizer->font, codepoint, &x0, &y0, &x1, &y1);
  box[0] = x0 * rasterizer->scale;
  box[1] = y0 * rasterizer->scale;
  box[2] = x1 * rasterizer->scale;
  box[3] = y1 * rasterizer->scale;
}

bool lovrRasterizerGetCurves(Rasterizer* rasterizer, uint32_t codepoint, void (*fn)(void* context, uint32_t degree, float* points), void* context) {
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

bool lovrRasterizerGetPixels(Rasterizer* rasterizer, uint32_t codepoint, float* pixels, uint32_t width, uint32_t height, double spread) {
  int id = stbtt_FindGlyphIndex(&rasterizer->font, codepoint);

  if (!id || stbtt_IsGlyphEmpty(&rasterizer->font, id)) {
    return false;
  }

  stbtt_vertex* vertices;
  int count = stbtt_GetGlyphShape(&rasterizer->font, id, &vertices);

  msShape* shape = msShapeCreate();
  msContour* contour = NULL;

  float x = 0.f;
  float y = 0.f;
  float scale = rasterizer->scale;
  for (int i = 0; i < count; i++) {
    stbtt_vertex vertex = vertices[i];
    float x2 = vertex.x * scale;
    float y2 = vertex.y * scale;

    if (vertex.type == STBTT_vmove) {
      contour = msShapeAddContour(shape);
    } else if (vertex.type == STBTT_vline) {
      msContourAddLinearEdge(contour, x, y, x2, y2);
    } else if (vertex.type == STBTT_vcurve) {
      float cx = vertex.cx * scale;
      float cy = vertex.cy * scale;
      msContourAddQuadraticEdge(contour, x, y, cx, cy, x2, y2);
    } else if (vertex.type == STBTT_vcubic) {
      float cx1 = vertex.cx * scale;
      float cy1 = vertex.cy * scale;
      float cx2 = vertex.cx1 * scale;
      float cy2 = vertex.cy1 * scale;
      msContourAddCubicEdge(contour, x, y, cx1, cy1, cx2, cy2, x2, y2);
    }

    x = x2;
    y = y2;
  }

  stbtt_FreeShape(&rasterizer->font, vertices);

  int x0, y0, x1, y1;
  stbtt_GetGlyphBox(&rasterizer->font, id, &x0, &y0, &x1, &y1);

  uint32_t padding = ceil(spread / 2.);
  float offsetX = -x0 * scale + padding;
  float offsetY = -y0 * scale + padding;

  msShapeNormalize(shape);
  msShapeOrientContours(shape);
  msEdgeColoringSimple(shape, 3., 0);
  msGenerateMTSDF(pixels, width, height, shape, spread, 1.f, 1.f, offsetX, offsetY);
  msShapeDestroy(shape);

  return true;
}
