#include "input/input.h"
#include "graphics/graphics.h"
//#include "lib/glfw.h"

void lovrInputInit() {
}

// get mouse position in pixel coords relative to top left of window.
// can return coords outside window (eg -ve or greater than window dimensions).
void lovrInputGetMousePosition(float *x, float *y) {
  GLFWwindow *window = lovrGraphicsPrivateGetWindow();
  if (!window) {
    *x=0.0f;
    *y=0.0f;
    return;
  }
  double mx,my;
  glfwGetCursorPos(window,&mx,&my);
  *x = (float)mx;
  *y = (float)my;
}

int lovrInputIsMouseDown(MouseButton button) {
  int b;
  GLFWwindow *window = lovrGraphicsPrivateGetWindow();
  if (!window) {
    return 0;
  }
  switch (button) {
    case MOUSE_BUTTON_LEFT: b=GLFW_MOUSE_BUTTON_LEFT; break;
    case MOUSE_BUTTON_RIGHT: b=GLFW_MOUSE_BUTTON_RIGHT; break;
    case MOUSE_BUTTON_MIDDLE: b=GLFW_MOUSE_BUTTON_MIDDLE; break;
    default: return 0;
  }
  return (glfwGetMouseButton(window,b) == GLFW_PRESS) ? 1:0;
}
