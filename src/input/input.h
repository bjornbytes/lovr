#pragma once

typedef enum {
  MOUSE_BUTTON_LEFT,
  MOUSE_BUTTON_RIGHT,
  MOUSE_BUTTON_MIDDLE,
} MouseButton;

void lovrInputInit();
void lovrInputGetMousePosition(float *x, float *y);
int lovrInputIsMouseDown(MouseButton button);

