#include "data/vertexData.h"
#include <string.h>
#include <stdio.h>

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

VertexData* lovrVertexDataCreate(uint32_t count, VertexFormat* format, bool allocate) {
  VertexData* vertexData = lovrAlloc(sizeof(VertexData), lovrVertexDataDestroy);
  if (!vertexData) return NULL;

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

  vertexData->count = count;
  vertexData->data.raw = NULL;

  if (allocate) {
    vertexData->data.raw = malloc(format->stride * count);
    memset(vertexData->data.raw, 0, format->stride * count);
  }

  return vertexData;
}

void lovrVertexDataDestroy(void* ref) {
  VertexData* vertexData = ref;
  if (vertexData->data.raw) {
    free(vertexData->data.raw);
  }
  free(vertexData);
}
