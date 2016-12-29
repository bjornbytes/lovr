#include "graphics/model.h"
#ifndef EMSCRIPTEN
#include "openvr.h"
#endif

ModelData* lovrModelDataFromFile(void* data, int size);
#ifndef EMSCRIPTEN
ModelData* lovrModelDataFromOpenVRModel(OpenVRModel* vrModel);
#endif
