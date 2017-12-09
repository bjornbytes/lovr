#include "graphics/texture.h"
#include "util.h"

typedef enum {
  CANVAS_3D,
  CANVAS_2D
} CanvasType;

typedef struct {
  Texture texture;
  CanvasType type;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthStencilBuffer;
  GLuint msaaTexture;
  int msaa;
} Canvas;

bool lovrCanvasSupportsFormat(TextureFormat format);

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, CanvasType type, int msaa, bool depth, bool stencil);
void lovrCanvasDestroy(const Ref* ref);
void lovrCanvasBind(Canvas* canvas);
void lovrCanvasResolveMSAA(Canvas* canvas);
TextureFormat lovrCanvasGetFormat(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
