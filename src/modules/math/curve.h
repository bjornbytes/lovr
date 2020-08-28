#include <stdint.h>
#include <stddef.h>

typedef struct Curve Curve;
Curve* lovrCurveCreate(void);
void lovrCurveDestroy(void* ref);
void lovrCurveEvaluate(Curve* curve, float t, float point[4]);
void lovrCurveGetTangent(Curve* curve, float t, float point[4]);
Curve* lovrCurveSlice(Curve* curve, float t1, float t2);
size_t lovrCurveGetPointCount(Curve* curve);
void lovrCurveGetPoint(Curve* curve, size_t index, float point[4]);
void lovrCurveSetPoint(Curve* curve, size_t index, float point[4]);
void lovrCurveAddPoint(Curve* curve, float point[4], size_t index);
void lovrCurveRemovePoint(Curve* curve, size_t index);
