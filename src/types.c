#include "types.h"
#include "util.h"

const Type lovrSupertypes[] = {
  [T_BallJoint] = T_Joint,
  [T_BoxShape] = T_Shape,
  [T_CapsuleShape] = T_Shape,
  [T_CylinderShape] = T_Shape,
  [T_DistanceJoint] = T_Joint,
  [T_HingeJoint] = T_Joint,
  [T_SliderJoint] = T_Joint,
  [T_SoundData] = T_Blob,
  [T_SphereShape] = T_Shape,
  [T_TextureData] = T_Blob
};

Ref* _lovrAlloc(size_t size) {
  Ref* ref = calloc(1, size);
  lovrAssert(ref, "Out of memory");
  ref->count = 1;
  return ref;
}
