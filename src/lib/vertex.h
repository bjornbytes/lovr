#include <stdlib.h>
#include "lib/glfw.h"
#include "lib/vec/vec.h"

#pragma once

#define ATTRIBUTE_COUNT 3

size_t AttributeSizes[ATTRIBUTE_COUNT];
GLenum AttributeConstants[ATTRIBUTE_COUNT];

typedef enum {
  ATTR_FLOAT,
  ATTR_BYTE,
  ATTR_INT
} AttributeType;

typedef struct {
  const char* name;
  AttributeType type;
  int count;
} Attribute;

typedef vec_t(Attribute) VertexFormat;
