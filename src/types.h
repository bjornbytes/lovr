#include <stddef.h>

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

typedef void lovrDestructor(void*);
typedef struct {
  const char* name;
  lovrDestructor* destructor;
  Type super;
} TypeInfo;

extern const TypeInfo lovrTypeInfo[T_MAX];

typedef struct {
  Type type;
  int count;
} Ref;

Ref* _lovrAlloc(size_t size, Type type);

#define lovrAlloc(T) (T*) _lovrAlloc(sizeof(T), T_ ## T)
#define lovrRetain(r) if (r && ++(((Ref*) r)->count) >= 0xff) lovrThrow("Ref count overflow")
#define lovrRelease(T, r) if (r && --(((Ref*) r)->count) == 0) lovr ## T ## Destroy(r), free(r)
#define lovrGenericRelease(r) if (r && --(((Ref*) r)->count) == 0) lovrTypeInfo[((Ref*) r)->type].destructor(r), free(r)
