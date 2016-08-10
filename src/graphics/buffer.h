#include "../glfw.h"
#include "../vendor/map/map.h"

#ifndef LOVR_BUFFER_TYPES
#define LOVR_BUFFER_TYPES
typedef struct {
  int size;
  GLfloat* data;
  const char* drawMode;
  GLuint vao;
  GLuint vbo;
  int rangeStart;
  int rangeCount;
} Buffer;
#endif

map_int_t BufferDrawModes;

void lovrBufferDestroy(Buffer* buffer);
void lovrBufferDraw(Buffer* buffer);
const char* lovrBufferGetDrawMode(Buffer* buffer);
int lovrBufferSetDrawMode(Buffer* buffer, const char* drawMode);
void lovrBufferGetVertex(Buffer* buffer, int index, float* x, float* y, float* z);
void lovrBufferSetVertex(Buffer* buffer, int index, float x, float y, float z);
void lovrBufferGetDrawRange(Buffer* buffer, int* start, int* count);
int lovrBufferSetDrawRange(Buffer* buffer, int start, int count);
