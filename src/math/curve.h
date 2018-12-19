#include "util.h"
#include "lib/math.h"
#include "lib/vec/vec.h"

typedef struct {
  Ref ref;
  vec_float_t points;
} Curve;

Curve* lovrCurveInit(Curve* curve, int sizeHint);
#define lovrCurveCreate(...) lovrCurveInit(lovrAlloc(Curve), __VA_ARGS__)
void lovrCurveDestroy(void* ref);
void lovrCurveEvaluate(Curve* curve, float t, vec3 point);
void lovrCurveGetTangent(Curve* curve, float t, vec3 point);
void lovrCurveRender(Curve* curve, float t1, float t2, vec3 points, int n);
Curve* lovrCurveSplit(Curve* curve, float t1, float t2);
int lovrCurveGetPointCount(Curve* curve);
void lovrCurveGetPoint(Curve* curve, int index, vec3 point);
void lovrCurveSetPoint(Curve* curve, int index, vec3 point);
void lovrCurveAddPoint(Curve* curve, vec3 point, int index);
void lovrCurveRemovePoint(Curve* curve, int index);
