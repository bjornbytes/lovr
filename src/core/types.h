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
  T_Curve,
  T_CylinderShape,
  T_DistanceJoint,
  T_File,
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

// If the thread module is enabled, we have to use atomic operations when modifying refcounts, which
// are a bit slower and vary depending on the platform.
#ifdef LOVR_ENABLE_THREAD
  #ifdef _WIN32
    #include <intrin.h>
    #define Refcount int
    #define refcount_increment(c) _InterlockedIncrement(&c)
    #define refcount_decrement(c) _InterlockedDecrement(&c)
  #else
    #include <stdatomic.h>
    #define Refcount _Atomic int
    #define refcount_increment(c) (atomic_fetch_add(&c, 1) + 1)
    #define refcount_decrement(c) (atomic_fetch_sub(&c, 1) - 1)
  #endif
#else
  #define Refcount int
  #define refcount_increment(c) ++c
  #define refcount_decrement(c) --c
#endif

typedef struct Ref {
  Type type;
  Refcount count;
} Ref;

void* _lovrAlloc(size_t size, Type type);
#define _ref(o) ((Ref*) o - 1)
#define lovrAlloc(T) (T*) _lovrAlloc(sizeof(T), T_ ## T)
#define lovrRetain(o) if (o && refcount_increment(_ref(o)->count) >= 0xff) lovrThrow("Ref count overflow")
#define lovrRelease(T, o) if (o && refcount_decrement(_ref(o)->count) == 0) lovr ## T ## Destroy(o), free(_ref(o))
#define lovrGenericRelease(o) if (o && refcount_decrement(_ref(o)->count) == 0) lovrTypeInfo[_ref(o)->type].destructor(o), free(_ref(o))
#define _lovrRelease(f, o) if (o && refcount_decrement(_ref(o)->count) == 0) f(o), free(_ref(o))
