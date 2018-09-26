#include "blob.h"
#include "data/modelData.h"
#include <stdlib.h>
#include <stdint.h>

#pragma once

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
  int8_t* i8;
  uint8_t* u8;
  int16_t* i16;
  uint16_t* u16;
  int32_t* i32;
  uint32_t* u32;
  float* f32;
} AttributePointer;

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
#define lovrVertexDataCreate(...) lovrVertexDataInit(lovrAlloc(VertexData), __VA_ARGS__)
#define lovrVertexDataDestroy NULL
