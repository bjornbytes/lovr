#pragma once

typedef struct {
  int width;
  int height;
} Glyph;

typedef struct {
  void* face;
} FontData;

typedef struct {
  FontData fontData;
} Font;
