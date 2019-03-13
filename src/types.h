#include <stdlib.h>
#include <stdint.h>

#pragma once

typedef enum {
  T_NONE = 0,
  T_vec3,
  T_quat,
  T_mat4,
  T_Animator,
  T_AudioStream,
  T_BallJoint,
  T_Blob,
  T_BoxShape,
  T_Buffer,
  T_Canvas,
  T_CapsuleShape,
  T_Channel,
  T_Collider,
  T_Controller,
  T_Curve,
  T_CylinderShape,
  T_DistanceJoint,
  T_Font,
  T_HingeJoint,
  T_Joint,
  T_Material,
  T_Mesh,
  T_Microphone,
  T_Model,
  T_ModelData,
  T_Pool,
  T_RandomGenerator,
  T_Rasterizer,
  T_Shader,
  T_ShaderBlock,
  T_Shape,
  T_SliderJoint,
  T_SoundData,
  T_Source,
  T_SphereShape,
  T_Texture,
  T_TextureData,
  T_Thread,
  T_World,
  T_MAX
} Type;

typedef struct { uint8_t count; } Ref;
typedef void lovrDestructor(void*);

extern const Type lovrSupertypes[T_MAX];

Ref* _lovrAlloc(size_t size);
#define lovrAlloc(T) (T*) _lovrAlloc(sizeof(T))
#define lovrRetain(R) if (R && ++(((Ref*) R)->count) >= 0xff) lovrThrow("Ref count overflow")
#define lovrRelease(T, R) if (R && --(((Ref*) R)->count) == 0) lovr ## T ## Destroy(R), free(R)
