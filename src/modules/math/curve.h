#include <stddef.h>

typedef struct Curve Curve;
Curve* lovrCurveCreate(void);
void lovrCurveDestroy(void* ref);
void lovrCurveEvaluate(Curve* curve, float t, float* point);
void lovrCurveGetTangent(Curve* curve, float t, float* point);
Curve* lovrCurveSlice(Curve* curve, float t1, float t2);
size_t lovrCurveGetPointCount(Curve* curve);
void lovrCurveGetPoint(Curve* curve, size_t index, float* point);
void lovrCurveSetPoint(Curve* curve, size_t index, float* point);
void lovrCurveAddPoint(Curve* curve, float* point, size_t index);
void lovrCurveRemovePoint(Curve* curve, size_t index);
