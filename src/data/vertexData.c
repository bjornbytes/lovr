#include "data/vertexData.h"
#include <string.h>

static const size_t attributeTypeSizes[] = {
  [I8] = 1,
  [U8] = 1,
  [I16] = 2,
  [U16] = 2,
  [I32] = 4,
  [U32] = 4,
  [F32] = 4
};

void vertexFormatInit(VertexFormat* format) {
  memset(format, 0, sizeof(*format));
}

void vertexFormatAppend(VertexFormat* format, const char* name, AttributeType type, int count) {
  size_t size = attributeTypeSizes[type];
  Attribute attribute = { name, type, count, size, format->stride };
  format->attributes[format->count++] = attribute;
  format->stride += size * count;
}

VertexData* lovrVertexDataInit(VertexData* vertexData, uint32_t count, VertexFormat* format) {
  if (format) {
    vertexData->format = *format;
  } else {
    format = &vertexData->format;
    vertexFormatInit(&vertexData->format);
    vertexFormatAppend(&vertexData->format, "lovrPosition", F32, 3);
    vertexFormatAppend(&vertexData->format, "lovrNormal", F32, 3);
    vertexFormatAppend(&vertexData->format, "lovrTexCoord", F32, 2);
    vertexFormatAppend(&vertexData->format, "lovrVertexColor", U8, 4);
  }

  size_t size = format->stride * count;
  vertexData->blob.data = calloc(1, size);
  vertexData->blob.size = size;
  vertexData->count = count;

  return vertexData;
}
