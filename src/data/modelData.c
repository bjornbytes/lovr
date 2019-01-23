#include "data/modelData.h"

ModelData* lovrModelDataInit(ModelData* model, Blob* source, ModelDataIO io) {
  if (lovrModelDataInitGltf(model, source, io)) {
    return model;
  } else if (lovrModelDataInitObj(model, source, io)) {
    return model;
  }

  lovrThrow("Unable to load model from '%s'", source->name);
  return NULL;
}

void lovrModelDataDestroy(void* ref) {
  ModelData* model = ref;
  for (int i = 0; i < model->blobCount; i++) {
    lovrRelease(model->blobs[i]);
  }
  for (int i = 0; i < model->imageCount; i++) {
    lovrRelease(model->images[i]);
  }
  free(model->data);
}

// Note: this code is a scary optimization
void lovrModelDataAllocate(ModelData* model) {
  size_t totalSize = 0;
  size_t sizes[14];
  totalSize += sizes[0] = model->blobCount * sizeof(Blob*);
  totalSize += sizes[1] = model->imageCount * sizeof(TextureData*);
  totalSize += sizes[2] = model->animationCount * sizeof(ModelAnimation);
  totalSize += sizes[3] = model->attributeCount * sizeof(ModelAttribute);
  totalSize += sizes[4] = model->bufferCount * sizeof(ModelBuffer);
  totalSize += sizes[5] = model->textureCount * sizeof(ModelTexture);
  totalSize += sizes[6] = model->materialCount * sizeof(ModelMaterial);
  totalSize += sizes[7] = model->primitiveCount * sizeof(ModelPrimitive);
  totalSize += sizes[8] = model->nodeCount * sizeof(ModelNode);
  totalSize += sizes[9] = model->skinCount * sizeof(ModelSkin);
  totalSize += sizes[10] = model->channelCount * sizeof(ModelAnimationChannel);
  totalSize += sizes[11] = model->childCount * sizeof(uint32_t);
  totalSize += sizes[12] = model->jointCount * sizeof(uint32_t);
  totalSize += sizes[13] = model->charCount * sizeof(char);

  size_t offset = 0;
  char* p = model->data = calloc(1, totalSize);
  lovrAssert(model->data, "Out of memory");
  model->blobs = (Blob**) (p + offset), offset += sizes[0];
  model->images = (TextureData**) (p + offset), offset += sizes[1];
  model->animations = (ModelAnimation*) (p + offset), offset += sizes[2];
  model->attributes = (ModelAttribute*) (p + offset), offset += sizes[3];
  model->buffers = (ModelBuffer*) (p + offset), offset += sizes[4];
  model->textures = (ModelTexture*) (p + offset), offset += sizes[5];
  model->materials = (ModelMaterial*) (p + offset), offset += sizes[6];
  model->primitives = (ModelPrimitive*) (p + offset), offset += sizes[7];
  model->nodes = (ModelNode*) (p + offset), offset += sizes[8];
  model->skins = (ModelSkin*) (p + offset), offset += sizes[9];
  model->channels = (ModelAnimationChannel*) (p + offset), offset += sizes[10];
  model->children = (uint32_t*) (p + offset), offset += sizes[11];
  model->joints = (uint32_t*) (p + offset), offset += sizes[12];
  model->chars = (char*) (p + offset), offset += sizes[13];
}
