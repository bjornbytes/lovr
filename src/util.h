#include <stdatomic.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#pragma once

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 18
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Dream Eater"

#ifndef M_PI
#define M_PI 3.14159265358979
#endif

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define CLAMP(x, min, max) MAX(min, MIN(max, x))
#define ALIGN(p, n) (((uintptr_t) (p) + (n - 1)) & ~(n - 1))
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))
#define CHECK_SIZEOF(T) int(*_o)[sizeof(T)]=1
#define BREAK() __asm("int $3")

// Allocation
#if defined(__clang__) || defined(__GNUC__)
void* lovrMalloc(size_t size) __attribute__((malloc, returns_nonnull, alloc_size(1)));
void* lovrCalloc(size_t size) __attribute__((malloc, returns_nonnull, alloc_size(1)));
void* lovrRealloc(void* data, size_t size) __attribute__((returns_nonnull, alloc_size(2)));
void lovrFree(void* data);
#else
void* lovrMalloc(size_t size);
void* lovrCalloc(size_t size);
void* lovrRealloc(void* data, size_t size);
void lovrFree(void* data);
#endif

#define lovrStrdup(s) (s ? memcpy(lovrMalloc(strlen(s) + 1), s, strlen(s) + 1) : NULL)

// Module
bool lovrModuleAcquire(atomic_uint* ref);
bool lovrModuleRelease(atomic_uint* ref);
void lovrModuleReady(atomic_uint* ref);
void lovrModuleReset(atomic_uint* ref);

// Refcounting (to be refcounted, a struct must have an atomic_uint refcount as its first field)
void lovrRetain(void* ref);
void lovrRelease(void* ref, void (*destructor)(void*));

// Errors
const char* lovrGetError(void);
int lovrSetError(const char* format, ...);

#define lovrUnreachable() abort()
#define lovrAssert(c, ...) do { if (!(c)) { lovrSetError(__VA_ARGS__); return 0; } } while (0)
#define lovrAssertGoto(label, c, ...) do { if (!(c)) { lovrSetError(__VA_ARGS__); goto label; } } while (0)
#ifdef LOVR_UNCHECKED
#define lovrCheck(c, ...) ((void) 0)
#define lovrCheckGoto(label, c, ...) ((void) 0)
#else
#define lovrCheck lovrAssert
#define lovrCheckGoto lovrAssertGoto
#endif

// Logging
typedef void fn_log(void*, int, const char*, const char*, va_list);
enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
void lovrSetLogCallback(fn_log* callback, void* userdata);
void lovrLog(int level, const char* tag, const char* format, ...);

// Profiling
#ifdef LOVR_PROFILE
#include <TracyC.h>
#define lovrProfileMarkFrame() TracyCFrameMark
#define lovrProfileSetThreadName(name) TracyCSetThreadName(name)
#define lovrProfileStart(id, label) TracyCZoneN(id, label, true)
#define lovrProfileEnd(id) TracyCZoneEnd(id)
#define lovrProfileAlloc(p, size) TracyCAlloc(p, size)
#define lovrProfileFree(p) TracyCFree(p)
#else
#define lovrProfileMarkFrame() ((void) 0)
#define lovrProfileSetThreadName(name) ((void) 0)
#define lovrProfileStart(id, label) ((void) 0)
#define lovrProfileEnd(id) ((void) 0)
#define lovrProfileAlloc(p, size) ((void) 0)
#define lovrProfileFree(p) ((void) 0)
#endif

