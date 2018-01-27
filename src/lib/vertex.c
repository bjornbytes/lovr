#include "lib/vertex.h"
#include <string.h>

static size_t attributeTypeSizes[3] = { 4, 1, 4 };

void vertexFormatInit(VertexFormat* format) {
  memset(format, 0, sizeof(*format));
}

void vertexFormatAppend(VertexFormat* format, const char* name, AttributeType type, int count) {
  size_t size = attributeTypeSizes[type];
  Attribute attribute = { name, type, count, size, format->stride };
  format->attributes[format->count++] = attribute;
  format->stride += size * count;
}
