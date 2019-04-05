#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#pragma once

#ifdef _WIN32
#define LOVR_EXPORT __declspec(dllexport)
#else
#define LOVR_EXPORT
#endif

#ifndef _Noreturn
#ifdef _WIN32
#define _Noreturn  __declspec(noreturn)
#else
#define _Noreturn __attribute__((noreturn))
#endif
#endif

#ifndef _Thread_local
#ifdef _WIN32
#define _Thread_local  __declspec(thread)
#else
#define _Thread_local __thread
#endif
#endif

#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) ((uintptr_t) p & -n)

#ifndef M_PI
#define M_PI 3.14159265358979323846264f
#endif

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

typedef struct { float r, g, b, a; } Color;

typedef void (*lovrErrorHandler)(void* userdata, const char* format, va_list args);
extern _Thread_local lovrErrorHandler lovrErrorCallback;
extern _Thread_local void* lovrErrorUserdata;

void lovrSetErrorCallback(lovrErrorHandler callback, void* context);
void _Noreturn lovrThrow(const char* format, ...);
void* _lovrAlloc(size_t size, Type type);
size_t utf8_decode(const char *s, const char *e, unsigned *pch);

#define lovrAssert(c, ...) if (!(c)) { lovrThrow(__VA_ARGS__); }
#define lovrAlloc(T) (T*) _lovrAlloc(sizeof(T), T_ ## T)
#define lovrRetain(r) if (r && ++(((Ref*) r)->count) >= 0xff) lovrThrow("Ref count overflow")
#define lovrRelease(T, r) if (r && --(((Ref*) r)->count) == 0) lovr ## T ## Destroy(r)
#define lovrGenericRelease(r) if (r && --(((Ref*) r)->count) == 0) lovrTypeInfo[((Ref*) r)->type].destructor(r)