// Dynamic Array
#define arr_t(T) struct { T* data; size_t length, capacity; }
#define arr_init(a) (a)->data = NULL, (a)->length = 0, (a)->capacity = 0
#define arr_free(a) if ((a)->data) lovrFree((a)->data)
#define arr_reserve(a, n) _arr_reserve((void**) &((a)->data), n, &(a)->capacity, sizeof(*(a)->data))
#define arr_expand(a, n) arr_reserve(a, (a)->length + n)
#define arr_push(a, x) arr_reserve(a, (a)->length + 1), (a)->data[(a)->length] = x, (a)->length++
#define arr_pop(a) (a)->data[--(a)->length]
#define arr_append(a, p, n) arr_reserve(a, (a)->length + n), memcpy((a)->data + (a)->length, p, n * sizeof(*(a)->data)), (a)->length += n
#define arr_splice(a, i, n) memmove((a)->data + (i), (a)->data + ((i) + n), ((a)->length - (i) - (n)) * sizeof(*(a)->data)), (a)->length -= n
#define arr_clear(a) (a)->length = 0

static inline void _arr_reserve(void** data, size_t n, size_t* capacity, size_t stride) {
  if (*capacity >= n) return;
  if (*capacity == 0) *capacity = 1;
  while (*capacity < n) *capacity *= 2;
  *data = lovrRealloc(*data, *capacity * stride);
}

// Hash function
uint64_t hash64(const void* data, size_t length);

// Hashmap (does not support removal)
typedef struct {
  uint64_t* hashes;
  uint64_t* values;
  uint32_t size;
  uint32_t used;
} map_t;

#define MAP_NIL UINT64_MAX

void map_init(map_t* map, uint32_t n);
void map_free(map_t* map);
uint64_t map_get(map_t* map, uint64_t hash);
void map_set(map_t* map, uint64_t hash, uint64_t value);

// UTF-8
size_t utf8_decode(const char *s, const char *e, unsigned *pch);
void utf8_encode(uint32_t codepoint, char str[4]);

// f16
typedef float float32;
typedef uint16_t float16;
void float16Init(void);
float16 float32to16(float32 f);
float32 float16to32(float16 f);

// Types
typedef enum {
  T_NONE,
  T_Source,
  T_AudioMesh,
  T_AudioStream,
  T_Blob,
  T_Image,
  T_ModelData,
  T_Rasterizer,
  T_Sound,
  T_File,
  T_Buffer,
  T_Texture,
  T_Sampler,
  T_Shader,
  T_Material,
  T_Font,
  T_Mesh,
  T_Model,
  T_Raytracer,
  T_Readback,
  T_Pass,
  T_Layer,
  T_Curve,
  T_Mat4,
  T_RandomGenerator,
  T_World,
  T_Collider,
  T_Contact,
  T_BoxShape,
  T_SphereShape,
  T_CapsuleShape,
  T_CylinderShape,
  T_ConvexShape,
  T_MeshShape,
  T_TerrainShape,
  T_WeldJoint,
  T_BallJoint,
  T_ConeJoint,
  T_DistanceJoint,
  T_HingeJoint,
  T_SliderJoint,
  T_Thread,
  T_Channel,
  T_COUNT
} ObjectType;

typedef struct {
  const char* name;
  void (*destructor)(void*);
} TypeInfo;

extern TypeInfo lovrTypeInfo[T_COUNT];

// Variant
typedef enum {
  TYPE_NIL,
  TYPE_BOOLEAN,
  TYPE_NUMBER,
  TYPE_STRING,
  TYPE_MINISTRING,
  TYPE_POINTER,
  TYPE_OBJECT,
  TYPE_VECTOR,
  TYPE_QUATERNION,
  TYPE_TABLE
} VariantType;

typedef union Variant {
  struct { VariantType type; };
  struct { VariantType type; bool value; } boolean;
  struct { VariantType type; double value; } number;
  struct { VariantType type; void* value; } pointer;
  struct { VariantType type; uint32_t length; char* pointer; } string;
  struct { VariantType type; uint8_t length; char data[11]; } ministring;
  struct { VariantType _type; ObjectType type; void* pointer; } object;
  struct { VariantType type; float data[3]; } vector;
  struct { VariantType type; int16_t data[4]; } quaternion;
  struct { VariantType type; uint32_t count; union Variant* pairs; } table;
} Variant;

void lovrVariantDestroy(Variant* variant);
