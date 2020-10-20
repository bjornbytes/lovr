#include <stdbool.h>
#include <stdint.h>

#pragma once

bool lovrGraphicsInit(bool debug);
void lovrGraphicsDestroy(void);

void lovrGraphicsPush(void);
void lovrGraphicsPop(void);
void lovrGraphicsOrigin(void);
void lovrGraphicsTranslate(float* translation);
void lovrGraphicsRotate(float* rotation);
void lovrGraphicsScale(float* scale);
void lovrGraphicsMatrixTransform(float* transform);
