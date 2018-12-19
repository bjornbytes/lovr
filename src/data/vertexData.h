#include "blob.h"
#include <stdlib.h>
#include <stdint.h>

#pragma once

typedef enum {
  ATTR_FLOAT,
  ATTR_BYTE,
  ATTR_INT
} AttributeType;

typedef struct {
  const char* name;
  AttributeType type;
  int count;
  size_t size;
  size_t offset;
} Attribute;

typedef struct {
  Attribute attributes[8];
  size_t stride;
  int count;
} VertexFormat;

typedef union {
  void* raw;
  float* floats;
  uint8_t* bytes;
  int* ints;
} VertexPointer;

typedef union {
  void* raw;
  uint16_t* shorts;
  uint32_t* ints;
} IndexPointer;

typedef struct {
  Blob blob;
  VertexFormat format;
  uint32_t count;
} VertexData;

void vertexFormatInit(VertexFormat* format);
void vertexFormatAppend(VertexFormat* format, const char* name, AttributeType type, int count);

VertexData* lovrVertexDataInit(VertexData* vertexData, uint32_t count, VertexFormat* format);
#define lovrVertexDataCreate(...) lovrVertexDataInit(lovrAlloc(VertexData, lovrVertexDataDestroy), __VA_ARGS__)
#define lovrVertexDataDestroy lovrBlobDestroy
