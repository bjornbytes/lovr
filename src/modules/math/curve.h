#include "core/arr.h"
#include "core/maf.h"

typedef struct {
  arr_t(float, 16) points;
} Curve;

Curve* lovrCurveInit(Curve* curve);
#define lovrCurveCreate(...) lovrCurveInit(lovrAlloc(Curve))
void lovrCurveDestroy(void* ref);
void lovrCurveEvaluate(Curve* curve, float t, vec3 point);
void lovrCurveGetTangent(Curve* curve, float t, vec3 point);
void lovrCurveRender(Curve* curve, float t1, float t2, vec3 points, uint32_t n);
Curve* lovrCurveSlice(Curve* curve, float t1, float t2);
size_t lovrCurveGetPointCount(Curve* curve);
void lovrCurveGetPoint(Curve* curve, size_t index, vec3 point);
void lovrCurveSetPoint(Curve* curve, size_t index, vec3 point);
void lovrCurveAddPoint(Curve* curve, vec3 point, size_t index);
void lovrCurveRemovePoint(Curve* curve, size_t index);
