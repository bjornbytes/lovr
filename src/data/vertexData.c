#include "data/vertexData.h"
#include <string.h>

static const size_t attributeTypeSizes[3] = { 4, 1, 4 };

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
    vertexFormatAppend(&vertexData->format, "lovrPosition", ATTR_FLOAT, 3);
    vertexFormatAppend(&vertexData->format, "lovrNormal", ATTR_FLOAT, 3);
    vertexFormatAppend(&vertexData->format, "lovrTexCoord", ATTR_FLOAT, 2);
    vertexFormatAppend(&vertexData->format, "lovrVertexColor", ATTR_BYTE, 4);
  }

  size_t size = format->stride * count;
  vertexData->blob.data = calloc(1, size);
  vertexData->blob.size = size;
  vertexData->count = count;

  return vertexData;
}
