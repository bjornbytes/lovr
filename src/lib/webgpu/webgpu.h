#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#include <emscripten/wasm_worker.h>
#endif

#ifdef __clang__
// The internal struct member offset layout is extremely important when marshalling structs to JS,
// so never let the compiler add any padding (we manually & explicitly make the fields the right size)
#pragma clang diagnostic push
#elif defined(_MSC_VER)
#pragma warning(push)
#endif

#ifdef __EMSCRIPTEN__
// In Web builds we have JavaScript code that reads/writes the structs, so verify that their binary layout
// will not generate any padding or other unexpected offsets.
#pragma clang diagnostic error "-Wpadded"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4200) // Disable MSVC complaining about zero-sized arrays for copy/move assignment in WGpuCompilationInfo
#endif

#include <stdint.h>

// The type 'double_int53_t' shall be an integer-like
// type that can represent at least 53-bits of consecutive
// unsigned integers. On the web, this will be a 'double',
// on other platforms it will be a 64-bit unsigned integer.
// The reason for using double on the web is due to the
// JavaScript WebGPU API does not allow utilizing
// BigInt to represent real 64-bit integers, so only 53 bits
// of integer values is available.
#ifdef __EMSCRIPTEN__
// Use double to represent a JavaScript number that can
// address 2^53 == 9007199254740992 = ~9.0 petabytes.
typedef double double_int53_t;
#else
// On other platforms than the web, double_int53_t is a real
// integer type.
typedef uint64_t double_int53_t;
#endif

#ifdef __GNUC__
#define NOTNULL __attribute__((nonnull))
#else
#define NOTNULL
#endif

// The following should be kept sorted in the order of the WebIDL for easier diffing across changes to the spec: https://www.w3.org/TR/webgpu/#idl-index
// with the exception that the callback typedefs should appear last in this file to see the necessary definitions.
#ifdef __EMSCRIPTEN__
typedef int WGpuObjectBase;
#else
typedef struct _WGpuObject *WGpuObjectBase;
#endif

typedef struct WGpuObjectDescriptorBase WGpuObjectDescriptorBase;
typedef struct WGpuSupportedLimits WGpuSupportedLimits;
typedef int WGPU_FEATURES_BITFIELD;
typedef int HTML_PREDEFINED_COLOR_SPACE;
typedef struct WGpuAdapterInfo WGpuAdapterInfo;
typedef int WGPU_FEATURE_LEVEL;
typedef struct WGpuRequestAdapterOptions WGpuRequestAdapterOptions;
typedef int WGPU_POWER_PREFERENCE;
typedef WGpuObjectBase WGpuAdapter;
typedef struct WGpuDeviceDescriptor WGpuDeviceDescriptor;
typedef WGpuObjectBase WGpuDevice;
typedef WGpuObjectBase WGpuBuffer;
typedef int WGPU_BUFFER_MAP_STATE;
typedef struct WGpuBufferDescriptor WGpuBufferDescriptor;
typedef int WGPU_BUFFER_USAGE_FLAGS;
typedef int WGPU_MAP_MODE_FLAGS;
typedef WGpuObjectBase WGpuTexture;
typedef struct WGpuTextureDescriptor WGpuTextureDescriptor;
typedef int WGPU_TEXTURE_DIMENSION;
typedef int WGPU_TEXTURE_USAGE_FLAGS;
typedef WGpuObjectBase WGpuTextureView;
typedef struct WGpuTextureViewDescriptor WGpuTextureViewDescriptor;
typedef int WGPU_TEXTURE_VIEW_DIMENSION;
typedef int WGPU_TEXTURE_ASPECT;
typedef int WGPU_TEXTURE_FORMAT;
typedef WGpuObjectBase WGpuExternalTexture;
typedef struct WGpuExternalTextureDescriptor WGpuExternalTextureDescriptor;
typedef WGpuObjectBase WGpuSampler;
typedef struct WGpuSamplerDescriptor WGpuSamplerDescriptor;
typedef int WGPU_ADDRESS_MODE;
typedef int WGPU_FILTER_MODE;
typedef int WGPU_MIPMAP_FILTER_MODE;
typedef int WGPU_COMPARE_FUNCTION;
typedef WGpuObjectBase WGpuBindGroupLayout;
// Not used:
//typedef struct WGpuBindGroupLayoutDescriptor WGpuBindGroupLayoutDescriptor;
typedef int WGPU_SHADER_STAGE_FLAGS;
typedef struct WGpuBindGroupLayoutEntry WGpuBindGroupLayoutEntry;
typedef int WGPU_BUFFER_BINDING_TYPE;
typedef struct WGpuBufferBindingLayout WGpuBufferBindingLayout;
typedef int WGPU_SAMPLER_BINDING_TYPE;
typedef struct WGpuSamplerBindingLayout WGpuSamplerBindingLayout;
typedef int WGPU_TEXTURE_SAMPLE_TYPE;
typedef struct WGpuTextureBindingLayout WGpuTextureBindingLayout;
typedef int WGPU_STORAGE_TEXTURE_ACCESS;
typedef struct WGpuStorageTextureBindingLayout WGpuStorageTextureBindingLayout;
typedef struct WGpuExternalTextureBindingLayout WGpuExternalTextureBindingLayout;
typedef WGpuObjectBase WGpuBindGroup;
// Not used:
//typedef struct WGpuBindGroupDescriptor WGpuBindGroupDescriptor;
typedef struct WGpuBindGroupEntry WGpuBindGroupEntry;
typedef WGpuObjectBase WGpuPipelineLayout;
// Not used:
//typedef struct WGpuPipelineLayoutDescriptor WGpuPipelineLayoutDescriptor;
typedef WGpuObjectBase WGpuShaderModule;
typedef struct WGpuShaderModuleDescriptor WGpuShaderModuleDescriptor;
typedef int WGPU_COMPILATION_MESSAGE_TYPE;
typedef struct WGpuCompilationMessage WGpuCompilationMessage;
typedef struct WGpuCompilationInfo WGpuCompilationInfo;
typedef int WGPU_AUTO_LAYOUT_MODE;
typedef struct WGpuPipelineConstant WGpuPipelineConstant;
typedef WGpuObjectBase WGpuComputePipeline;
// Not used:
//typedef struct WGpuComputePipelineDescriptor WGpuComputePipelineDescriptor;
typedef WGpuObjectBase WGpuRenderPipeline;
typedef WGpuObjectBase WGpuPipelineBase;
typedef struct WGpuRenderPipelineDescriptor WGpuRenderPipelineDescriptor;
typedef int WGPU_PRIMITIVE_TOPOLOGY;
typedef struct WGpuPrimitiveState WGpuPrimitiveState;
typedef int WGPU_FRONT_FACE;
typedef int WGPU_CULL_MODE;
typedef struct WGpuMultisampleState WGpuMultisampleState;
typedef struct WGpuFragmentState WGpuFragmentState;
typedef struct WGpuColorTargetState WGpuColorTargetState;
typedef struct WGpuBlendState WGpuBlendState;
typedef int WGPU_COLOR_WRITE_FLAGS;
typedef struct WGpuBlendComponent WGpuBlendComponent;
typedef int WGPU_BLEND_FACTOR;
typedef int WGPU_BLEND_OPERATION;
typedef struct WGpuDepthStencilState WGpuDepthStencilState;
typedef struct WGpuStencilFaceState WGpuStencilFaceState;
typedef int WGPU_STENCIL_OPERATION;
typedef int WGPU_INDEX_FORMAT;
typedef int WGPU_VERTEX_FORMAT;
typedef int WGPU_VERTEX_STEP_MODE;
typedef struct WGpuVertexState WGpuVertexState;
typedef struct WGpuVertexBufferLayout WGpuVertexBufferLayout;
typedef struct WGpuVertexAttribute WGpuVertexAttribute;
typedef WGpuObjectBase WGpuCommandBuffer;
typedef struct WGpuCommandBufferDescriptor WGpuCommandBufferDescriptor;
typedef WGpuObjectBase WGpuCommandEncoder;
typedef struct WGpuCommandEncoderDescriptor WGpuCommandEncoderDescriptor;
typedef struct WGpuTexelCopyBufferInfo WGpuTexelCopyBufferInfo;
typedef struct WGpuTexelCopyTextureInfo WGpuTexelCopyTextureInfo;
typedef struct WGpuCopyExternalImageDestInfo WGpuCopyExternalImageDestInfo;
typedef struct WGpuCopyExternalImageSourceInfo WGpuCopyExternalImageSourceInfo;
typedef WGpuObjectBase WGpuBindingCommandsMixin;
typedef WGpuObjectBase WGpuComputePassEncoder;
typedef struct WGpuComputePassDescriptor WGpuComputePassDescriptor;
typedef WGpuObjectBase WGpuRenderCommandsMixin;
typedef WGpuObjectBase WGpuRenderPassEncoder;
typedef struct WGpuRenderPassDescriptor WGpuRenderPassDescriptor;
typedef struct WGpuRenderPassColorAttachment WGpuRenderPassColorAttachment;
typedef struct WGpuRenderPassDepthStencilAttachment WGpuRenderPassDepthStencilAttachment;
typedef int WGPU_LOAD_OP;
typedef int WGPU_STORE_OP;
typedef WGpuObjectBase WGpuRenderBundle;
typedef struct WGpuRenderBundleDescriptor WGpuRenderBundleDescriptor;
typedef WGpuObjectBase WGpuRenderBundleEncoder;
typedef struct WGpuRenderBundleEncoderDescriptor WGpuRenderBundleEncoderDescriptor;
typedef WGpuObjectBase WGpuQueue;
typedef WGpuObjectBase WGpuQuerySet;
typedef struct WGpuQuerySetDescriptor WGpuQuerySetDescriptor;
typedef int WGPU_QUERY_TYPE;
typedef int WGPU_PIPELINE_STATISTIC_NAME;
typedef WGpuObjectBase WGpuCanvasContext;
typedef int WGPU_CANVAS_ALPHA_MODE;
typedef struct WGpuCanvasConfiguration WGpuCanvasConfiguration;
typedef int WGPU_DEVICE_LOST_REASON;
typedef int WGPU_ERROR_FILTER;
typedef int WGPU_ERROR_TYPE;
typedef struct WGpuColor WGpuColor;
typedef struct WGpuOrigin2D WGpuOrigin2D;
typedef struct WGpuOrigin3D WGpuOrigin3D;
typedef struct WGpuExtent3D WGpuExtent3D;
typedef struct WGpuBindGroupLayoutEntry WGpuBindGroupLayoutEntry;
typedef WGpuObjectBase WGpuImageBitmap;
typedef struct WGpuPipelineError WGpuPipelineError;

// Callbacks in the order of appearance in the WebIDL:

typedef void (*WGpuRequestAdapterCallback)(WGpuAdapter adapter, void *userData);
typedef void (*WGpuRequestDeviceCallback)(WGpuDevice device, void *userData);
typedef void (*WGpuCreatePipelineCallback)(WGpuDevice device, WGpuPipelineError *error, WGpuPipelineBase pipeline, void *userData);
typedef void (*WGpuBufferMapCallback)(WGpuBuffer buffer, void *userData, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset, double_int53_t size);
typedef void (*WGpuGetCompilationInfoCallback)(WGpuShaderModule shaderModule, WGpuCompilationInfo *compilationInfo NOTNULL, void *userData);
typedef void (*WGpuOnSubmittedWorkDoneCallback)(WGpuQueue queue, void *userData);
typedef void (*WGpuDeviceLostCallback)(WGpuDevice device, WGPU_DEVICE_LOST_REASON deviceLostReason, const char *message, void *userData);
typedef void (*WGpuDeviceErrorCallback)(WGpuDevice device, WGPU_ERROR_TYPE errorType, const char *errorMessage, void *userData);
typedef void (*WGpuLoadImageBitmapCallback)(WGpuImageBitmap bitmap, int width, int height, void *userData);

#ifdef __EMSCRIPTEN__
typedef int OffscreenCanvasId;
#endif

// Some WebGPU JS API functions have default parameters so that the user can omit passing them.
// These defaults are carried through to these headers. However C does not support default parameters to
// functions, so enable the default parameters only when called from C++ code.
#ifdef __cplusplus
#define _WGPU_DEFAULT_VALUE(x) = x
#else
#define _WGPU_DEFAULT_VALUE(x)
#endif

#ifdef __GNUC__
#define WGPU_NAN __builtin_nan("")
#define WGPU_INFINITY __builtin_inf()
#else
#include <math.h>
#define WGPU_NAN ((double)NAN)
#define WGPU_INFINITY ((double)INFINITY)
#endif

// WGPU_BOOL is a boolean type that has a defined size of 4 bytes to ensure a fixed struct padding.
#define WGPU_BOOL int
#define WGPU_TRUE 1
#define WGPU_FALSE 0

// This macro allows structs that contain pointers to be explicitly aligned up to 8 bytes so that
// even in 32-bit pointer builds, struct alignments are checked to match against Wasm64 builds.
// Invariant: every struct that contains pointers should be annotated with _WGPU_ALIGN_TO_64BITS alignment.
#if __cplusplus >= 201103L
#define _WGPU_ALIGN_TO_64BITS alignas(8)
#else
#define _WGPU_ALIGN_TO_64BITS __attribute__((aligned(8)))
#endif

// The _WGPU_PTR_PADDING() macro pads pointers in 32-bit builds up to 64-bits so that memory layout
// of WebGPU structures is identical in 32-bit and 64-bit builds. This way the JS side marshalling
// can stay the same (except for reading pointers).
#if defined(__wasm64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define _WGPU_PTR_PADDING(x)
#else
#define _WGPU_PTR_PADDING(x) uint32_t unused_padding_to_make_32bit_ptrs_64bit_##x;
#endif

#if defined(__EMSCRIPTEN__) && __cplusplus >= 201103L
#define VERIFY_STRUCT_SIZE(struct_name, size) static_assert(sizeof(struct_name) == (size), "lib_webgpu.js is hardcoded to expect this size. If this changes, modify lib_webgpu.js accordingly. (search for sizeof(..) on struct name)");
#else
#define VERIFY_STRUCT_SIZE(struct_name, size)
#endif

#ifdef __cplusplus
extern "C" {
#endif


// Returns the number of WebGPU objects referenced by the WebGPU JS library. N.b. aim to use this function only for debugging purposes, as it
// takes O(n) time to enumerate through all live objects.
uint32_t wgpu_get_num_live_objects(void);

// Calls .destroy() on the given WebGPU object (if it has such a member function) and releases the JS side reference to it. Use this function
// to release memory for all types of WebGPU objects after you are done with them.
// Note that deleting a GPUTexture will also delete all GPUTextureViews that have been created from it.
// Similar to free(), calling wgpu_object_destroy() on null, or an object that has already been destroyed before is safe, and no-op. (so no need to
// do excess "if (wgpuObject) wgpu_object_destroy(wgpuObject);")
void wgpu_object_destroy(WGpuObjectBase wgpuObject);

// Deinitializes all initialized WebGPU objects.
void wgpu_destroy_all_objects(void);

#ifdef __EMSCRIPTEN__
// Initializes a WebGPU rendering context to a canvas by calling canvas.getContext('webgpu').
WGpuCanvasContext wgpu_canvas_get_webgpu_context(const char *canvasSelector NOTNULL);

// Initializes a WebGPU rendering context to the given Offscreen Canvas.
WGpuCanvasContext wgpu_offscreen_canvas_get_webgpu_context(OffscreenCanvasId id);
#else
WGpuCanvasContext wgpu_canvas_get_webgpu_context(void *hwnd);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The ordering and structure of this remainder of this file follows the official WebGPU WebIDL definitions at https://www.w3.org/TR/webgpu/#idl-index
// This is so that when the official IDL is modified, the modifications can be easily diffed here for updates.

/*
interface mixin GPUObjectBase {
    attribute USVString label;
};
*/
// Returns true if the given handle references a valid WebGPU object
WGPU_BOOL wgpu_is_valid_object(WGpuObjectBase obj);
// Set a human-readable label for the given WebGPU object. Pass an empty string "" to clear a label.
void wgpu_object_set_label(WGpuObjectBase obj, const char *label NOTNULL);
// Gets the human-readable label of a WebGPU object. If dstLabelSize is too short to
// contain the label string, then the label is truncated.
// dstLabelSize: length of dstLabel array in bytes.
// Returns the number of bytes written (excluding null byte at end).
int wgpu_object_get_label(WGpuObjectBase obj, char *dstLabel NOTNULL, uint32_t dstLabelSize);

/*
dictionary GPUObjectDescriptorBase {
    USVString label;
};
*/

/*
dictionary GPUExtent3DDict {
    required GPUIntegerCoordinate width;
    GPUIntegerCoordinate height = 1;
    GPUIntegerCoordinate depthOrArrayLayers = 1;
};
typedef (sequence<GPUIntegerCoordinate> or GPUExtent3DDict) GPUExtent3D;
*/
typedef struct WGpuExtent3D
{
  int width;
  int height; // = 1;
  int depthOrArrayLayers; // = 1;
} WGpuExtent3D;
extern const WGpuExtent3D WGPU_EXTENT_3D_DEFAULT_INITIALIZER;

/*
[Exposed=(Window, DedicatedWorker)]
interface GPUSupportedLimits {
    readonly attribute unsigned long maxTextureDimension1D;
    readonly attribute unsigned long maxTextureDimension2D;
    readonly attribute unsigned long maxTextureDimension3D;
    readonly attribute unsigned long maxTextureArrayLayers;
    readonly attribute unsigned long maxBindGroups;
    readonly attribute unsigned long maxBindGroupsPlusVertexBuffers;
    readonly attribute unsigned long maxBindingsPerBindGroup;
    readonly attribute unsigned long maxDynamicUniformBuffersPerPipelineLayout;
    readonly attribute unsigned long maxDynamicStorageBuffersPerPipelineLayout;
    readonly attribute unsigned long maxSampledTexturesPerShaderStage;
    readonly attribute unsigned long maxSamplersPerShaderStage;
    readonly attribute unsigned long maxStorageBuffersPerShaderStage;
    readonly attribute unsigned long maxStorageBuffersInVertexStage;
    readonly attribute unsigned long maxStorageBuffersInFragmentStage;
    readonly attribute unsigned long maxStorageTexturesPerShaderStage;
    readonly attribute unsigned long maxStorageTexturesInVertexStage;
    readonly attribute unsigned long maxStorageTexturesInFragmentStage;
    readonly attribute unsigned long maxUniformBuffersPerShaderStage;
    readonly attribute unsigned long long maxUniformBufferBindingSize;
    readonly attribute unsigned long long maxStorageBufferBindingSize;
    readonly attribute unsigned long minUniformBufferOffsetAlignment;
    readonly attribute unsigned long minStorageBufferOffsetAlignment;
    readonly attribute unsigned long maxVertexBuffers;
    readonly attribute unsigned long long maxBufferSize;
    readonly attribute unsigned long maxVertexAttributes;
    readonly attribute unsigned long maxVertexBufferArrayStride;
    readonly attribute unsigned long maxInterStageShaderVariables;
    readonly attribute unsigned long maxColorAttachments;
    readonly attribute unsigned long maxColorAttachmentBytesPerSample;
    readonly attribute unsigned long maxComputeWorkgroupStorageSize;
    readonly attribute unsigned long maxComputeInvocationsPerWorkgroup;
    readonly attribute unsigned long maxComputeWorkgroupSizeX;
    readonly attribute unsigned long maxComputeWorkgroupSizeY;
    readonly attribute unsigned long maxComputeWorkgroupSizeZ;
    readonly attribute unsigned long maxComputeWorkgroupsPerDimension;
};
*/
typedef struct WGpuSupportedLimits
{
  // See the table in https://www.w3.org/TR/webgpu/#limits for the minimum/maximum
  // default values for these limits.

  // When requesting an adapter with given limits, pass a value of zero to
  // omit requesting limits for that particular field.

  // 64-bit fields must be present first before the 32-bit fields in this struct.
  uint64_t maxUniformBufferBindingSize; // required >= 65536
  uint64_t maxStorageBufferBindingSize; // required >= 128*1024*1024 (128MB)
  uint64_t maxBufferSize;               // required >= 256*1024*1024 (256MB)

  uint32_t maxTextureDimension1D; // required >= 8192
  uint32_t maxTextureDimension2D; // required >= 8192
  uint32_t maxTextureDimension3D; // required >= 2048
  uint32_t maxTextureArrayLayers; // required >= 256
  uint32_t maxBindGroups; // required >= 4
  uint32_t maxBindGroupsPlusVertexBuffers; // required >= 24
  uint32_t maxBindingsPerBindGroup; // required >= 1000
  uint32_t maxDynamicUniformBuffersPerPipelineLayout; // required >= 8
  uint32_t maxDynamicStorageBuffersPerPipelineLayout; // required >= 4
  uint32_t maxSampledTexturesPerShaderStage; // required >= 16
  uint32_t maxSamplersPerShaderStage; // required >= 16
  uint32_t maxStorageBuffersPerShaderStage; // required >= 8
  uint32_t maxStorageBuffersInVertexStage; // required >= 8
  uint32_t maxStorageBuffersInFragmentStage; // required >= 8
  uint32_t maxStorageTexturesPerShaderStage; // required >= 4
  uint32_t maxStorageTexturesInVertexStage; // required >= 8
  uint32_t maxStorageTexturesInFragmentStage; // required >= 8
  uint32_t maxUniformBuffersPerShaderStage; // required >= 12
  uint32_t minUniformBufferOffsetAlignment; // required >= 256 bytes
  uint32_t minStorageBufferOffsetAlignment; // required >= 256 bytes
  uint32_t maxVertexBuffers; // required >= 8
  uint32_t maxVertexAttributes; // required >= 16
  uint32_t maxVertexBufferArrayStride; // required >= 2048
  uint32_t maxInterStageShaderVariables; // required >= 16
  uint32_t maxColorAttachments; // required >= 8
  uint32_t maxColorAttachmentBytesPerSample; // required >= 32
  uint32_t maxComputeWorkgroupStorageSize; // required >= 16384 bytes
  uint32_t maxComputeInvocationsPerWorkgroup; // required >= 256
  uint32_t maxComputeWorkgroupSizeX; // required >= 256
  uint32_t maxComputeWorkgroupSizeY; // required >= 256
  uint32_t maxComputeWorkgroupSizeZ; // required >= 64
  uint32_t maxComputeWorkgroupsPerDimension; // required >= 65535
} WGpuSupportedLimits;

VERIFY_STRUCT_SIZE(WGpuSupportedLimits, 38*sizeof(uint32_t));

// Assertion in lib_webgpu.js function $wgpuReadSupportedLimits() must be kept in sync:
#define _WGPU_NUM_64BIT_LIMIT_FIELDS 3
#define _WGPU_NUM_32BIT_LIMIT_FIELDS 32
VERIFY_STRUCT_SIZE(WGpuSupportedLimits, (_WGPU_NUM_64BIT_LIMIT_FIELDS*2+_WGPU_NUM_32BIT_LIMIT_FIELDS)*sizeof(uint32_t));

/*
[Exposed=(Window, DedicatedWorker)]
interface GPUSupportedFeatures {
    readonly setlike<DOMString>;
};
*/
/*
enum GPUFeatureName {
    "core-features-and-limits",
    "depth-clip-control",
    "depth32float-stencil8",
    "texture-compression-bc",
    "texture-compression-bc-sliced-3d",
    "texture-compression-etc2",
    "texture-compression-astc",
    "texture-compression-astc-sliced-3d",
    "timestamp-query",
    "indirect-first-instance",
    "shader-f16",
    "rg11b10ufloat-renderable",
    "bgra8unorm-storage",
    "float32-filterable",
    "float32-blendable",
    "clip-distances",
    "dual-source-blending",
    "subgroups",
    "texture-formats-tier1",
    "texture-formats-tier2",
    "primitive-index",
    "texture-component-swizzle",
};
*/
typedef int WGPU_FEATURES_BITFIELD;
#define WGPU_FEATURE_CORE_FEATURES_AND_LIMITS              0x1
#define WGPU_FEATURE_DEPTH_CLIP_CONTROL                    0x2
#define WGPU_FEATURE_DEPTH32FLOAT_STENCIL8                 0x4
#define WGPU_FEATURE_TEXTURE_COMPRESSION_BC                0x8
#define WGPU_FEATURE_TEXTURE_COMPRESSION_BC_SLICED_3D     0x10
#define WGPU_FEATURE_TEXTURE_COMPRESSION_ETC2             0x20
#define WGPU_FEATURE_TEXTURE_COMPRESSION_ASTC             0x40
#define WGPU_FEATURE_TEXTURE_COMPRESSION_ASTC_SLICED_3D   0x80
#define WGPU_FEATURE_TIMESTAMP_QUERY                     0x100
#define WGPU_FEATURE_INDIRECT_FIRST_INSTANCE             0x200
#define WGPU_FEATURE_SHADER_F16                          0x400
#define WGPU_FEATURE_RG11B10UFLOAT_RENDERABLE            0x800
#define WGPU_FEATURE_BGRA8UNORM_STORAGE                 0x1000
#define WGPU_FEATURE_FLOAT32_FILTERABLE                 0x2000
#define WGPU_FEATURE_FLOAT32_BLENDABLE                  0x4000
#define WGPU_FEATURE_CLIP_DISTANCES                     0x8000
#define WGPU_FEATURE_DUAL_SOURCE_BLENDING              0x10000
#define WGPU_FEATURE_SUBGROUPS                         0x20000
#define WGPU_FEATURE_TEXTURE_FORMATS_TIER1             0x40000
#define WGPU_FEATURE_TEXTURE_FORMATS_TIER2             0x80000
#define WGPU_FEATURE_PRIMITIVE_INDEX                  0x100000
#define WGPU_FEATURE_TEXTURE_COMPONENT_SWIZZLE        0x200000

#define WGPU_FEATURE_FIRST_UNUSED_BIT                 0x400000 // Allows examining the number of actually used bits in a WGPU_FEATURES_BITFIELD value.

/*
// WebGPU reuses the color space enum from the HTML Canvas specification:
   https://html.spec.whatwg.org/multipage/canvas.html#predefinedcolorspace
   Because of that reason, it is prefixed here with HTML_ as opposed to WGPU_.
enum PredefinedColorSpace {
    "srgb",
    "srgb-linear",
    "display-p3",
    "display-p3-linear"
};
*/
typedef int HTML_PREDEFINED_COLOR_SPACE;
#define HTML_PREDEFINED_COLOR_SPACE_INVALID 0
#define HTML_PREDEFINED_COLOR_SPACE_SRGB 1
#define HTML_PREDEFINED_COLOR_SPACE_SRGB_LINEAR 2
#define HTML_PREDEFINED_COLOR_SPACE_DISPLAY_P3 3
#define HTML_PREDEFINED_COLOR_SPACE_DISPLAY_P3_LINEAR 4

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUAdapterInfo {
    readonly attribute DOMString vendor;
    readonly attribute DOMString architecture;
    readonly attribute DOMString device;
    readonly attribute DOMString description;
    readonly attribute unsigned long subgroupMinSize;
    readonly attribute unsigned long subgroupMaxSize;
    readonly attribute boolean isFallbackAdapter;
};
*/
typedef struct WGpuAdapterInfo
{
  char vendor[512];
  char architecture[512];
  char device[512];
  char description[512];
  uint32_t subgroupMinSize;
  uint32_t subgroupMaxSize;
  WGPU_BOOL isFallbackAdapter;
} WGpuAdapterInfo;

/*
interface mixin NavigatorGPU {
    [SameObject, SecureContext] readonly attribute GPU gpu;
};
Navigator includes NavigatorGPU;
WorkerNavigator includes NavigatorGPU;

[Exposed=(Window, DedicatedWorker), SecureContext]
interface WGSLLanguageFeatures {
    readonly setlike<DOMString>;
};

[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPU {
    Promise<GPUAdapter?> requestAdapter(optional GPURequestAdapterOptions options = {});
    GPUTextureFormat getPreferredCanvasFormat();
    [SameObject] readonly attribute WGSLLanguageFeatures wgslLanguageFeatures;
};
*/
// Returns true if the browser is advertising to be WebGPU-aware. This means that the browser in question is shipping with WebGPU available, but does not
// necessarily mean that there would exist any WebGPU adapters or devices that are supported. The only way to know if WebGPU is actually possible will
// be to try to request and adapter and then a device.
WGPU_BOOL navigator_gpu_available(void);

// This function can be used to remove access to WebGPU API on the current JS page. This can be useful for debugging or sandboxing purposes. Note that
// if the page has already initialized a WebGPU context, then the context is not affected. Amounts to a 'delete navigator.gpu' operation.
// After calling this function navigator_gpu_available() will return false.
void navigator_delete_webgpu_api_access(void);

// Returns the number of currently asyncified synchronous operations that are pending.
// Call this function in requestAnimationFrame() handlers to detect whether a previous asyncified operation is pending, to detect
// if e.g. a previous call to wgpu_buffer_map_sync() has not yet resolved, and skip rendering until the previous call resolves.
// See buffer_map_sync.c for an example.
int wgpu_sync_operations_pending(void);

// Performs a requestAnimationFrame() animation loop in a manner that is paused/held whenever there are JSPI-asyncified operations in
// flight and execution of Wasm should be suspended. After the JSPI call is resolved, the given requestAnimationFrame() call will continue to run.
// Semantics of the callback function are as in emscripten_request_animation_frame_loop() API.
void wgpu_request_animation_frame_loop(WGPU_BOOL (*callback)(double time, void *userData), void *userData);

typedef void (*WGpuRequestAdapterCallback)(WGpuAdapter adapter, void *userData);
// Requests an adapter from the user agent. The user agent chooses whether to return an adapter, and, if so, chooses according to the provided options.
// If WebGPU is not supported by the browser, returns false.
// Otherwise returns true, and the callback will resolve later with an ID handle to the adapter.
// The callback will also be resolved in the event of an initialization failure, but the ID handle
// passed to the callback will then be zero.
// options: may be null to request an adapter without specific options.
// Note: If the current browser is not aware of the WebGPU API, then this function will by design abort execution
// (fail on assert, and throw a JS exception in release builds). To gracefully detect whether the current browser is new enough to be WebGPU API aware,
// call the function navigator_gpu_available() to check.
WGPU_BOOL navigator_gpu_request_adapter_async(const WGpuRequestAdapterOptions *options NOTNULL, WGpuRequestAdapterCallback adapterCallback, void *userData);
// Requests a WebGPU adapter synchronously. Requires building with -sJSPI=1 linker flag to work.
// options: may be null to request an adapter without specific options.
WGpuAdapter navigator_gpu_request_adapter_sync(const WGpuRequestAdapterOptions *options NOTNULL);

// Like above, but tiny code size without options.
void navigator_gpu_request_adapter_async_simple(WGpuRequestAdapterCallback adapterCallback);
WGpuAdapter navigator_gpu_request_adapter_sync_simple(void);

#ifdef __EMSCRIPTEN__
WGPU_TEXTURE_FORMAT navigator_gpu_get_preferred_canvas_format(void);
#else
// Dawn requires the adapter and canvas context to get the preferred format.
WGPU_TEXTURE_FORMAT navigator_gpu_get_preferred_canvas_format(WGpuAdapter adapter, WGpuCanvasContext canvasContext);
#endif


// Returns an array of strings representing supported WGSL language features. The array of strings is terminated by a null string.
// If you do not need to enumerate though all supported language features, you can use the simpler navigator_gpu_is_wgsl_language_feature_supported()
// function.
// N.b. this function allocates memory on the caller's stack frame, so the returned pointer is not valid only in the function that called
// navigator_gpu_get_wgsl_language_features(). It is recommended to parse the contents of the returned pointer to internal data structures, to
// avoid calling this function multiple times.
const char * const * navigator_gpu_get_wgsl_language_features(void);
// Tests if the given WGSL language feature is supported. (the given feature string exists in navigator.gpu.wgslLanguageFeatures set).
// If this information is needed often (e.g. in an inner loop of a shader cross-compiler), then it is recommended to cache the return value,
// since the supported WGSL language features will not change during page lifetime.
WGPU_BOOL navigator_gpu_is_wgsl_language_feature_supported(const char *feature);

typedef int WGPU_FEATURE_LEVEL;
#define WGPU_FEATURE_LEVEL_CORE 0
#define WGPU_FEATURE_LEVEL_COMPATIBILITY 1

/*
dictionary GPURequestAdapterOptions {
    DOMString featureLevel = "core";
    GPUPowerPreference powerPreference;
    boolean forceFallbackAdapter = false;
    boolean xrCompatible = false;
};
*/
typedef struct WGpuRequestAdapterOptions
{
  WGPU_FEATURE_LEVEL featureLevel;
  // Optionally provides a hint indicating what class of adapter should be selected from the system’s available adapters.
  // The value of this hint may influence which adapter is chosen, but it must not influence whether an adapter is returned or not.
  // Note: The primary utility of this hint is to influence which GPU is used in a multi-GPU system. For instance, some laptops
  //       have a low-power integrated GPU and a high-performance discrete GPU.
  // Note: Depending on the exact hardware configuration, such as battery status and attached displays or removable GPUs, the user
  //       agent may select different adapters given the same power preference. Typically, given the same hardware configuration and
  //       state and powerPreference, the user agent is likely to select the same adapter.
  WGPU_POWER_PREFERENCE powerPreference;
  WGPU_BOOL forceFallbackAdapter;
  WGPU_BOOL xrCompatible;
} WGpuRequestAdapterOptions;
extern const WGpuRequestAdapterOptions WGPU_REQUEST_ADAPTER_OPTIONS_DEFAULT_INITIALIZER;

/*
enum GPUPowerPreference {
    "low-power",
    "high-performance"
};
*/
typedef int WGPU_POWER_PREFERENCE;
#define WGPU_POWER_PREFERENCE_DEFAULT 0
#define WGPU_POWER_PREFERENCE_LOW_POWER 1
#define WGPU_POWER_PREFERENCE_HIGH_PERFORMANCE 2

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUAdapter {
    [SameObject] readonly attribute GPUSupportedFeatures features;
    [SameObject] readonly attribute WGpuSupportedLimits limits;
    [SameObject] readonly attribute GPUAdapterInfo info;
    readonly attribute boolean isFallbackAdapter;

    Promise<GPUDevice> requestDevice(optional GPUDeviceDescriptor descriptor = {});
};
*/
typedef WGpuObjectBase WGpuAdapter;
// Returns true if the given handle references a valid GPUAdapter.
WGPU_BOOL wgpu_is_adapter(WGpuObjectBase object);

// Returns a bitfield of all the supported features on this adapter.
WGPU_FEATURES_BITFIELD wgpu_adapter_or_device_get_features(WGpuAdapter adapterOrDevice);
#define wgpu_adapter_get_features wgpu_adapter_or_device_get_features

// Returns true if the given feature is supported by this adapter.
WGPU_BOOL wgpu_adapter_or_device_supports_feature(WGpuAdapter adapterOrDevice, WGPU_FEATURES_BITFIELD feature);
#define wgpu_adapter_supports_feature wgpu_adapter_or_device_supports_feature

// Populates the adapter.limits field of the given adapter to the provided structure.
void wgpu_adapter_or_device_get_limits(WGpuAdapter adapterOrDevice, WGpuSupportedLimits *limits NOTNULL);
#define wgpu_adapter_get_limits wgpu_adapter_or_device_get_limits

// Returns the WebGPU adapter 'info' field.
void wgpu_adapter_or_device_get_info(WGpuAdapter adapterOrDevice, WGpuAdapterInfo *adapterInfo NOTNULL);
#define wgpu_adapter_get_info wgpu_adapter_or_device_get_info

typedef void (*WGpuRequestDeviceCallback)(WGpuDevice device, void *userData);

void wgpu_adapter_request_device_async(WGpuAdapter adapter, const WGpuDeviceDescriptor *descriptor NOTNULL, WGpuRequestDeviceCallback deviceCallback, void *userData);
// Requests a WebGPU device synchronously. Requires building with -sJSPI=1 linker flag to work.
WGpuDevice wgpu_adapter_request_device_sync(WGpuAdapter adapter, const WGpuDeviceDescriptor *descriptor NOTNULL);

// Like above, but tiny code size without options.
void wgpu_adapter_request_device_async_simple(WGpuAdapter adapter, WGpuRequestDeviceCallback deviceCallback);
WGpuDevice wgpu_adapter_request_device_sync_simple(WGpuAdapter adapter);

/*
dictionary GPUQueueDescriptor : GPUObjectDescriptorBase {
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuQueueDescriptor
{
  const char *label;
  _WGPU_PTR_PADDING(0);
} WGpuQueueDescriptor;

VERIFY_STRUCT_SIZE(WGpuQueueDescriptor, 2*sizeof(uint32_t));

/*
dictionary GPUDeviceDescriptor : GPUObjectDescriptorBase {
    sequence<GPUFeatureName> requiredFeatures = [];
    record<DOMString, (GPUSize64 or undefined)> requiredLimits = {};
    GPUQueueDescriptor defaultQueue = {};
};
*/
typedef struct WGpuDeviceDescriptor
{
  WGpuSupportedLimits requiredLimits;
  WGpuQueueDescriptor defaultQueue;
  WGPU_FEATURES_BITFIELD requiredFeatures;
  uint32_t unused_padding;
} WGpuDeviceDescriptor;
extern const WGpuDeviceDescriptor WGPU_DEVICE_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUDevice : EventTarget {
    [SameObject] readonly attribute GPUSupportedFeatures features;
    [SameObject] readonly attribute GPUSupportedLimits limits;
    [SameObject] readonly attribute GPUAdapterInfo adapterInfo;

    [SameObject] readonly attribute GPUQueue queue;

    undefined destroy();

    GPUBuffer createBuffer(GPUBufferDescriptor descriptor);
    GPUTexture createTexture(GPUTextureDescriptor descriptor);
    GPUSampler createSampler(optional GPUSamplerDescriptor descriptor = {});
    GPUExternalTexture importExternalTexture(GPUExternalTextureDescriptor descriptor);

    GPUBindGroupLayout createBindGroupLayout(GPUBindGroupLayoutDescriptor descriptor);
    GPUPipelineLayout createPipelineLayout(GPUPipelineLayoutDescriptor descriptor);
    GPUBindGroup createBindGroup(GPUBindGroupDescriptor descriptor);

    GPUShaderModule createShaderModule(GPUShaderModuleDescriptor descriptor);
    GPUComputePipeline createComputePipeline(GPUComputePipelineDescriptor descriptor);
    GPURenderPipeline createRenderPipeline(GPURenderPipelineDescriptor descriptor);
    Promise<GPUComputePipeline> createComputePipelineAsync(GPUComputePipelineDescriptor descriptor);
    Promise<GPURenderPipeline> createRenderPipelineAsync(GPURenderPipelineDescriptor descriptor);

    GPUCommandEncoder createCommandEncoder(optional GPUCommandEncoderDescriptor descriptor = {});
    GPURenderBundleEncoder createRenderBundleEncoder(GPURenderBundleEncoderDescriptor descriptor);

    GPUQuerySet createQuerySet(GPUQuerySetDescriptor descriptor);
};
GPUDevice includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuDevice;
// Returns true if the given handle references a valid GPUDevice.
WGPU_BOOL wgpu_is_device(WGpuObjectBase object);

#define wgpu_device_get_features wgpu_adapter_or_device_get_features
#define wgpu_device_supports_feature wgpu_adapter_or_device_supports_feature
#define wgpu_device_get_adapter_info wgpu_adapter_or_device_get_info
#define wgpu_device_get_limits wgpu_adapter_or_device_get_limits

WGpuQueue wgpu_device_get_queue(WGpuDevice device);

#ifndef __EMSCRIPTEN__
void wgpu_device_tick(WGpuDevice device);
#endif

WGpuBuffer wgpu_device_create_buffer(WGpuDevice device, const WGpuBufferDescriptor *bufferDesc NOTNULL);
WGpuTexture wgpu_device_create_texture(WGpuDevice device, const WGpuTextureDescriptor *textureDesc NOTNULL);
WGpuSampler wgpu_device_create_sampler(WGpuDevice device, const WGpuSamplerDescriptor *samplerDesc NOTNULL);
WGpuExternalTexture wgpu_device_import_external_texture(WGpuDevice device, const WGpuExternalTextureDescriptor *externalTextureDesc NOTNULL);

// N.b. not currently using signature WGpuBindGroupLayout wgpu_device_create_bind_group_layout(WGpuDevice device, const WGpuBindGroupLayoutDescriptor *bindGroupLayoutDesc);
// since WGpuBindGroupLayoutDescriptor is a single element struct consisting only of a single array. (if it is expanded in the future, switch to using that signature)
WGpuBindGroupLayout wgpu_device_create_bind_group_layout(WGpuDevice device, const WGpuBindGroupLayoutEntry *bindGroupLayoutEntries, int numEntries);

// N.b. not currently using signature WGpuPipelineLayout wgpu_device_create_pipeline_layout(WGpuDevice device, const WGpuPipelineLayoutDescriptor *pipelineLayoutDesc);
// since WGpuPipelineLayoutDescriptor is a single element struct consisting only of a single array. (if it is expanded in the future, switch to using that signature)
WGpuPipelineLayout wgpu_device_create_pipeline_layout(WGpuDevice device, const WGpuBindGroupLayout *bindGroupLayouts, int numLayouts);

// N.b. not currently using signature WGpuBindGroup wgpu_device_create_bind_group(WGpuDevice device, const WGpuBindGroupDescriptor *bindGroupDesc);
// since WGpuBindGroupDescriptor is a such a light struct. (if it is expanded in the future, switch to using that signature)
WGpuBindGroup wgpu_device_create_bind_group(WGpuDevice device, WGpuBindGroupLayout bindGroupLayout, const WGpuBindGroupEntry *entries, int numEntries);

WGpuShaderModule wgpu_device_create_shader_module(WGpuDevice device, const WGpuShaderModuleDescriptor *shaderModuleDesc NOTNULL);

/*
[Exposed=(Window, DedicatedWorker), SecureContext, Serializable]
interface GPUPipelineError : DOMException {
    constructor(optional DOMString message = "", GPUPipelineErrorInit options);
    readonly attribute GPUPipelineErrorReason reason;
};

dictionary GPUPipelineErrorInit {
    required GPUPipelineErrorReason reason;
};

enum GPUPipelineErrorReason {
    "validation",
    "internal",
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuPipelineError
{
  const char *name; // The name of the DOMException type that represents this error.
  _WGPU_PTR_PADDING(0);
  const char *message; // A possibly human-readable message or description of the error.
  _WGPU_PTR_PADDING(1);
  const char *reason; // One of GPUPipelineErrorReason values.
  _WGPU_PTR_PADDING(2);
} WGpuPipelineError;

VERIFY_STRUCT_SIZE(WGpuPipelineError, 6*sizeof(uint32_t));

// When this callback fires, on success the 'pipeline' parameter is nonzero.
// On failure, pipeline is 0, and in WEBGPU_DEBUG builds 'error' parameter identifies details about the failure. (in release builds the error parameter will be null)
typedef void (*WGpuCreatePipelineCallback)(WGpuDevice device, WGpuPipelineError *error, WGpuPipelineBase pipeline, void *userData);

// N.b. not currently using signature WGpuComputePipeline wgpu_device_create_compute_pipeline(WGpuDevice device, const WGpuComputePipelineDescriptor *computePipelineDesc);
// since WGpuComputePipelineDescriptor is a such a light struct. (if it is expanded in the future, switch to using that signature)
WGpuComputePipeline wgpu_device_create_compute_pipeline(WGpuDevice device, WGpuShaderModule computeModule, const char *entryPoint, WGpuPipelineLayout layout, const WGpuPipelineConstant *constants, int numConstants);
void wgpu_device_create_compute_pipeline_async(WGpuDevice device, WGpuShaderModule computeModule, const char *entryPoint, WGpuPipelineLayout layout, const WGpuPipelineConstant *constants, int numConstants, WGpuCreatePipelineCallback callback, void *userData);

WGpuRenderPipeline wgpu_device_create_render_pipeline(WGpuDevice device, const WGpuRenderPipelineDescriptor *renderPipelineDesc NOTNULL);
void wgpu_device_create_render_pipeline_async(WGpuDevice device, const WGpuRenderPipelineDescriptor *renderPipelineDesc NOTNULL, WGpuCreatePipelineCallback callback, void *userData);

// Creates a WebGPU Command Encoder. There are currently no fields specified in the spec for commandEncoderDesc, so just pass a pointer to a default-initialized object (&WGPU_COMMAND_ENCODER_DESCRIPTOR_DEFAULT_INITIALIZER), or a null pointer.
WGpuCommandEncoder wgpu_device_create_command_encoder(WGpuDevice device, const WGpuCommandEncoderDescriptor *commandEncoderDesc);
// Same as above, but without any descriptor args.
WGpuCommandEncoder wgpu_device_create_command_encoder_simple(WGpuDevice device);

WGpuRenderBundleEncoder wgpu_device_create_render_bundle_encoder(WGpuDevice device, const WGpuRenderBundleEncoderDescriptor *renderBundleEncoderDesc NOTNULL);

WGpuQuerySet wgpu_device_create_query_set(WGpuDevice device, const WGpuQuerySetDescriptor *querySetDesc NOTNULL);

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUBuffer {
    readonly attribute GPUSize64Out size;
    readonly attribute GPUFlagsConstant usage;

    readonly attribute GPUBufferMapState mapState;

    Promise<undefined> mapAsync(GPUMapModeFlags mode, optional GPUSize64 offset = 0, optional GPUSize64 size);
    ArrayBuffer getMappedRange(optional GPUSize64 offset = 0, optional GPUSize64 size);
    undefined unmap();

    undefined destroy();
};
GPUBuffer includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuBuffer;
// Returns true if the given handle references a valid GPUBuffer.
WGPU_BOOL wgpu_is_buffer(WGpuObjectBase object);

// TODO: Add error status to map callback for when mapAsync() promise rejects.
typedef void (*WGpuBufferMapCallback)(WGpuBuffer buffer, void *userData, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset, double_int53_t size);
#define WGPU_MAX_SIZE -1
void wgpu_buffer_map_async(WGpuBuffer buffer, WGpuBufferMapCallback callback, void *userData, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset _WGPU_DEFAULT_VALUE(0), double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_MAX_SIZE));

// Maps the given WGpuBuffer synchronously. Requires building with -sJSPI=1 linker flag to work.
void wgpu_buffer_map_sync(WGpuBuffer buffer, WGPU_MAP_MODE_FLAGS mode, double_int53_t offset _WGPU_DEFAULT_VALUE(0), double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_MAX_SIZE));

#define WGPU_BUFFER_GET_MAPPED_RANGE_FAILED ((double_int53_t)-1)

// Calls buffer.getMappedRange(). Returns `startOffset`, which is used as an ID token to wgpu_buffer_read/write_mapped_range().
// If .getMappedRange() fails, the value WGPU_BUFFER_GET_MAPPED_RANGE_FAILED (-1) will be returned.
double_int53_t wgpu_buffer_get_mapped_range(WGpuBuffer buffer, double_int53_t startOffset, double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_MAX_SIZE));
void wgpu_buffer_read_mapped_range(WGpuBuffer buffer, double_int53_t startOffset, double_int53_t subOffset, void *dst NOTNULL, double_int53_t size);
void wgpu_buffer_write_mapped_range(WGpuBuffer buffer, double_int53_t startOffset, double_int53_t subOffset, const void *src NOTNULL, double_int53_t size);
void wgpu_buffer_unmap(WGpuBuffer buffer);

// Getters for retrieving buffer properties:
double_int53_t wgpu_buffer_size(WGpuBuffer buffer);
WGPU_BUFFER_USAGE_FLAGS wgpu_buffer_usage(WGpuBuffer buffer);
WGPU_BUFFER_MAP_STATE wgpu_buffer_map_state(WGpuBuffer buffer);

/*
enum GPUBufferMapState {
    "unmapped",
    "pending",
    "mapped"
};
*/
typedef int WGPU_BUFFER_MAP_STATE;
#define WGPU_BUFFER_MAP_STATE_INVALID  0
#define WGPU_BUFFER_MAP_STATE_UNMAPPED 1
#define WGPU_BUFFER_MAP_STATE_PENDING  2
#define WGPU_BUFFER_MAP_STATE_MAPPED   3

/*
dictionary GPUBufferDescriptor : GPUObjectDescriptorBase {
    required GPUSize64 size;
    required GPUBufferUsageFlags usage;
    boolean mappedAtCreation = false;
};
*/
typedef struct WGpuBufferDescriptor
{
  uint64_t size;
  WGPU_BUFFER_USAGE_FLAGS usage;
  WGPU_BOOL mappedAtCreation; // Note: it is valid to set mappedAtCreation to true without MAP_READ or MAP_WRITE in usage. This can be used to set the buffer’s initial data.
} WGpuBufferDescriptor;

/*
typedef [EnforceRange] unsigned long GPUBufferUsageFlags;
[Exposed=(Window, DedicatedWorker)]
namespace GPUBufferUsage {
    const GPUFlagsConstant MAP_READ      = 0x0001;
    const GPUFlagsConstant MAP_WRITE     = 0x0002;
    const GPUFlagsConstant COPY_SRC      = 0x0004;
    const GPUFlagsConstant COPY_DST      = 0x0008;
    const GPUFlagsConstant INDEX         = 0x0010;
    const GPUFlagsConstant VERTEX        = 0x0020;
    const GPUFlagsConstant UNIFORM       = 0x0040;
    const GPUFlagsConstant STORAGE       = 0x0080;
    const GPUFlagsConstant INDIRECT      = 0x0100;
    const GPUFlagsConstant QUERY_RESOLVE = 0x0200;
};
*/
typedef int WGPU_BUFFER_USAGE_FLAGS;
#define WGPU_BUFFER_USAGE_MAP_READ      0x0001
#define WGPU_BUFFER_USAGE_MAP_WRITE     0x0002
#define WGPU_BUFFER_USAGE_COPY_SRC      0x0004
#define WGPU_BUFFER_USAGE_COPY_DST      0x0008
#define WGPU_BUFFER_USAGE_INDEX         0x0010
#define WGPU_BUFFER_USAGE_VERTEX        0x0020
#define WGPU_BUFFER_USAGE_UNIFORM       0x0040
#define WGPU_BUFFER_USAGE_STORAGE       0x0080
#define WGPU_BUFFER_USAGE_INDIRECT      0x0100
#define WGPU_BUFFER_USAGE_QUERY_RESOLVE 0x0200

/*
typedef [EnforceRange] unsigned long GPUMapModeFlags;
[Exposed=(Window, DedicatedWorker)]
namespace GPUMapMode {
    const GPUFlagsConstant READ  = 0x0001;
    const GPUFlagsConstant WRITE = 0x0002;
};
*/
typedef int WGPU_MAP_MODE_FLAGS;
#define WGPU_MAP_MODE_READ   0x1
#define WGPU_MAP_MODE_WRITE  0x2

/*
enum GPUTextureViewDimension {
    "1d",
    "2d",
    "2d-array",
    "cube",
    "cube-array",
    "3d"
};
*/
typedef int WGPU_TEXTURE_VIEW_DIMENSION;
#define WGPU_TEXTURE_VIEW_DIMENSION_INVALID 0
#define WGPU_TEXTURE_VIEW_DIMENSION_1D 1
#define WGPU_TEXTURE_VIEW_DIMENSION_2D 2
#define WGPU_TEXTURE_VIEW_DIMENSION_2D_ARRAY 3
#define WGPU_TEXTURE_VIEW_DIMENSION_CUBE 4
#define WGPU_TEXTURE_VIEW_DIMENSION_CUBE_ARRAY 5
#define WGPU_TEXTURE_VIEW_DIMENSION_3D 6

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUTexture {
    GPUTextureView createView(optional GPUTextureViewDescriptor descriptor = {});

    undefined destroy();

    readonly attribute GPUIntegerCoordinate width;
    readonly attribute GPUIntegerCoordinate height;
    readonly attribute GPUIntegerCoordinate depthOrArrayLayers;
    readonly attribute GPUIntegerCoordinate mipLevelCount;
    readonly attribute GPUSize32 sampleCount;
    readonly attribute GPUTextureDimension dimension;
    readonly attribute GPUTextureFormat format;
    readonly attribute GPUFlagsConstant usage;
    readonly attribute (GPUTextureViewDimension or undefined) textureBindingViewDimension;
};
GPUTexture includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuTexture;
// Returns true if the given handle references a valid GPUTexture.
WGPU_BOOL wgpu_is_texture(WGpuObjectBase object);
// textureViewDesc: Can be null, in which case a default view is created.
WGpuTextureView wgpu_texture_create_view(WGpuTexture texture, const WGpuTextureViewDescriptor *textureViewDesc _WGPU_DEFAULT_VALUE(0));
// Same as above, but does not take any descriptor args.
WGpuTextureView wgpu_texture_create_view_simple(WGpuTexture texture);

// Getters for retrieving texture properties:
uint32_t wgpu_texture_width(WGpuTexture texture);
uint32_t wgpu_texture_height(WGpuTexture texture);
uint32_t wgpu_texture_depth_or_array_layers(WGpuTexture texture);
uint32_t wgpu_texture_mip_level_count(WGpuTexture texture);
uint32_t wgpu_texture_sample_count(WGpuTexture texture);
WGPU_TEXTURE_DIMENSION wgpu_texture_dimension(WGpuTexture texture);
WGPU_TEXTURE_FORMAT wgpu_texture_format(WGpuTexture texture);
WGPU_TEXTURE_USAGE_FLAGS wgpu_texture_usage(WGpuTexture texture);
// Returns the textureBindingViewDimension parameter. In core mode, this will
// always be undefined (WGPU_TEXTURE_VIEW_DIMENSION_INVALID), since this restriction
// does not apply to core contexts.
// In compatibility mode, returns the value of the textureBindingViewDimension
// for the given texture. This function can be called to discover what the default
// value applied by the implementation is, if
// textureBindingViewDimension==WGPU_TEXTURE_VIEW_DIMENSION_INVALID was passed at
// texture creation time.
WGPU_TEXTURE_VIEW_DIMENSION wgpu_texture_binding_view_dimension(WGpuTexture texture);
/*
dictionary GPUTextureDescriptor : GPUObjectDescriptorBase {
    required GPUExtent3D size;
    GPUIntegerCoordinate mipLevelCount = 1;
    GPUSize32 sampleCount = 1;
    GPUTextureDimension dimension = "2d";
    required GPUTextureFormat format;
    required GPUTextureUsageFlags usage;
    sequence<GPUTextureFormat> viewFormats = [];
    GPUTextureViewDimension textureBindingViewDimension;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuTextureDescriptor
{
  WGPU_TEXTURE_FORMAT *viewFormats;
  _WGPU_PTR_PADDING(0);
  int numViewFormats;
  uint32_t width;
  uint32_t height; // default = 1;
  uint32_t depthOrArrayLayers; // default = 1;
  uint32_t mipLevelCount; // default = 1;
  uint32_t sampleCount; // default = 1;
  WGPU_TEXTURE_DIMENSION dimension; // default = WGPU_TEXTURE_DIMENSION_2D
  WGPU_TEXTURE_FORMAT format;
  WGPU_TEXTURE_USAGE_FLAGS usage;
  // On compatibility mode devices, views created from this texture must have this as their dimension.
  // If not specified (0==WGPU_TEXTURE_VIEW_DIMENSION_INVALID is passed), a default is chosen.
  // On core mode devices devices, this is ignored, and there is no such restriction.
  WGPU_TEXTURE_VIEW_DIMENSION textureBindingViewDimension;
} WGpuTextureDescriptor;
extern const WGpuTextureDescriptor WGPU_TEXTURE_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
enum GPUTextureDimension {
    "1d",
    "2d",
    "3d",
};
*/
typedef int WGPU_TEXTURE_DIMENSION;
#define WGPU_TEXTURE_DIMENSION_INVALID 0
#define WGPU_TEXTURE_DIMENSION_1D 1
#define WGPU_TEXTURE_DIMENSION_2D 2
#define WGPU_TEXTURE_DIMENSION_3D 3

/*
typedef [EnforceRange] unsigned long GPUTextureUsageFlags;
[Exposed=(Window, DedicatedWorker)]
namespace GPUTextureUsage {
    const GPUFlagsConstant COPY_SRC             = 0x01;
    const GPUFlagsConstant COPY_DST             = 0x02;
    const GPUFlagsConstant TEXTURE_BINDING      = 0x04;
    const GPUFlagsConstant STORAGE_BINDING      = 0x08;
    const GPUFlagsConstant RENDER_ATTACHMENT    = 0x10;
    const GPUFlagsConstant TRANSIENT_ATTACHMENT = 0x20;

};
*/
typedef int WGPU_TEXTURE_USAGE_FLAGS;
#define WGPU_TEXTURE_USAGE_COPY_SRC             0x01
#define WGPU_TEXTURE_USAGE_COPY_DST             0x02
#define WGPU_TEXTURE_USAGE_TEXTURE_BINDING      0x04
#define WGPU_TEXTURE_USAGE_STORAGE_BINDING      0x08
#define WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT    0x10
#define WGPU_TEXTURE_USAGE_TRANSIENT_ATTACHMENT 0x20

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUTextureView {
};
GPUTextureView includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuTextureView;
// Returns true if the given handle references a valid GPUTextureView.
WGPU_BOOL wgpu_is_texture_view(WGpuObjectBase object);


/*
dictionary GPUTextureViewDescriptor : GPUObjectDescriptorBase {
    GPUTextureFormat format;
    GPUTextureViewDimension dimension;
    GPUTextureUsageFlags usage = 0;
    GPUTextureAspect aspect = "all";
    GPUIntegerCoordinate baseMipLevel = 0;
    GPUIntegerCoordinate mipLevelCount;
    GPUIntegerCoordinate baseArrayLayer = 0;
    GPUIntegerCoordinate arrayLayerCount;
};
*/
typedef struct WGpuTextureViewDescriptor
{
  WGPU_TEXTURE_FORMAT format;
  WGPU_TEXTURE_VIEW_DIMENSION dimension;
  // The allowed usages for the texture view. Must be a subset of the
  // usage flags of the texture. If 0, defaults to the full set of
  // usage flags of the texture.
  WGPU_TEXTURE_USAGE_FLAGS usage; // default = 0
  WGPU_TEXTURE_ASPECT aspect; // default = WGPU_TEXTURE_ASPECT_ALL
  uint32_t baseMipLevel; // default = 0
  uint32_t mipLevelCount;
  uint32_t baseArrayLayer; // default = 0
  uint32_t arrayLayerCount;

  // A null-terminated string, representing the swizzle access order of this texture. Default "rgba". Valid characters 'r','g','b','a','0','1', e.g. "a001" and "rrrr" are valid.
  unsigned char swizzle[8];
} WGpuTextureViewDescriptor;
extern const WGpuTextureViewDescriptor WGPU_TEXTURE_VIEW_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
enum GPUTextureAspect {
    "all",
    "stencil-only",
    "depth-only"
};
*/
typedef int WGPU_TEXTURE_ASPECT;
#define WGPU_TEXTURE_ASPECT_INVALID 0
#define WGPU_TEXTURE_ASPECT_ALL 1
#define WGPU_TEXTURE_ASPECT_STENCIL_ONLY 2
#define WGPU_TEXTURE_ASPECT_DEPTH_ONLY 3

/*
enum GPUTextureFormat {
    // 8-bit formats
    "r8unorm", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "r8snorm",
    "r8uint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "r8sint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled

    // 16-bit formats
    "r16unorm", // Supported with "texture-formats-tier1"
    "r16snorm", // Supported with "texture-formats-tier1"
    "r16uint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "r16sint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "r16float", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rg8unorm",
    "rg8snorm",
    "rg8uint",
    "rg8sint",

    // 32-bit formats
    "r32uint",
    "r32sint",
    "r32float",
    "rg16unorm", // Supported with "texture-formats-tier1"
    "rg16snorm", // Supported with "texture-formats-tier1"
    "rg16uint",
    "rg16sint",
    "rg16float",
    "rgba8unorm", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rgba8unorm-srgb",
    "rgba8snorm",
    "rgba8uint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rgba8sint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "bgra8unorm",
    "bgra8unorm-srgb",

    // Packed 32-bit formats
    "rgb9e5ufloat",
    "rgb10a2uint",
    "rgb10a2unorm",
    "rg11b10ufloat",

    // 64-bit formats
    "rg32uint",
    "rg32sint",
    "rg32float",
    "rgba16unorm", // Supported with "texture-formats-tier1"
    "rgba16snorm", // Supported with "texture-formats-tier1"
    "rgba16uint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rgba16sint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rgba16float", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled

    // 128-bit formats
    "rgba32uint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rgba32sint", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    "rgba32float", // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled

    // Depth/stencil formats
    "stencil8",
    "depth16unorm",
    "depth24plus",
    "depth24plus-stencil8",
    "depth32float",

    // "depth32float-stencil8" feature
    "depth32float-stencil8",

    // BC compressed formats usable if "texture-compression-bc" is both
    // supported by the device/user agent and enabled in requestDevice.
    "bc1-rgba-unorm",
    "bc1-rgba-unorm-srgb",
    "bc2-rgba-unorm",
    "bc2-rgba-unorm-srgb",
    "bc3-rgba-unorm",
    "bc3-rgba-unorm-srgb",
    "bc4-r-unorm",
    "bc4-r-snorm",
    "bc5-rg-unorm",
    "bc5-rg-snorm",
    "bc6h-rgb-ufloat",
    "bc6h-rgb-float",
    "bc7-rgba-unorm",
    "bc7-rgba-unorm-srgb",

    // ETC2 compressed formats usable if "texture-compression-etc2" is both
    // supported by the device/user agent and enabled in requestDevice.
    "etc2-rgb8unorm",
    "etc2-rgb8unorm-srgb",
    "etc2-rgb8a1unorm",
    "etc2-rgb8a1unorm-srgb",
    "etc2-rgba8unorm",
    "etc2-rgba8unorm-srgb",
    "eac-r11unorm",
    "eac-r11snorm",
    "eac-rg11unorm",
    "eac-rg11snorm",

    // ASTC compressed formats usable if "texture-compression-astc" is both
    // supported by the device/user agent and enabled in requestDevice.
    "astc-4x4-unorm",
    "astc-4x4-unorm-srgb",
    "astc-5x4-unorm",
    "astc-5x4-unorm-srgb",
    "astc-5x5-unorm",
    "astc-5x5-unorm-srgb",
    "astc-6x5-unorm",
    "astc-6x5-unorm-srgb",
    "astc-6x6-unorm",
    "astc-6x6-unorm-srgb",
    "astc-8x5-unorm",
    "astc-8x5-unorm-srgb",
    "astc-8x6-unorm",
    "astc-8x6-unorm-srgb",
    "astc-8x8-unorm",
    "astc-8x8-unorm-srgb",
    "astc-10x5-unorm",
    "astc-10x5-unorm-srgb",
    "astc-10x6-unorm",
    "astc-10x6-unorm-srgb",
    "astc-10x8-unorm",
    "astc-10x8-unorm-srgb",
    "astc-10x10-unorm",
    "astc-10x10-unorm-srgb",
    "astc-12x10-unorm",
    "astc-12x10-unorm-srgb",
    "astc-12x12-unorm",
    "astc-12x12-unorm-srgb"
};
*/
typedef int WGPU_TEXTURE_FORMAT;
#define WGPU_TEXTURE_FORMAT_INVALID               0
    // 8-bit formats
#define WGPU_TEXTURE_FORMAT_R8UNORM               1 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_R8SNORM               2
#define WGPU_TEXTURE_FORMAT_R8UINT                3 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_R8SINT                4 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    // 16-bit formats
#define WGPU_TEXTURE_FORMAT_R16UNORM              5 // Supported with "texture-formats-tier1"
#define WGPU_TEXTURE_FORMAT_R16SNORM              6 // Supported with "texture-formats-tier1"
#define WGPU_TEXTURE_FORMAT_R16UINT               7 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_R16SINT               8 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_R16FLOAT              9 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RG8UNORM              10
#define WGPU_TEXTURE_FORMAT_RG8SNORM              11
#define WGPU_TEXTURE_FORMAT_RG8UINT               12
#define WGPU_TEXTURE_FORMAT_RG8SINT               13
    // 32-bit formats
#define WGPU_TEXTURE_FORMAT_R32UINT               14
#define WGPU_TEXTURE_FORMAT_R32SINT               15
#define WGPU_TEXTURE_FORMAT_R32FLOAT              16
#define WGPU_TEXTURE_FORMAT_RG16UNORM             17 // Supported with "texture-formats-tier1"
#define WGPU_TEXTURE_FORMAT_RG16SNORM             18 // Supported with "texture-formats-tier1"
#define WGPU_TEXTURE_FORMAT_RG16UINT              19
#define WGPU_TEXTURE_FORMAT_RG16SINT              20
#define WGPU_TEXTURE_FORMAT_RG16FLOAT             21
#define WGPU_TEXTURE_FORMAT_RGBA8UNORM            22 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RGBA8UNORM_SRGB       23
#define WGPU_TEXTURE_FORMAT_RGBA8SNORM            24
#define WGPU_TEXTURE_FORMAT_RGBA8UINT             25 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RGBA8SINT             26 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_BGRA8UNORM            27
#define WGPU_TEXTURE_FORMAT_BGRA8UNORM_SRGB       28
    // Packed 32-bit formats
#define WGPU_TEXTURE_FORMAT_RGB9E5UFLOAT          29
#define WGPU_TEXTURE_FORMAT_RGB10A2UINT           30
#define WGPU_TEXTURE_FORMAT_RGB10A2UNORM          31
#define WGPU_TEXTURE_FORMAT_RG11B10UFLOAT         32
    // 64-bit formats
#define WGPU_TEXTURE_FORMAT_RG32UINT              33
#define WGPU_TEXTURE_FORMAT_RG32SINT              34
#define WGPU_TEXTURE_FORMAT_RG32FLOAT             35
#define WGPU_TEXTURE_FORMAT_RGBA16UNORM           36 // Supported with "texture-formats-tier1"
#define WGPU_TEXTURE_FORMAT_RGBA16SNORM           37 // Supported with "texture-formats-tier1"
#define WGPU_TEXTURE_FORMAT_RGBA16UINT            38 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RGBA16SINT            39 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RGBA16FLOAT           40 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    // 128-bit formats
#define WGPU_TEXTURE_FORMAT_RGBA32UINT            41 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RGBA32SINT            42 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
#define WGPU_TEXTURE_FORMAT_RGBA32FLOAT           43 // Read-write GPUStorageTextureAccess is available if "texture-formats-tier2" is enabled
    // Depth/stencil formats
#define WGPU_TEXTURE_FORMAT_STENCIL8              44
#define WGPU_TEXTURE_FORMAT_DEPTH16UNORM          45
#define WGPU_TEXTURE_FORMAT_DEPTH24PLUS           46
#define WGPU_TEXTURE_FORMAT_DEPTH24PLUS_STENCIL8  47
#define WGPU_TEXTURE_FORMAT_DEPTH32FLOAT          48
#define WGPU_TEXTURE_FORMAT_DEPTH32FLOAT_STENCIL8 49
    // BC compressed formats usable if "texture-compression-bc" is both
    // supported by the device/user agent and enabled in requestDevice.
#define WGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM        50 // DXT1
#define WGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM_SRGB   51 // DXT1
#define WGPU_TEXTURE_FORMAT_BC2_RGBA_UNORM        52 // DXT3, or DXT2 + premultiplied alpha
#define WGPU_TEXTURE_FORMAT_BC2_RGBA_UNORM_SRGB   53 // DXT3, or DXT2 + premultiplied alpha
#define WGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM        54 // DXT5, or DXT4 + premultiplied alpha
#define WGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM_SRGB   55 // DXT5, or DXT4 + premultiplied alpha
#define WGPU_TEXTURE_FORMAT_BC4_R_UNORM           56
#define WGPU_TEXTURE_FORMAT_BC4_R_SNORM           57
#define WGPU_TEXTURE_FORMAT_BC5_RG_UNORM          58
#define WGPU_TEXTURE_FORMAT_BC5_RG_SNORM          59
#define WGPU_TEXTURE_FORMAT_BC6H_RGB_UFLOAT       60
#define WGPU_TEXTURE_FORMAT_BC6H_RGB_FLOAT        61
#define WGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM        62
#define WGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM_SRGB   63
    // ETC2 compressed formats usable if "texture-compression-etc2" is both
    // supported by the device/user agent and enabled in requestDevice.
#define WGPU_TEXTURE_FORMAT_ETC2_RGB8UNORM        64
#define WGPU_TEXTURE_FORMAT_ETC2_RGB8UNORM_SRGB   65
#define WGPU_TEXTURE_FORMAT_ETC2_RGB8A1UNORM      66
#define WGPU_TEXTURE_FORMAT_ETC2_RGB8A1UNORM_SRGB 67
#define WGPU_TEXTURE_FORMAT_ETC2_RGBA8UNORM       68
#define WGPU_TEXTURE_FORMAT_ETC2_RGBA8UNORM_SRGB  69
#define WGPU_TEXTURE_FORMAT_EAC_R11UNORM          70
#define WGPU_TEXTURE_FORMAT_EAC_R11SNORM          71
#define WGPU_TEXTURE_FORMAT_EAC_RG11UNORM         72
#define WGPU_TEXTURE_FORMAT_EAC_RG11SNORM         73
    // ASTC compressed formats usable if "texture-compression-astc" is both
    // supported by the device/user agent and enabled in requestDevice.
#define WGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM        74
#define WGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM_SRGB   75
#define WGPU_TEXTURE_FORMAT_ASTC_5X4_UNORM        76
#define WGPU_TEXTURE_FORMAT_ASTC_5X4_UNORM_SRGB   77
#define WGPU_TEXTURE_FORMAT_ASTC_5X5_UNORM        78
#define WGPU_TEXTURE_FORMAT_ASTC_5X5_UNORM_SRGB   79
#define WGPU_TEXTURE_FORMAT_ASTC_6X5_UNORM        80
#define WGPU_TEXTURE_FORMAT_ASTC_6X5_UNORM_SRGB   81
#define WGPU_TEXTURE_FORMAT_ASTC_6X6_UNORM        82
#define WGPU_TEXTURE_FORMAT_ASTC_6X6_UNORM_SRGB   83
#define WGPU_TEXTURE_FORMAT_ASTC_8X5_UNORM        84
#define WGPU_TEXTURE_FORMAT_ASTC_8X5_UNORM_SRGB   85
#define WGPU_TEXTURE_FORMAT_ASTC_8X6_UNORM        86
#define WGPU_TEXTURE_FORMAT_ASTC_8X6_UNORM_SRGB   87
#define WGPU_TEXTURE_FORMAT_ASTC_8X8_UNORM        88
#define WGPU_TEXTURE_FORMAT_ASTC_8X8_UNORM_SRGB   89
#define WGPU_TEXTURE_FORMAT_ASTC_10X5_UNORM       90
#define WGPU_TEXTURE_FORMAT_ASTC_10X5_UNORM_SRGB  91
#define WGPU_TEXTURE_FORMAT_ASTC_10X6_UNORM       92
#define WGPU_TEXTURE_FORMAT_ASTC_10X6_UNORM_SRGB  93
#define WGPU_TEXTURE_FORMAT_ASTC_10X8_UNORM       94
#define WGPU_TEXTURE_FORMAT_ASTC_10X8_UNORM_SRGB  95
#define WGPU_TEXTURE_FORMAT_ASTC_10X10_UNORM      96
#define WGPU_TEXTURE_FORMAT_ASTC_10X10_UNORM_SRGB 97
#define WGPU_TEXTURE_FORMAT_ASTC_12X10_UNORM      98
#define WGPU_TEXTURE_FORMAT_ASTC_12X10_UNORM_SRGB 99
#define WGPU_TEXTURE_FORMAT_ASTC_12X12_UNORM      100
#define WGPU_TEXTURE_FORMAT_ASTC_12X12_UNORM_SRGB 101
#define WGPU_TEXTURE_FORMAT_LAST_VALUE            101 // This needs to be equal to the highest texture format number above

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUExternalTexture {
};
GPUExternalTexture includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuExternalTexture;
// Returns true if the given handle references a valid GPUExternalTexture.
WGPU_BOOL wgpu_is_external_texture(WGpuObjectBase object);

/*
dictionary GPUExternalTextureDescriptor : GPUObjectDescriptorBase {
    required HTMLVideoElement source;
    PredefinedColorSpace colorSpace = "srgb";
};
*/
typedef struct WGpuExternalTextureDescriptor
{
  // An object ID pointing to instance of type HTMLVideoElement. To obtain this id, you must call
  // either wgpuStore() or wgpuStoreAndSetParent() on JavaScript side on a HTMLVideoElement object
  // to pin/register the video element to a Wasm referenceable object ID.
  WGpuObjectBase source;
  HTML_PREDEFINED_COLOR_SPACE colorSpace;
} WGpuExternalTextureDescriptor;
extern const WGpuExternalTextureDescriptor WGPU_EXTERNAL_TEXTURE_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUSampler {
};
GPUSampler includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuSampler;
// Returns true if the given handle references a valid GPUSampler.
WGPU_BOOL wgpu_is_sampler(WGpuObjectBase object);

/*
dictionary GPUSamplerDescriptor : GPUObjectDescriptorBase {
    GPUAddressMode addressModeU = "clamp-to-edge";
    GPUAddressMode addressModeV = "clamp-to-edge";
    GPUAddressMode addressModeW = "clamp-to-edge";
    GPUFilterMode magFilter = "nearest";
    GPUFilterMode minFilter = "nearest";
    GPUMipmapFilterMode mipmapFilter = "nearest";
    float lodMinClamp = 0;
    float lodMaxClamp = 32;
    GPUCompareFunction compare;
    [Clamp] unsigned short maxAnisotropy = 1;
};
*/
typedef struct WGpuSamplerDescriptor
{
  WGPU_ADDRESS_MODE addressModeU; // default = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE
  WGPU_ADDRESS_MODE addressModeV; // default = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE
  WGPU_ADDRESS_MODE addressModeW; // default = WGPU_ADDRESS_MODE_CLAMP_TO_EDGE
  WGPU_FILTER_MODE magFilter;     // default = WGPU_FILTER_MODE_NEAREST
  WGPU_FILTER_MODE minFilter;     // default = WGPU_FILTER_MODE_NEAREST
  WGPU_MIPMAP_FILTER_MODE mipmapFilter; // default = WGPU_MIPMAP_FILTER_MODE_NEAREST
  float lodMinClamp;              // default = 0
  float lodMaxClamp;              // default = 32
  WGPU_COMPARE_FUNCTION compare;  // default = WGPU_COMPARE_FUNCTION_INVALID (not used)
  uint32_t maxAnisotropy;         // default = 1. N.b. this is 32-bit wide in the bindings implementation for simplicity, unlike in the IDL which specifies a unsigned short.
                                  //                   Most implementations support maxAnisotropy values in range between 1 and 16, inclusive.
                                  //                   The used value of maxAnisotropy will be clamped to the maximum value that the platform supports.
} WGpuSamplerDescriptor;
extern const WGpuSamplerDescriptor WGPU_SAMPLER_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
enum GPUAddressMode {
    "clamp-to-edge",
    "repeat",
    "mirror-repeat"
};
*/
typedef int WGPU_ADDRESS_MODE;
#define WGPU_ADDRESS_MODE_INVALID 0
#define WGPU_ADDRESS_MODE_CLAMP_TO_EDGE 1
#define WGPU_ADDRESS_MODE_REPEAT 2
#define WGPU_ADDRESS_MODE_MIRROR_REPEAT 3

/*
enum GPUFilterMode {
    "nearest",
    "linear"
};
*/
typedef int WGPU_FILTER_MODE;
#define WGPU_FILTER_MODE_INVALID 0
#define WGPU_FILTER_MODE_NEAREST 1
#define WGPU_FILTER_MODE_LINEAR 2

/*
enum GPUMipmapFilterMode {
    "nearest",
    "linear"
};
*/
typedef int WGPU_MIPMAP_FILTER_MODE;
#define WGPU_MIPMAP_FILTER_MODE_INVALID 0
#define WGPU_MIPMAP_FILTER_MODE_NEAREST 1
#define WGPU_MIPMAP_FILTER_MODE_LINEAR 2

/*
enum GPUCompareFunction {
    "never",
    "less",
    "equal",
    "less-equal",
    "greater",
    "not-equal",
    "greater-equal",
    "always"
};
*/
typedef int WGPU_COMPARE_FUNCTION;
#define WGPU_COMPARE_FUNCTION_INVALID 0
#define WGPU_COMPARE_FUNCTION_NEVER 1
#define WGPU_COMPARE_FUNCTION_LESS 2
#define WGPU_COMPARE_FUNCTION_EQUAL 3
#define WGPU_COMPARE_FUNCTION_LESS_EQUAL 4
#define WGPU_COMPARE_FUNCTION_GREATER 5
#define WGPU_COMPARE_FUNCTION_NOT_EQUAL 6
#define WGPU_COMPARE_FUNCTION_GREATER_EQUAL 7
#define WGPU_COMPARE_FUNCTION_ALWAYS 8

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUBindGroupLayout {
};
GPUBindGroupLayout includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuBindGroupLayout;
// Returns true if the given handle references a valid GPUBindGroupLayout.
WGPU_BOOL wgpu_is_bind_group_layout(WGpuObjectBase object);

/*
dictionary GPUBindGroupLayoutDescriptor : GPUObjectDescriptorBase {
    required sequence<GPUBindGroupLayoutEntry> entries;
};
*/
// Currently not used

/*
typedef [EnforceRange] unsigned long GPUShaderStageFlags;
[Exposed=(Window, DedicatedWorker)]
namespace GPUShaderStage {
    const GPUFlagsConstant VERTEX   = 0x1;
    const GPUFlagsConstant FRAGMENT = 0x2;
    const GPUFlagsConstant COMPUTE  = 0x4;
};
*/
typedef int WGPU_SHADER_STAGE_FLAGS;
#define WGPU_SHADER_STAGE_VERTEX   0x1
#define WGPU_SHADER_STAGE_FRAGMENT 0x2
#define WGPU_SHADER_STAGE_COMPUTE  0x4

/*
dictionary GPUBindGroupLayoutEntry {
    required GPUIndex32 binding;
    required GPUShaderStageFlags visibility;

    GPUBufferBindingLayout buffer;
    GPUSamplerBindingLayout sampler;
    GPUTextureBindingLayout texture;
    GPUStorageTextureBindingLayout storageTexture;
    GPUExternalTextureBindingLayout externalTexture;
};
*/
typedef int WGPU_BIND_GROUP_LAYOUT_TYPE;
#define WGPU_BIND_GROUP_LAYOUT_TYPE_INVALID 0
#define WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER 1
#define WGPU_BIND_GROUP_LAYOUT_TYPE_SAMPLER 2
#define WGPU_BIND_GROUP_LAYOUT_TYPE_TEXTURE 3
#define WGPU_BIND_GROUP_LAYOUT_TYPE_STORAGE_TEXTURE 4
#define WGPU_BIND_GROUP_LAYOUT_TYPE_EXTERNAL_TEXTURE 5

// typedef struct WGpuBindGroupLayoutEntry at the end of this file.

/*
enum GPUBufferBindingType {
    "uniform",
    "storage",
    "read-only-storage",
};
*/
typedef int WGPU_BUFFER_BINDING_TYPE;
#define WGPU_BUFFER_BINDING_TYPE_INVALID 0
#define WGPU_BUFFER_BINDING_TYPE_UNIFORM 1
#define WGPU_BUFFER_BINDING_TYPE_STORAGE 2
#define WGPU_BUFFER_BINDING_TYPE_READ_ONLY_STORAGE 3

/*
dictionary GPUBufferBindingLayout {
    GPUBufferBindingType type = "uniform";
    boolean hasDynamicOffset = false;
    GPUSize64 minBindingSize = 0;
};
*/
typedef struct WGpuBufferBindingLayout
{
  WGPU_BUFFER_BINDING_TYPE type;
  int hasDynamicOffset;
  uint64_t minBindingSize;
} WGpuBufferBindingLayout;
extern const WGpuBufferBindingLayout WGPU_BUFFER_BINDING_LAYOUT_DEFAULT_INITIALIZER;

/*
enum GPUSamplerBindingType {
    "filtering",
    "non-filtering",
    "comparison",
};
*/
typedef int WGPU_SAMPLER_BINDING_TYPE;
#define WGPU_SAMPLER_BINDING_TYPE_INVALID 0
#define WGPU_SAMPLER_BINDING_TYPE_FILTERING 1
#define WGPU_SAMPLER_BINDING_TYPE_NON_FILTERING 2
#define WGPU_SAMPLER_BINDING_TYPE_COMPARISON 3

/*
dictionary GPUSamplerBindingLayout {
    GPUSamplerBindingType type = "filtering";
};
*/
typedef struct WGpuSamplerBindingLayout
{
  WGPU_SAMPLER_BINDING_TYPE type;
} WGpuSamplerBindingLayout;
extern const WGpuSamplerBindingLayout WGPU_SAMPLER_BINDING_LAYOUT_DEFAULT_INITIALIZER;

/*
enum GPUTextureSampleType {
  "float",
  "unfilterable-float",
  "depth",
  "sint",
  "uint",
};
*/
typedef int WGPU_TEXTURE_SAMPLE_TYPE;
#define WGPU_TEXTURE_SAMPLE_TYPE_INVALID 0
#define WGPU_TEXTURE_SAMPLE_TYPE_FLOAT 1
#define WGPU_TEXTURE_SAMPLE_TYPE_UNFILTERABLE_FLOAT 2
#define WGPU_TEXTURE_SAMPLE_TYPE_DEPTH 3
#define WGPU_TEXTURE_SAMPLE_TYPE_SINT 4
#define WGPU_TEXTURE_SAMPLE_TYPE_UINT 5

/*
dictionary GPUTextureBindingLayout {
    GPUTextureSampleType sampleType = "float";
    GPUTextureViewDimension viewDimension = "2d";
    boolean multisampled = false;
};
*/
typedef struct WGpuTextureBindingLayout
{
  WGPU_TEXTURE_SAMPLE_TYPE sampleType;
  WGPU_TEXTURE_VIEW_DIMENSION viewDimension;
  WGPU_BOOL multisampled;
} WGpuTextureBindingLayout;
extern const WGpuTextureBindingLayout WGPU_TEXTURE_BINDING_LAYOUT_DEFAULT_INITIALIZER;

/*
enum GPUStorageTextureAccess {
    "write-only",
    "read-only",
    "read-write",
};
*/
typedef int WGPU_STORAGE_TEXTURE_ACCESS;
#define WGPU_STORAGE_TEXTURE_ACCESS_INVALID 0
#define WGPU_STORAGE_TEXTURE_ACCESS_WRITE_ONLY 1
#define WGPU_STORAGE_TEXTURE_ACCESS_READ_ONLY 2
#define WGPU_STORAGE_TEXTURE_ACCESS_READ_WRITE 3

/*
dictionary GPUStorageTextureBindingLayout {
    GPUStorageTextureAccess access = "write-only";
    required GPUTextureFormat format;
    GPUTextureViewDimension viewDimension = "2d";
};
*/
typedef struct WGpuStorageTextureBindingLayout
{
  WGPU_STORAGE_TEXTURE_ACCESS access;
  WGPU_TEXTURE_FORMAT format;
  WGPU_TEXTURE_VIEW_DIMENSION viewDimension;
} WGpuStorageTextureBindingLayout;
extern const WGpuStorageTextureBindingLayout WGPU_STORAGE_TEXTURE_BINDING_LAYOUT_DEFAULT_INITIALIZER;

/*
dictionary GPUExternalTextureBindingLayout {
};
*/
typedef struct WGpuExternalTextureBindingLayout
{
  uint32_t unused_padding; // Appease mixed C and C++ compilation to agree on non-zero struct size.
} WGpuExternalTextureBindingLayout;

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUBindGroup {
};
GPUBindGroup includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuBindGroup;
// Returns true if the given handle references a valid GPUBindGroup.
WGPU_BOOL wgpu_is_bind_group(WGpuObjectBase object);

/*
dictionary GPUBindGroupDescriptor : GPUObjectDescriptorBase {
    required GPUBindGroupLayout layout;
    required sequence<GPUBindGroupEntry> entries;
};
*/
// Currently unused

/*
typedef (GPUSampler or
         GPUTexture or
         GPUTextureView or
         GPUBuffer or
         GPUBufferBinding or
         GPUExternalTexture) GPUBindingResource;

dictionary GPUBindGroupEntry {
    required GPUIndex32 binding;
    required GPUBindingResource resource;
};
*/
typedef struct WGpuBindGroupEntry
{
  uint32_t binding;
  WGpuObjectBase resource;
  // If 'resource' points to a WGpuBuffer, bufferBindOffset and bufferBindSize specify
  // the offset and length of the buffer to bind. If 'resource' does not point to a WGpuBuffer,
  // offset and size are ignored.
  // If the bind group layout uses dynamic offsets (WGpuBindGroupLayoutEntry::hasDynamicOffset was true
  // at bind group layout creation time), then bufferBindOffset and dynamic offset
  // are added together when calling wgpu_encoder_set_bind_group(). (i.e. the offsets accumulate)
  uint64_t bufferBindOffset;
  uint64_t bufferBindSize; // If set to 0 (default), the whole buffer is bound.
} WGpuBindGroupEntry;
extern const WGpuBindGroupEntry WGPU_BIND_GROUP_ENTRY_DEFAULT_INITIALIZER;

/*
dictionary GPUBufferBinding {
    required GPUBuffer buffer;
    GPUSize64 offset = 0;
    GPUSize64 size;
};
*/
// Not exposed. Integrated as part of WGpuBindGroupEntry.

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUPipelineLayout {
};
GPUPipelineLayout includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuPipelineLayout;
// Returns true if the given handle references a valid GPUPipelineLayout.
WGPU_BOOL wgpu_is_pipeline_layout(WGpuObjectBase object);

/*
dictionary GPUPipelineLayoutDescriptor : GPUObjectDescriptorBase {
    required sequence<GPUBindGroupLayout?> bindGroupLayouts;
};
*/
// Currently unused.

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUShaderModule {
    Promise<GPUCompilationInfo> getCompilationInfo();
};
GPUShaderModule includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuShaderModule;
// Returns true if the given handle references a valid GPUShaderModule.
WGPU_BOOL wgpu_is_shader_module(WGpuObjectBase object);

typedef void (*WGpuGetCompilationInfoCallback)(WGpuShaderModule shaderModule, WGpuCompilationInfo *compilationInfo NOTNULL, void *userData);

// Asynchronously obtains information about WebGPU shader module compilation to the given callback.
// !! Remember to call wgpu_free_compilation_info() in the callback function after being done with the data.
void wgpu_shader_module_get_compilation_info_async(WGpuShaderModule shaderModule, WGpuGetCompilationInfoCallback callback, void *userData);

/*
dictionary GPUShaderModuleCompilationHint {
    required USVString entryPoint;
    (GPUPipelineLayout or GPUAutoLayoutMode) layout;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuShaderModuleCompilationHint
{
  const char *entryPoint;
  _WGPU_PTR_PADDING(0);
  WGpuPipelineLayout layout;  // Assign the special value WGPU_AUTO_LAYOUT_MODE_AUTO (default) to hint an automatically created pipeline object.
  uint32_t unused_padding;
} WGpuShaderModuleCompilationHint;
extern const WGpuShaderModuleCompilationHint WGPU_SHADER_MODULE_COMPILATION_HINT_DEFAULT_INITIALIZER;

/*
dictionary GPUShaderModuleDescriptor : GPUObjectDescriptorBase {
    required USVString code;
    sequence<GPUShaderModuleCompilationHint> compilationHints = [];
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuShaderModuleDescriptor
{
  const char *code;
  _WGPU_PTR_PADDING(0);
  const WGpuShaderModuleCompilationHint *hints;
  _WGPU_PTR_PADDING(1);
  int numHints;
  uint32_t unused_padding;
} WGpuShaderModuleDescriptor;

/*
enum GPUCompilationMessageType {
    "error",
    "warning",
    "info"
};
*/
typedef int WGPU_COMPILATION_MESSAGE_TYPE;
#define WGPU_COMPILATION_MESSAGE_TYPE_ERROR 0
#define WGPU_COMPILATION_MESSAGE_TYPE_WARNING 1
#define WGPU_COMPILATION_MESSAGE_TYPE_INFO 2

const char *wgpu_compilation_message_type_to_string(WGPU_COMPILATION_MESSAGE_TYPE type);

/*
[Exposed=(Window, DedicatedWorker), Serializable, SecureContext]
interface GPUCompilationMessage {
    readonly attribute DOMString message;
    readonly attribute GPUCompilationMessageType type;
    readonly attribute unsigned long long lineNum;
    readonly attribute unsigned long long linePos;
    readonly attribute unsigned long long offset;
    readonly attribute unsigned long long length;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuCompilationMessage
{
  // A human-readable string containing the message generated during the shader compilation.
  char *message;
  _WGPU_PTR_PADDING(0);

  // The severity level of the message.
  WGPU_COMPILATION_MESSAGE_TYPE type;

  // The line number in the shader code the message corresponds to. Value is one-based, such
  // that a lineNum of 1 indicates the first line of the shader code.
  // If the message corresponds to a substring this points to the line on which the substring
  // begins. Must be 0 if the message does not correspond to any specific point in the shader code.
  // N.b. lineNum, linePos, offset and length are intentionally uint32_t instead of uint64_t to
  // ease Wasm<->JS marshalling. (uint64_t is overkill)
  uint32_t lineNum;

  // The offset, in UTF-16 code units, from the beginning of line lineNum of the shader code
  // to the point or beginning of the substring that the message corresponds to. Value is
  // one-based, such that a linePos of 1 indicates the first character of the line.
  // If message corresponds to a substring this points to the first UTF-16 code unit of the
  // substring. Must be 0 if the message does not correspond to any specific point in the shader code.
  uint32_t linePos;

  // The offset from the beginning of the shader code in UTF-16 code units to the point or
  // beginning of the substring that message corresponds to. Must reference the same position as
  // lineNum and linePos. Must be 0 if the message does not correspond to any specific point in
  // the shader code.
  uint32_t offset;

  // The number of UTF-16 code units in the substring that message corresponds to. If the message
  // does not correspond with a substring then length must be 0.
  uint32_t length;

  uint32_t unused_padding;
} WGpuCompilationMessage;

VERIFY_STRUCT_SIZE(WGpuCompilationMessage, 8*sizeof(uint32_t));

/*
[Exposed=(Window, DedicatedWorker), Serializable, SecureContext]
interface GPUCompilationInfo {
    readonly attribute FrozenArray<GPUCompilationMessage> messages;
};
*/
typedef struct WGpuCompilationInfo
{
  int numMessages;
  uint32_t unused_padding;
  WGpuCompilationMessage messages[];
} WGpuCompilationInfo;

VERIFY_STRUCT_SIZE(WGpuCompilationInfo, 2*sizeof(uint32_t));

// Deallocates a WGpuCompilationInfo object produced by a call to wgpu_free_compilation_info()
#define wgpu_free_compilation_info(info) free((info))

/*
enum GPUAutoLayoutMode {
    "auto",
};
*/
typedef int WGPU_AUTO_LAYOUT_MODE;
#define WGPU_AUTO_LAYOUT_MODE_NO_HINT 0 // In shader compilation, specifies that no hint is to be passed. In pipeline creation, means same as WGPU_AUTO_LAYOUT_MODE_AUTO.
#define WGPU_AUTO_LAYOUT_MODE_AUTO    1 // In shader compilation, specifies that the hint { layout: 'auto' } is to be passed. In pipeline creation, uses automatic layout creation.

/*
dictionary GPUPipelineDescriptorBase : GPUObjectDescriptorBase {
    required (GPUPipelineLayout or GPUAutoLayoutMode) layout;
};
*/
// Not used since it contains only one member. Will be implemented if # of members increases.

/*
interface mixin GPUPipelineBase {
     [NewObject] GPUBindGroupLayout getBindGroupLayout(unsigned long index);
};
*/
// Returns the bind group layout at the given index of the pipeline. Important: this function allocates a
// new WebGPU object, so in order not to leak WebGPU handles, call wgpu_object_destroy() on the returned value
// when done with it.
WGpuBindGroupLayout wgpu_pipeline_get_bind_group_layout(WGpuObjectBase pipelineBase, uint32_t index);
#define wgpu_render_pipeline_get_bind_group_layout wgpu_pipeline_get_bind_group_layout
#define wgpu_compute_pipeline_get_bind_group_layout wgpu_pipeline_get_bind_group_layout

/*
dictionary GPUProgrammableStage {
    required GPUShaderModule module;
    USVString entryPoint;
    record<USVString, GPUPipelineConstantValue> constants = {};
};
typedef double GPUPipelineConstantValue; // May represent WGSL’s bool, f32, i32, u32, and f16 if enabled.
*/

typedef struct _WGPU_ALIGN_TO_64BITS WGpuPipelineConstant
{
  const char *name;
  _WGPU_PTR_PADDING(0);
  double value;
} WGpuPipelineConstant;

/*

[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUComputePipeline {
};
GPUComputePipeline includes GPUObjectBase;
GPUComputePipeline includes GPUPipelineBase;
*/
typedef WGpuObjectBase WGpuComputePipeline;
// Returns true if the given handle references a valid GPUComputePipeline.
WGPU_BOOL wgpu_is_compute_pipeline(WGpuObjectBase object);

/*
dictionary GPUComputePipelineDescriptor : GPUPipelineDescriptorBase {
    required GPUProgrammableStage compute;
};
*/
// Currently unused.

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPURenderPipeline {
};
GPURenderPipeline includes GPUObjectBase;
GPURenderPipeline includes GPUPipelineBase;
*/
typedef WGpuObjectBase WGpuRenderPipeline;
typedef WGpuObjectBase WGpuPipelineBase;
// Returns true if the given handle references a valid GPURenderPipeline.
WGPU_BOOL wgpu_is_render_pipeline(WGpuObjectBase object);

/*
dictionary GPURenderPipelineDescriptor : GPUPipelineDescriptorBase {
    required GPUVertexState vertex;
    GPUPrimitiveState primitive = {};
    GPUDepthStencilState depthStencil;
    GPUMultisampleState multisample = {};
    GPUFragmentState fragment;
};
*/
// Defined at the end of this file

/*
enum GPUPrimitiveTopology {
    "point-list",
    "line-list",
    "line-strip",
    "triangle-list",
    "triangle-strip"
};
*/
typedef int WGPU_PRIMITIVE_TOPOLOGY;
#define WGPU_PRIMITIVE_TOPOLOGY_INVALID 0
#define WGPU_PRIMITIVE_TOPOLOGY_POINT_LIST 1
#define WGPU_PRIMITIVE_TOPOLOGY_LINE_LIST 2
#define WGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP 3
#define WGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST 4
#define WGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP 5

/*
dictionary GPUPrimitiveState {
    GPUPrimitiveTopology topology = "triangle-list";
    GPUIndexFormat stripIndexFormat;
    GPUFrontFace frontFace = "ccw";
    GPUCullMode cullMode = "none";

    // Requires "depth-clip-control" feature.
    boolean unclippedDepth = false;
};
*/
typedef struct WGpuPrimitiveState
{
  WGPU_PRIMITIVE_TOPOLOGY topology; // Defaults to WGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ('triangle-list')
  WGPU_INDEX_FORMAT stripIndexFormat; // Defaults to undefined, must be explicitly specified if WGPU_PRIMITIVE_TOPOLOGY_LINE_STRIP ('line-strip') or WGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ('triangle-strip') is used.
  WGPU_FRONT_FACE frontFace; // Defaults to WGPU_FRONT_FACE_CCW ('ccw')
  WGPU_CULL_MODE cullMode; // Defaults to WGPU_CULL_MODE_NONE ('none')

  WGPU_BOOL unclippedDepth; // defaults to false.
} WGpuPrimitiveState;

VERIFY_STRUCT_SIZE(WGpuPrimitiveState, 5*sizeof(uint32_t));
/*
enum GPUFrontFace {
    "ccw",
    "cw"
};
*/
typedef int WGPU_FRONT_FACE;
#define WGPU_FRONT_FACE_INVALID 0
#define WGPU_FRONT_FACE_CCW 1
#define WGPU_FRONT_FACE_CW 2

/*
enum GPUCullMode {
    "none",
    "front",
    "back"
};
*/
typedef int WGPU_CULL_MODE;
#define WGPU_CULL_MODE_INVALID 0
#define WGPU_CULL_MODE_NONE 1 // All polygons pass this test.
#define WGPU_CULL_MODE_FRONT 2 // The front-facing polygons are discarded, and do not process in later stages of the render pipeline.
#define WGPU_CULL_MODE_BACK 3 // The back-facing polygons are discarded.

/*
dictionary GPUMultisampleState {
    GPUSize32 count = 1;
    GPUSampleMask mask = 0xFFFFFFFF;
    boolean alphaToCoverageEnabled = false;
};
*/
typedef struct WGpuMultisampleState
{
  uint32_t count;
  uint32_t mask;
  WGPU_BOOL alphaToCoverageEnabled;
} WGpuMultisampleState;

VERIFY_STRUCT_SIZE(WGpuMultisampleState, 3*sizeof(uint32_t));

/*
dictionary GPUFragmentState: GPUProgrammableStage {
    required sequence<GPUColorTargetState?> targets;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuFragmentState
{
  const char *entryPoint;
  _WGPU_PTR_PADDING(0);

  const WGpuColorTargetState *targets;
  _WGPU_PTR_PADDING(1);

  const WGpuPipelineConstant *constants;
  _WGPU_PTR_PADDING(2);

  WGpuShaderModule module;
  int numTargets;
  int numConstants;
  uint32_t unused_padding;
} WGpuFragmentState;

VERIFY_STRUCT_SIZE(WGpuFragmentState, 10*sizeof(uint32_t));
/*
dictionary GPUColorTargetState {
    required GPUTextureFormat format;

    GPUBlendState blend;
    GPUColorWriteFlags writeMask = 0xF;  // GPUColorWrite.ALL
};
*/
// Defined at the end of this file

/*
dictionary GPUBlendState {
    required GPUBlendComponent color;
    required GPUBlendComponent alpha;
};
*/
// Defined at the end of this file

/*
typedef [EnforceRange] unsigned long GPUColorWriteFlags;
[Exposed=(Window, DedicatedWorker)]
namespace GPUColorWrite {
    const GPUFlagsConstant RED   = 0x1;
    const GPUFlagsConstant GREEN = 0x2;
    const GPUFlagsConstant BLUE  = 0x4;
    const GPUFlagsConstant ALPHA = 0x8;
    const GPUFlagsConstant ALL   = 0xF;
};
*/
typedef int WGPU_COLOR_WRITE_FLAGS;
#define WGPU_COLOR_WRITE_RED   0x01
#define WGPU_COLOR_WRITE_GREEN 0x02
#define WGPU_COLOR_WRITE_BLUE  0x04
#define WGPU_COLOR_WRITE_ALPHA 0x08
#define WGPU_COLOR_WRITE_ALL   0x0F

/*
dictionary GPUBlendComponent {
    GPUBlendOperation operation = "add";
    GPUBlendFactor srcFactor = "one";
    GPUBlendFactor dstFactor = "zero";
};
*/
typedef struct WGpuBlendComponent
{
  WGPU_BLEND_OPERATION operation;
  WGPU_BLEND_FACTOR srcFactor;
  WGPU_BLEND_FACTOR dstFactor;
} WGpuBlendComponent;

/*
enum GPUBlendFactor {
    "zero",
    "one",
    "src",
    "one-minus-src",
    "src-alpha",
    "one-minus-src-alpha",
    "dst",
    "one-minus-dst",
    "dst-alpha",
    "one-minus-dst-alpha",
    "src-alpha-saturated",
    "constant",
    "one-minus-constant"
    "src1",
    "one-minus-src1",
    "src1-alpha",
    "one-minus-src1-alpha",
};
*/
typedef int WGPU_BLEND_FACTOR;
#define WGPU_BLEND_FACTOR_INVALID               0
#define WGPU_BLEND_FACTOR_ZERO                  1
#define WGPU_BLEND_FACTOR_ONE                   2
#define WGPU_BLEND_FACTOR_SRC                   3
#define WGPU_BLEND_FACTOR_ONE_MINUS_SRC         4
#define WGPU_BLEND_FACTOR_SRC_ALPHA             5
#define WGPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA   6
#define WGPU_BLEND_FACTOR_DST                   7
#define WGPU_BLEND_FACTOR_ONE_MINUS_DST         8
#define WGPU_BLEND_FACTOR_DST_ALPHA             9
#define WGPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA  10
#define WGPU_BLEND_FACTOR_SRC_ALPHA_SATURATED  11
#define WGPU_BLEND_FACTOR_CONSTANT             12
#define WGPU_BLEND_FACTOR_ONE_MINUS_CONSTANT   13
#define WGPU_BLEND_FACTOR_SRC1                 14
#define WGPU_BLEND_FACTOR_ONE_MINUS_SRC1       15
#define WGPU_BLEND_FACTOR_SRC1_ALPHA           16
#define WGPU_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA 17

/*
enum GPUBlendOperation {
    "add",
    "subtract",
    "reverse-subtract",
    "min",
    "max"
};
*/
typedef int WGPU_BLEND_OPERATION;
#define WGPU_BLEND_OPERATION_INVALID 0
#define WGPU_BLEND_OPERATION_DISABLED 0 // Alias to 'WGPU_BLEND_OPERATION_INVALID'. Used to denote alpha blending being disabled in a more readable way.
#define WGPU_BLEND_OPERATION_ADD 1
#define WGPU_BLEND_OPERATION_SUBTRACT 2
#define WGPU_BLEND_OPERATION_REVERSE_SUBTRACT 3
#define WGPU_BLEND_OPERATION_MIN 4
#define WGPU_BLEND_OPERATION_MAX 5

/*
dictionary GPUDepthStencilState {
    required GPUTextureFormat format;

    boolean depthWriteEnabled;
    GPUCompareFunction depthCompare;

    GPUStencilFaceState stencilFront = {};
    GPUStencilFaceState stencilBack = {};

    GPUStencilValue stencilReadMask = 0xFFFFFFFF;
    GPUStencilValue stencilWriteMask = 0xFFFFFFFF;

    GPUDepthBias depthBias = 0;
    float depthBiasSlopeScale = 0;
    float depthBiasClamp = 0;
};
*/
// Defined at the end of this file

/*
dictionary GPUStencilFaceState {
    GPUCompareFunction compare = "always";
    GPUStencilOperation failOp = "keep";
    GPUStencilOperation depthFailOp = "keep";
    GPUStencilOperation passOp = "keep";
};
*/
typedef struct WGpuStencilFaceState
{
  WGPU_COMPARE_FUNCTION compare;
  WGPU_STENCIL_OPERATION failOp;
  WGPU_STENCIL_OPERATION depthFailOp;
  WGPU_STENCIL_OPERATION passOp;
} WGpuStencilFaceState;

VERIFY_STRUCT_SIZE(WGpuStencilFaceState, 4*sizeof(uint32_t));
/*
enum GPUStencilOperation {
    "keep",
    "zero",
    "replace",
    "invert",
    "increment-clamp",
    "decrement-clamp",
    "increment-wrap",
    "decrement-wrap"
};
*/
typedef int WGPU_STENCIL_OPERATION;
#define WGPU_STENCIL_OPERATION_INVALID 0
#define WGPU_STENCIL_OPERATION_KEEP 1
#define WGPU_STENCIL_OPERATION_ZERO 2
#define WGPU_STENCIL_OPERATION_REPLACE 3
#define WGPU_STENCIL_OPERATION_INVERT 4
#define WGPU_STENCIL_OPERATION_INCREMENT_CLAMP 5
#define WGPU_STENCIL_OPERATION_DECREMENT_CLAMP 6
#define WGPU_STENCIL_OPERATION_INCREMENT_WRAP 7
#define WGPU_STENCIL_OPERATION_DECREMENT_WRAP 8

/*
enum GPUIndexFormat {
    "uint16",
    "uint32"
};
*/
typedef int WGPU_INDEX_FORMAT;
#define WGPU_INDEX_FORMAT_INVALID 0
#define WGPU_INDEX_FORMAT_UINT16 1
#define WGPU_INDEX_FORMAT_UINT32 2

/*
enum GPUVertexFormat {
    "uint8",
    "uint8x2",
    "uint8x4",
    "sint8",
    "sint8x2",
    "sint8x4",
    "unorm8",
    "unorm8x2",
    "unorm8x4",
    "snorm8",
    "snorm8x2",
    "snorm8x4",
    "uint16",
    "uint16x2",
    "uint16x4",
    "sint16",
    "sint16x2",
    "sint16x4",
    "unorm16",
    "unorm16x2",
    "unorm16x4",
    "snorm16",
    "snorm16x2",
    "snorm16x4",
    "float16",
    "float16x2",
    "float16x4",
    "float32",
    "float32x2",
    "float32x3",
    "float32x4",
    "uint32",
    "uint32x2",
    "uint32x3",
    "uint32x4",
    "sint32",
    "sint32x2",
    "sint32x3",
    "sint32x4",
    "unorm10-10-10-2",
    "unorm8x4-bgra",
};
*/
// The numbering on these types continues at the end of WGPU_TEXTURE_FORMAT
// for optimization reasons.
typedef int WGPU_VERTEX_FORMAT;
#define WGPU_VERTEX_FORMAT_INVALID           0
#define WGPU_VERTEX_FORMAT_FIRST_VALUE     102
#define WGPU_VERTEX_FORMAT_UINT8           102
#define WGPU_VERTEX_FORMAT_UINT8X2         103
#define WGPU_VERTEX_FORMAT_UINT8X4         104
#define WGPU_VERTEX_FORMAT_SINT8           105
#define WGPU_VERTEX_FORMAT_SINT8X2         106
#define WGPU_VERTEX_FORMAT_SINT8X4         107
#define WGPU_VERTEX_FORMAT_UNORM8          108
#define WGPU_VERTEX_FORMAT_UNORM8X2        109
#define WGPU_VERTEX_FORMAT_UNORM8X4        110
#define WGPU_VERTEX_FORMAT_SNORM8          111
#define WGPU_VERTEX_FORMAT_SNORM8X2        112
#define WGPU_VERTEX_FORMAT_SNORM8X4        113
#define WGPU_VERTEX_FORMAT_UINT16          114
#define WGPU_VERTEX_FORMAT_UINT16X2        115
#define WGPU_VERTEX_FORMAT_UINT16X4        116
#define WGPU_VERTEX_FORMAT_SINT16          117
#define WGPU_VERTEX_FORMAT_SINT16X2        118
#define WGPU_VERTEX_FORMAT_SINT16X4        119
#define WGPU_VERTEX_FORMAT_UNORM16         120
#define WGPU_VERTEX_FORMAT_UNORM16X2       121
#define WGPU_VERTEX_FORMAT_UNORM16X4       122
#define WGPU_VERTEX_FORMAT_SNORM16         123
#define WGPU_VERTEX_FORMAT_SNORM16X2       124
#define WGPU_VERTEX_FORMAT_SNORM16X4       125
#define WGPU_VERTEX_FORMAT_FLOAT16         126
#define WGPU_VERTEX_FORMAT_FLOAT16X2       127
#define WGPU_VERTEX_FORMAT_FLOAT16X4       128
#define WGPU_VERTEX_FORMAT_FLOAT32         129
#define WGPU_VERTEX_FORMAT_FLOAT32X2       130
#define WGPU_VERTEX_FORMAT_FLOAT32X3       131
#define WGPU_VERTEX_FORMAT_FLOAT32X4       132
#define WGPU_VERTEX_FORMAT_UINT32          133
#define WGPU_VERTEX_FORMAT_UINT32X2        134
#define WGPU_VERTEX_FORMAT_UINT32X3        135
#define WGPU_VERTEX_FORMAT_UINT32X4        136
#define WGPU_VERTEX_FORMAT_SINT32          137
#define WGPU_VERTEX_FORMAT_SINT32X2        138
#define WGPU_VERTEX_FORMAT_SINT32X3        139
#define WGPU_VERTEX_FORMAT_SINT32X4        140
#define WGPU_VERTEX_FORMAT_UNORM10_10_10_2 141
#define WGPU_VERTEX_FORMAT_UNORM8X4_BGRA   142

#if __cplusplus >= 201103L
static_assert(WGPU_VERTEX_FORMAT_FIRST_VALUE == WGPU_TEXTURE_FORMAT_LAST_VALUE + 1, "WGPU_VERTEX_FORMAT enums must have values after WGPU_TEXTURE_FORMAT values!");
#endif

// Calculates the dimension/number of channels in the given format (1-4).
int wgpu_vertex_format_channel_count(WGPU_VERTEX_FORMAT format);

// Calculates the size of a single element in the given format (1-16).
int wgpu_vertex_format_byte_size(WGPU_VERTEX_FORMAT format);

// Returns true if the given vertex format is any one of the _UNORM types.
WGPU_BOOL wgpu_vertex_format_is_unorm(WGPU_VERTEX_FORMAT format);

// Returns a textual representation of the given format, useful for debug printing purposes.
const char *wgpu_vertex_format_to_string(WGPU_VERTEX_FORMAT format);

// Parses the WGSL element data type of a vertex format.
typedef int WGPU_WGSL_ELEMENT_TYPE;
#define WGPU_WGSL_ELEMENT_TYPE_FLOAT 0 // f32, vec2f, vec3f or vec4f
#define WGPU_WGSL_ELEMENT_TYPE_HALF 1 // f16, vec2h, vec3h or vec4h
#define WGPU_WGSL_ELEMENT_TYPE_UINT 2 // u32, vec2u, vec3u or vec4u
#define WGPU_WGSL_ELEMENT_TYPE_SINT 3 // s32, vec2i, vec3i or vec4i
WGPU_WGSL_ELEMENT_TYPE wgpu_vertex_format_wgsl_element_type(WGPU_VERTEX_FORMAT format);

/*
enum GPUVertexStepMode {
    "vertex",
    "instance"
};
*/
typedef int WGPU_VERTEX_STEP_MODE;
#define WGPU_VERTEX_STEP_MODE_INVALID 0
#define WGPU_VERTEX_STEP_MODE_VERTEX 1
#define WGPU_VERTEX_STEP_MODE_INSTANCE 2

/*
dictionary GPUVertexState: GPUProgrammableStage {
    sequence<GPUVertexBufferLayout?> buffers = [];
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuVertexState
{
  const char *entryPoint;
  _WGPU_PTR_PADDING(0);

  const WGpuVertexBufferLayout *buffers;
  _WGPU_PTR_PADDING(1);

  const WGpuPipelineConstant *constants;
  _WGPU_PTR_PADDING(2);

  WGpuShaderModule module;
  int numBuffers;
  int numConstants;
  uint32_t unused_padding;
} WGpuVertexState;

VERIFY_STRUCT_SIZE(WGpuVertexState, 10*sizeof(uint32_t));

/*
dictionary GPUVertexBufferLayout {
    required GPUSize64 arrayStride;
    GPUVertexStepMode stepMode = "vertex";
    required sequence<GPUVertexAttribute> attributes;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuVertexBufferLayout
{
  const WGpuVertexAttribute *attributes;
  _WGPU_PTR_PADDING(0);
  int numAttributes;
  WGPU_VERTEX_STEP_MODE stepMode;
  uint64_t arrayStride;
} WGpuVertexBufferLayout;

VERIFY_STRUCT_SIZE(WGpuVertexBufferLayout, 6*sizeof(uint32_t));

/*
dictionary GPUVertexAttribute {
    required GPUVertexFormat format;
    required GPUSize64 offset;

    required GPUIndex32 shaderLocation;
};
*/
typedef struct WGpuVertexAttribute
{
  uint64_t offset; // The offset, in bytes, from the beginning of the element to the data for the attribute.
  uint32_t shaderLocation; // The numeric location associated with this attribute, which will correspond with a "@location" attribute declared in the vertex.module.
  WGPU_VERTEX_FORMAT format;
} WGpuVertexAttribute;

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUCommandBuffer {
};
GPUCommandBuffer includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuCommandBuffer;
// Returns true if the given handle references a valid GPUCommandBuffer.
WGPU_BOOL wgpu_is_command_buffer(WGpuObjectBase object);

/*
dictionary GPUCommandBufferDescriptor : GPUObjectDescriptorBase {
};
*/
typedef struct WGpuCommandBufferDescriptor
{
  uint32_t unused_padding; // Appease mixed C and C++ compilation to agree on non-zero struct size. Remove this once label is added
} WGpuCommandBufferDescriptor;

/*
interface mixin GPUDebugCommandsMixin {
    undefined pushDebugGroup(USVString groupLabel);
    undefined popDebugGroup();
    undefined insertDebugMarker(USVString markerLabel);
};
*/
typedef WGpuObjectBase WGpuDebugCommandsMixin; // One of GPURenderBundleEncoder, GPURenderPassEncoder, GPUComputePassEncoder or GPUCommandEncoder

void wgpu_encoder_push_debug_group(WGpuDebugCommandsMixin encoder, const char *groupLabel NOTNULL);
void wgpu_encoder_pop_debug_group(WGpuDebugCommandsMixin encoder);
void wgpu_encoder_insert_debug_marker(WGpuDebugCommandsMixin encoder, const char *markerLabel NOTNULL);

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUCommandEncoder {
    GPURenderPassEncoder beginRenderPass(GPURenderPassDescriptor descriptor);
    GPUComputePassEncoder beginComputePass(optional GPUComputePassDescriptor descriptor = {});

    undefined copyBufferToBuffer(
        GPUBuffer source,
        GPUSize64 sourceOffset,
        GPUBuffer destination,
        GPUSize64 destinationOffset,
        GPUSize64 size);

    undefined copyBufferToTexture(
        GPUTexelCopyBufferInfo source,
        GPUTexelCopyTextureInfo destination,
        GPUExtent3D copySize);

    undefined copyTextureToBuffer(
        GPUTexelCopyTextureInfo source,
        GPUTexelCopyBufferInfo destination,
        GPUExtent3D copySize);

    undefined copyTextureToTexture(
        GPUTexelCopyTextureInfo source,
        GPUTexelCopyTextureInfo destination,
        GPUExtent3D copySize);

    undefined clearBuffer(
        GPUBuffer buffer,
        optional GPUSize64 offset = 0,
        optional GPUSize64 size);

    undefined resolveQuerySet(
        GPUQuerySet querySet,
        GPUSize32 firstQuery,
        GPUSize32 queryCount,
        GPUBuffer destination,
        GPUSize64 destinationOffset);

    GPUCommandBuffer finish(optional GPUCommandBufferDescriptor descriptor = {});
};
GPUCommandEncoder includes GPUObjectBase;
GPUCommandEncoder includes GPUCommandsMixin;
GPUCommandEncoder includes GPUDebugCommandsMixin;
*/
typedef WGpuObjectBase WGpuCommandEncoder;
// Returns true if the given handle references a valid GPUCommandEncoder.
WGPU_BOOL wgpu_is_command_encoder(WGpuObjectBase object);

WGpuRenderPassEncoder wgpu_command_encoder_begin_render_pass(WGpuCommandEncoder commandEncoder, const WGpuRenderPassDescriptor *renderPassDesc NOTNULL);
WGpuComputePassEncoder wgpu_command_encoder_begin_compute_pass(WGpuCommandEncoder commandEncoder, const WGpuComputePassDescriptor *computePassDesc _WGPU_DEFAULT_VALUE(0));
// Copies a buffer to another. Passing WGPU_INFINITY as the size (or omitting it altogether as a default value in C++) copies the whole buffer, without needing to pass the size.
void wgpu_command_encoder_copy_buffer_to_buffer(WGpuCommandEncoder commandEncoder, WGpuBuffer source, double_int53_t sourceOffset, WGpuBuffer destination, double_int53_t destinationOffset, double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_INFINITY));
void wgpu_command_encoder_copy_buffer_to_texture(WGpuCommandEncoder commandEncoder, const WGpuTexelCopyBufferInfo *source NOTNULL, const WGpuTexelCopyTextureInfo *destination NOTNULL, uint32_t copyWidth, uint32_t copyHeight _WGPU_DEFAULT_VALUE(1), uint32_t copyDepthOrArrayLayers _WGPU_DEFAULT_VALUE(1));
void wgpu_command_encoder_copy_texture_to_buffer(WGpuCommandEncoder commandEncoder, const WGpuTexelCopyTextureInfo *source NOTNULL, const WGpuTexelCopyBufferInfo *destination NOTNULL, uint32_t copyWidth, uint32_t copyHeight _WGPU_DEFAULT_VALUE(1), uint32_t copyDepthOrArrayLayers _WGPU_DEFAULT_VALUE(1));
void wgpu_command_encoder_copy_texture_to_texture(WGpuCommandEncoder commandEncoder, const WGpuTexelCopyTextureInfo *source NOTNULL, const WGpuTexelCopyTextureInfo *destination NOTNULL, uint32_t copyWidth, uint32_t copyHeight _WGPU_DEFAULT_VALUE(1), uint32_t copyDepthOrArrayLayers _WGPU_DEFAULT_VALUE(1));
void wgpu_command_encoder_clear_buffer(WGpuCommandEncoder commandEncoder, WGpuBuffer buffer, double_int53_t offset _WGPU_DEFAULT_VALUE(0), double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_MAX_SIZE));
// destinationOffset must be a multiple of 256 bytes.
void wgpu_command_encoder_resolve_query_set(WGpuCommandEncoder commandEncoder, WGpuQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGpuBuffer destination, double_int53_t destinationOffset);

// GPUCommandEncoder and GPURenderBundleEncoder share the same finish() command.
WGpuObjectBase wgpu_encoder_finish(WGpuObjectBase commandOrRenderBundleEncoder);
#define wgpu_command_encoder_finish wgpu_encoder_finish

// Inherited from GPUDebugCommandsMixin
#define wgpu_command_encoder_push_debug_group wgpu_encoder_push_debug_group
#define wgpu_command_encoder_pop_debug_group wgpu_encoder_pop_debug_group
#define wgpu_command_encoder_insert_debug_marker wgpu_encoder_insert_debug_marker

/*
dictionary GPUCommandEncoderDescriptor : GPUObjectDescriptorBase {
};
*/
typedef struct WGpuCommandEncoderDescriptor
{
  uint32_t unused_padding; // Appease mixed C and C++ compilation to agree on non-zero struct size.
} WGpuCommandEncoderDescriptor;
extern const WGpuCommandEncoderDescriptor WGPU_COMMAND_ENCODER_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
dictionary GPUTexelCopyBufferLayout {
    GPUSize64 offset = 0;
    GPUSize32 bytesPerRow;
    GPUSize32 rowsPerImage;
};
// unused: fused to WGpuTexelCopyBufferInfo
*/

/*
dictionary GPUTexelCopyBufferInfo : GPUTexelCopyBufferLayout {
    required GPUBuffer buffer;
};
*/
typedef struct WGpuTexelCopyBufferInfo
{
  uint64_t offset;
  uint32_t bytesPerRow; // Must be a multiple of 256.
  uint32_t rowsPerImage;
  WGpuBuffer buffer;
  uint32_t unused_padding;
} WGpuTexelCopyBufferInfo;
extern const WGpuTexelCopyBufferInfo WGPU_TEXEL_COPY_BUFFER_INFO_DEFAULT_INITIALIZER;

/*
dictionary GPUTexelCopyTextureInfo {
    required GPUTexture texture;
    GPUIntegerCoordinate mipLevel = 0;
    GPUOrigin3D origin = {};
    GPUTextureAspect aspect = "all";
};
*/
// Defined at the end of this file

/*
dictionary WGPUCopyExternalImageDestInfo : GPUTexelCopyTextureInfo {
    PredefinedColorSpace colorSpace = "srgb";
    boolean premultipliedAlpha = false;
};
*/
// Defined at the end of this file

/*
dictionary GPUCopyExternalImageSourceInfo {
    required (ImageBitmap or HTMLVideoElement or HTMLCanvasElement or OffscreenCanvas) source;
    GPUOrigin2D origin = {};
    boolean flipY = false;
};
*/
// Defined at the end of this file

/*
interface mixin GPUBindingCommandsMixin {
    undefined setBindGroup(GPUIndex32 index, GPUBindGroup bindGroup,
                      optional sequence<GPUBufferDynamicOffset> dynamicOffsets = []);

    undefined setBindGroup(GPUIndex32 index, GPUBindGroup bindGroup,
        [AllowShared] Uint32Array dynamicOffsetsData,
                      GPUSize64 dynamicOffsetsDataStart,
                      GPUSize32 dynamicOffsetsDataLength);
};
*/
typedef WGpuObjectBase WGpuBindingCommandsMixin;
// Returns true if the given handle references a valid GPUBindingCommandsMixin. (one of: GPUComputePassEncoder, GPURenderPassEncoder, or GPURenderBundleEncoder)
WGPU_BOOL wgpu_is_binding_commands_mixin(WGpuObjectBase object);
// When dynamic offsets are used, they apply in the binding order, i.e. dynamicOffsets[0] applies to the first dynamic @binding, dynamicOffsets[1] the second dynamic @binding (not necessarily at index 1), and so on.
void wgpu_encoder_set_bind_group(WGpuBindingCommandsMixin encoder, uint32_t index, WGpuBindGroup bindGroup, const uint32_t *dynamicOffsets _WGPU_DEFAULT_VALUE(0), uint32_t numDynamicOffsets _WGPU_DEFAULT_VALUE(0));

// Some of the functions in GPURenderBundleEncoder, GPURenderPassEncoder and GPUComputePassEncoder are identical in implementation,
// so group them under a common base class.
void wgpu_encoder_set_pipeline(WGpuBindingCommandsMixin encoder, WGpuObjectBase pipeline);
void wgpu_encoder_end(WGpuBindingCommandsMixin encoder);

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUComputePassEncoder {
    undefined setPipeline(GPUComputePipeline pipeline);
    undefined dispatchWorkgroups(GPUSize32 workgroupCountX, optional GPUSize32 workgroupCountY = 1, optional GPUSize32 workgroupCountZ = 1);
    undefined dispatchWorkgroupsIndirect(GPUBuffer indirectBuffer, GPUSize64 indirectOffset);

    undefined end();
};
GPUComputePassEncoder includes GPUObjectBase;
GPUComputePassEncoder includes GPUCommandsMixin;
GPUComputePassEncoder includes GPUDebugCommandsMixin;
GPUComputePassEncoder includes GPUBindingCommandsMixin;
*/
typedef WGpuObjectBase WGpuComputePassEncoder;
// Returns true if the given handle references a valid GPUComputePassEncoder.
WGPU_BOOL wgpu_is_compute_pass_encoder(WGpuObjectBase object);

#define wgpu_compute_pass_encoder_set_pipeline wgpu_encoder_set_pipeline
void wgpu_compute_pass_encoder_dispatch_workgroups(WGpuComputePassEncoder encoder, uint32_t workgroupCountX, uint32_t workgroupCountY _WGPU_DEFAULT_VALUE(1), uint32_t workgroupCountZ _WGPU_DEFAULT_VALUE(1));
void wgpu_compute_pass_encoder_dispatch_workgroups_indirect(WGpuComputePassEncoder encoder, WGpuBuffer indirectBuffer, double_int53_t indirectOffset);
#define wgpu_compute_pass_encoder_end wgpu_encoder_end

// Inherited from GPUDebugCommandsMixin:
#define wgpu_compute_pass_encoder_push_debug_group wgpu_encoder_push_debug_group
#define wgpu_compute_pass_encoder_pop_debug_group wgpu_encoder_pop_debug_group
#define wgpu_compute_pass_encoder_insert_debug_marker wgpu_encoder_insert_debug_marker

// Inherited from GPUBindingCommandsMixin:
#define wgpu_compute_pass_encoder_set_bind_group wgpu_encoder_set_bind_group

/*
dictionary GPUComputePassTimestampWrites {
    required GPUQuerySet querySet;
    GPUSize32 beginningOfPassWriteIndex;
    GPUSize32 endOfPassWriteIndex;
};
*/
typedef struct WGpuComputePassTimestampWrites
{
  WGpuQuerySet querySet;
  int32_t beginningOfPassWriteIndex; // Important! Specify -1 to not perform a write at beginning of pass. Setting 0 will write to index 0. Ignored if querySet == 0.
  int32_t endOfPassWriteIndex;       // Important! Specify -1 to not perform a write at end of pass. Setting 0 will write to index 0. Ignored if querySet == 0.
} WGpuComputePassTimestampWrites;
extern const WGpuComputePassTimestampWrites WGPU_COMPUTE_PASS_TIMESTAMP_WRITES_DEFAULT_INITIALIZER;

VERIFY_STRUCT_SIZE(WGpuComputePassTimestampWrites, 3*sizeof(uint32_t));

/*
dictionary GPUComputePassDescriptor
         : GPUObjectDescriptorBase {
    GPUComputePassTimestampWrites timestampWrites;
};
*/
typedef struct WGpuComputePassDescriptor
{
  WGpuComputePassTimestampWrites timestampWrites;
} WGpuComputePassDescriptor;
extern const WGpuComputePassDescriptor WGPU_COMPUTE_PASS_DESCRIPTOR_DEFAULT_INITIALIZER;

/*
interface mixin GPURenderCommandsMixin {
    undefined setPipeline(GPURenderPipeline pipeline);

    undefined setIndexBuffer(GPUBuffer buffer, GPUIndexFormat indexFormat, optional GPUSize64 offset = 0, optional GPUSize64 size);
    undefined setVertexBuffer(GPUIndex32 slot, GPUBuffer buffer, optional GPUSize64 offset = 0, optional GPUSize64 size);

    undefined draw(GPUSize32 vertexCount, optional GPUSize32 instanceCount = 1,
        optional GPUSize32 firstVertex = 0, optional GPUSize32 firstInstance = 0);
    undefined drawIndexed(GPUSize32 indexCount, optional GPUSize32 instanceCount = 1,
        optional GPUSize32 firstIndex = 0,
        optional GPUSignedOffset32 baseVertex = 0,
        optional GPUSize32 firstInstance = 0);

    undefined drawIndirect(GPUBuffer indirectBuffer, GPUSize64 indirectOffset);
    undefined drawIndexedIndirect(GPUBuffer indirectBuffer, GPUSize64 indirectOffset);
};
*/
// Deliberate API naming divergence: in upstream WebGPU API, there are base "mixin" classes
typedef WGpuObjectBase WGpuRenderCommandsMixin;
// Returns true if the given handle references a valid GPURenderCommandsMixin.
WGPU_BOOL wgpu_is_render_commands_mixin(WGpuObjectBase object);

#define wgpu_render_commands_mixin_set_pipeline wgpu_encoder_set_pipeline
void wgpu_render_commands_mixin_set_index_buffer(WGpuRenderCommandsMixin renderCommandsMixin, WGpuBuffer buffer, WGPU_INDEX_FORMAT indexFormat, double_int53_t offset _WGPU_DEFAULT_VALUE(0), double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_MAX_SIZE));
void wgpu_render_commands_mixin_set_vertex_buffer(WGpuRenderCommandsMixin renderCommandsMixin, int32_t slot, WGpuBuffer buffer, double_int53_t offset _WGPU_DEFAULT_VALUE(0), double_int53_t size _WGPU_DEFAULT_VALUE(WGPU_MAX_SIZE));

void wgpu_render_commands_mixin_draw(WGpuRenderCommandsMixin renderCommandsMixin, uint32_t vertexCount, uint32_t instanceCount _WGPU_DEFAULT_VALUE(1), uint32_t firstVertex _WGPU_DEFAULT_VALUE(0), uint32_t firstInstance _WGPU_DEFAULT_VALUE(0));
void wgpu_render_commands_mixin_draw_indexed(WGpuRenderCommandsMixin renderCommandsMixin, uint32_t indexCount, uint32_t instanceCount _WGPU_DEFAULT_VALUE(1), uint32_t firstIndex _WGPU_DEFAULT_VALUE(0), int32_t baseVertex _WGPU_DEFAULT_VALUE(0), uint32_t firstInstance _WGPU_DEFAULT_VALUE(0));

void wgpu_render_commands_mixin_draw_indirect(WGpuRenderCommandsMixin renderCommandsMixin, WGpuBuffer indirectBuffer, double_int53_t indirectOffset);
void wgpu_render_commands_mixin_draw_indexed_indirect(WGpuRenderCommandsMixin renderCommandsMixin, WGpuBuffer indirectBuffer, double_int53_t indirectOffset);

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPURenderPassEncoder {
    undefined setViewport(float x, float y,
                     float width, float height,
                     float minDepth, float maxDepth);

    undefined setScissorRect(GPUIntegerCoordinate x, GPUIntegerCoordinate y,
                        GPUIntegerCoordinate width, GPUIntegerCoordinate height);

    undefined setBlendConstant(GPUColor color);
    undefined setStencilReference(GPUStencilValue reference);

    undefined beginOcclusionQuery(GPUSize32 queryIndex);
    undefined endOcclusionQuery();

    undefined executeBundles(sequence<GPURenderBundle> bundles);
    undefined end();
};
GPURenderPassEncoder includes GPUObjectBase;
GPURenderPassEncoder includes GPUCommandsMixin;
GPURenderPassEncoder includes GPUDebugCommandsMixin;
GPURenderPassEncoder includes GPUBindingCommandsMixin;
GPURenderPassEncoder includes GPURenderCommandsMixin;
*/
typedef WGpuObjectBase WGpuRenderPassEncoder;
// Returns true if the given handle references a valid GPURenderPassEncoder.
WGPU_BOOL wgpu_is_render_pass_encoder(WGpuObjectBase object);

void wgpu_render_pass_encoder_set_viewport(WGpuRenderPassEncoder encoder, float x, float y, float width, float height, float minDepth, float maxDepth);
void wgpu_render_pass_encoder_set_scissor_rect(WGpuRenderPassEncoder encoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void wgpu_render_pass_encoder_set_blend_constant(WGpuRenderPassEncoder encoder, double r, double g, double b, double a);
void wgpu_render_pass_encoder_set_stencil_reference(WGpuRenderPassEncoder encoder, uint32_t stencilValue);
void wgpu_render_pass_encoder_begin_occlusion_query(WGpuRenderPassEncoder encoder, int32_t queryIndex);
void wgpu_render_pass_encoder_end_occlusion_query(WGpuRenderPassEncoder encoder);
void wgpu_render_pass_encoder_execute_bundles(WGpuRenderPassEncoder encoder, const WGpuRenderBundle *bundles, int numBundles);
#define wgpu_render_pass_encoder_end wgpu_encoder_end

// Inherited from GPUDebugCommandsMixin:
#define wgpu_render_pass_encoder_push_debug_group wgpu_encoder_push_debug_group
#define wgpu_render_pass_encoder_pop_debug_group wgpu_encoder_pop_debug_group
#define wgpu_render_pass_encoder_insert_debug_marker wgpu_encoder_insert_debug_marker

// Inherited from GPUBindingCommandsMixin:
#define wgpu_render_pass_encoder_set_bind_group wgpu_encoder_set_bind_group

// Inherited from GPURenderCommandsMixin:
#define wgpu_render_pass_encoder_set_pipeline wgpu_render_commands_mixin_set_pipeline
#define wgpu_render_pass_encoder_set_index_buffer wgpu_render_commands_mixin_set_index_buffer
#define wgpu_render_pass_encoder_set_vertex_buffer wgpu_render_commands_mixin_set_vertex_buffer
#define wgpu_render_pass_encoder_draw wgpu_render_commands_mixin_draw
#define wgpu_render_pass_encoder_draw_indexed wgpu_render_commands_mixin_draw_indexed
#define wgpu_render_pass_encoder_draw_indirect wgpu_render_commands_mixin_draw_indirect
#define wgpu_render_pass_encoder_draw_indexed_indirect wgpu_render_commands_mixin_draw_indexed_indirect

/*
dictionary GPURenderPassColorAttachment {
    required (GPUTexture or GPUTextureView) view;
    GPUIntegerCoordinate depthSlice;
    (GPUTexture or GPUTextureView) resolveTarget;

    GPUColor clearValue;
    required GPULoadOp loadOp;
    required GPUStoreOp storeOp;
};
*/
// Defined at the end of this file

/*
dictionary GPURenderPassDepthStencilAttachment {
    required (GPUTexture or GPUTextureView) view;

    float depthClearValue;
    GPULoadOp depthLoadOp;
    GPUStoreOp depthStoreOp;
    boolean depthReadOnly = false;

    GPUStencilValue stencilClearValue = 0;
    GPULoadOp stencilLoadOp;
    GPUStoreOp stencilStoreOp;
    boolean stencilReadOnly = false;
};
*/
typedef struct WGpuRenderPassDepthStencilAttachment
{
  WGpuObjectBase view; // WGpuTexture, or WGpuTextureView

  WGPU_LOAD_OP depthLoadOp; // Either WGPU_LOAD_OP_LOAD (== default, 0) or WGPU_LOAD_OP_CLEAR
  float depthClearValue; // Must be between 0.0 and 1.0, inclusive.

  WGPU_STORE_OP depthStoreOp;
  WGPU_BOOL depthReadOnly;

  WGPU_LOAD_OP stencilLoadOp;  // Either WGPU_LOAD_OP_LOAD (== default, 0) or WGPU_LOAD_OP_CLEAR
  uint32_t stencilClearValue;
  WGPU_STORE_OP stencilStoreOp;
  WGPU_BOOL stencilReadOnly;
} WGpuRenderPassDepthStencilAttachment;
extern const WGpuRenderPassDepthStencilAttachment WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_DEFAULT_INITIALIZER;

VERIFY_STRUCT_SIZE(WGpuRenderPassDepthStencilAttachment, 9*sizeof(uint32_t));

/*
enum GPULoadOp {
    "load",
    "clear",
};
*/
typedef int WGPU_LOAD_OP;
#define WGPU_LOAD_OP_UNDEFINED 0
#define WGPU_LOAD_OP_LOAD 1
#define WGPU_LOAD_OP_CLEAR 2

/*
enum GPUStoreOp {
    "store",
    "discard"
};
*/
typedef int WGPU_STORE_OP;
#define WGPU_STORE_OP_UNDEFINED 0
#define WGPU_STORE_OP_STORE 1
#define WGPU_STORE_OP_DISCARD 2

/*
dictionary GPURenderPassLayout : GPUObjectDescriptorBase {
    required sequence<GPUTextureFormat?> colorFormats;
    GPUTextureFormat depthStencilFormat;
    GPUSize32 sampleCount = 1;
};
*/
// Not currently exposed, fused to GPURenderBundleEncoderDescriptor.

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPURenderBundle {
};
GPURenderBundle includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuRenderBundle;
// Returns true if the given handle references a valid GPURenderBundle.
WGPU_BOOL wgpu_is_render_bundle(WGpuObjectBase object);

/*
dictionary GPURenderBundleDescriptor : GPUObjectDescriptorBase {
};
*/
typedef struct WGpuRenderBundleDescriptor
{
  uint32_t unused_padding; // Appease mixed C and C++ compilation to agree on non-zero struct size.
} WGpuRenderBundleDescriptor;

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPURenderBundleEncoder {
    GPURenderBundle finish(optional GPURenderBundleDescriptor descriptor = {});
};
GPURenderBundleEncoder includes GPUObjectBase;
GPURenderBundleEncoder includes GPUCommandsMixin;
GPURenderBundleEncoder includes GPUDebugCommandsMixin;
GPURenderBundleEncoder includes GPUBindingCommandsMixin;
GPURenderBundleEncoder includes GPURenderCommandsMixin;
*/
typedef WGpuObjectBase WGpuRenderBundleEncoder;
// Returns true if the given handle references a valid GPURenderBundleEncoder.
WGPU_BOOL wgpu_is_render_bundle_encoder(WGpuObjectBase object);
#define wgpu_render_bundle_encoder_finish wgpu_encoder_finish

#define wgpu_render_bundle_encoder_set_bind_group wgpu_encoder_set_bind_group
// Inherited from GPUDebugCommandsMixin:
#define wgpu_render_bundle_encoder_push_debug_group wgpu_encoder_push_debug_group
#define wgpu_render_bundle_encoder_pop_debug_group wgpu_encoder_pop_debug_group
#define wgpu_render_bundle_encoder_insert_debug_marker wgpu_encoder_insert_debug_marker

#define wgpu_render_bundle_encoder_set_pipeline wgpu_encoder_set_pipeline
#define wgpu_render_bundle_encoder_set_index_buffer wgpu_render_commands_mixin_set_index_buffer
#define wgpu_render_bundle_encoder_set_vertex_buffer wgpu_render_commands_mixin_set_vertex_buffer
#define wgpu_render_bundle_encoder_draw wgpu_render_commands_mixin_draw
#define wgpu_render_bundle_encoder_draw_indexed wgpu_render_commands_mixin_draw_indexed
#define wgpu_render_bundle_encoder_draw_indirect wgpu_render_commands_mixin_draw_indirect
#define wgpu_render_bundle_encoder_draw_indexed_indirect wgpu_render_commands_mixin_draw_indexed_indirect

/*
dictionary GPURenderBundleEncoderDescriptor : GPURenderPassLayout {
    boolean depthReadOnly = false;
    boolean stencilReadOnly = false;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuRenderBundleEncoderDescriptor
{
  const WGPU_TEXTURE_FORMAT *colorFormats;
  _WGPU_PTR_PADDING(0);
  int numColorFormats;
  WGPU_TEXTURE_FORMAT depthStencilFormat;
  uint32_t sampleCount; // Default = 1. (pass 1 to disable MSAA, passing 0 here is not valid)
  WGPU_BOOL depthReadOnly;
  WGPU_BOOL stencilReadOnly;
  uint32_t unused_padding;
} WGpuRenderBundleEncoderDescriptor;
extern const WGpuRenderBundleEncoderDescriptor WGPU_RENDER_BUNDLE_ENCODER_DESCRIPTOR_DEFAULT_INITIALIZER;

VERIFY_STRUCT_SIZE(WGpuRenderBundleEncoderDescriptor, 8*sizeof(uint32_t));

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUQueue {
    undefined submit(sequence<GPUCommandBuffer> commandBuffers);

    Promise<undefined> onSubmittedWorkDone();

    undefined writeBuffer(
        GPUBuffer buffer,
        GPUSize64 bufferOffset,
        [AllowShared] BufferSource data,
        optional GPUSize64 dataOffset = 0,
        optional GPUSize64 size);

    undefined writeTexture(
      GPUTexelCopyTextureInfo destination,
      [AllowShared] BufferSource data,
      GPUTexelCopyBufferLayout dataLayout,
      GPUExtent3D size);

    undefined copyExternalImageToTexture(
        GPUCopyExternalImageSourceInfo source,
        GPUCopyExternalImageDestInfo destination,
        GPUExtent3D copySize);
};
GPUQueue includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuQueue;
// Returns true if the given handle references a valid GPUQueue.
WGPU_BOOL wgpu_is_queue(WGpuObjectBase object);

// Submits one command buffer to the given queue for rendering. Then, the command buffer is destroyed. (as if calling wgpu_object_destroy() on it)
// N.b. if you start recording a command buffer and choose not to submit it, then you must manually call wgpu_object_destroy() on it to avoid
// a memory leak. (see function wgpu_get_num_live_objects() to help debug the number of live references)
void wgpu_queue_submit_one_and_destroy(WGpuQueue queue, WGpuCommandBuffer commandBuffer);

// Submits multiple command buffers to the given queue for rendering. Then, the command buffers are destroyed. (as if calling wgpu_object_destroy() on them)
// N.b. if you start recording a command buffer and choose not to submit it, then you must manually call wgpu_object_destroy() on it to avoid
// a memory leak. (see function wgpu_get_num_live_objects() to help debug the number of live references)
void wgpu_queue_submit_multiple_and_destroy(WGpuQueue queue, const WGpuCommandBuffer *commandBuffers, int numCommandBuffers);

typedef void (*WGpuOnSubmittedWorkDoneCallback)(WGpuQueue queue, void *userData);
void wgpu_queue_set_on_submitted_work_done_callback(WGpuQueue queue, WGpuOnSubmittedWorkDoneCallback callback, void *userData);

// Uploads data to the given GPUBuffer. Data is copied from memory in byte addresses data[0], data[1], ... data[size-1], and uploaded
// to the GPU buffer at byte offset bufferOffset, bufferOffset+1, ..., bufferOffset+size-1.
void wgpu_queue_write_buffer(WGpuQueue queue, WGpuBuffer buffer, double_int53_t bufferOffset, const void *data NOTNULL, double_int53_t size);
void wgpu_queue_write_texture(WGpuQueue queue, const WGpuTexelCopyTextureInfo *destination NOTNULL, const void *data NOTNULL, uint32_t bytesPerBlockRow, uint32_t blockRowsPerImage, uint32_t writeWidth, uint32_t writeHeight _WGPU_DEFAULT_VALUE(1), uint32_t writeDepthOrArrayLayers _WGPU_DEFAULT_VALUE(1));
void wgpu_queue_copy_external_image_to_texture(WGpuQueue queue, const WGpuCopyExternalImageSourceInfo *source NOTNULL, const WGpuCopyExternalImageDestInfo *destination NOTNULL, uint32_t copyWidth, uint32_t copyHeight _WGPU_DEFAULT_VALUE(1), uint32_t copyDepthOrArrayLayers _WGPU_DEFAULT_VALUE(1));

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUQuerySet {
    undefined destroy();

    readonly attribute GPUQueryType type;
    readonly attribute GPUSize32 count;
};
GPUQuerySet includes GPUObjectBase;
*/
typedef WGpuObjectBase WGpuQuerySet;
// Returns true if the given handle references a valid GPUQuerySet.
WGPU_BOOL wgpu_is_query_set(WGpuObjectBase object);
// Getters for retrieving query set properties:
WGPU_QUERY_TYPE wgpu_query_set_type(WGpuQuerySet querySet);
uint32_t wgpu_query_set_count(WGpuQuerySet querySet);

/*
dictionary GPUQuerySetDescriptor : GPUObjectDescriptorBase {
    required GPUQueryType type;
    required GPUSize32 count;
};
*/
typedef struct WGpuQuerySetDescriptor
{
  WGPU_QUERY_TYPE type;
  uint32_t count;
} WGpuQuerySetDescriptor;

/*
enum GPUQueryType {
    "occlusion",
    "timestamp"
};
*/
typedef int WGPU_QUERY_TYPE;
#define WGPU_QUERY_TYPE_INVALID 0
#define WGPU_QUERY_TYPE_OCCLUSION 1
#define WGPU_QUERY_TYPE_TIMESTAMP 2

// TODO: Remove, GPUPipelineStatisticName no longer exists
/*
enum GPUPipelineStatisticName {
    "timestamp"
};
*/
typedef int WGPU_PIPELINE_STATISTIC_NAME;
#define WGPU_PIPELINE_STATISTIC_NAME_INVALID 0
#define WGPU_PIPELINE_STATISTIC_NAME_TIMESTAMP 1

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUCanvasContext {
    readonly attribute (HTMLCanvasElement or OffscreenCanvas) canvas;

    undefined configure(GPUCanvasConfiguration configuration);
    undefined unconfigure();

    GPUCanvasConfiguration? getConfiguration();
    GPUTexture getCurrentTexture();
};
*/
typedef WGpuObjectBase WGpuCanvasContext;
// Returns true if the given handle references a valid GPUCanvasContext.
WGPU_BOOL wgpu_is_canvas_context(WGpuObjectBase object);

// TODO: Add char *wgpu_canvas_context_get_canvas_selector_id() for 'canvas' member property, as both CSS ID selector and object ID.

// Configures the swap chain for this context.
#ifdef __EMSCRIPTEN__
void wgpu_canvas_context_configure(WGpuCanvasContext canvasContext, const WGpuCanvasConfiguration *config NOTNULL);

// Reads the configuration of the given canvas context.
// Note: If getConfiguration() returns an empty object, then this function will return a null pointer.
// IMPORTANT! The return value from this function is malloc()ed. Call free() on the returned pointer to avoid leaking memory.
// (there intentionally exists no separate wgpu_canvas_context_get_configuration_free() or such, but plain free() is ok)
WGpuCanvasConfiguration *wgpu_canvas_context_get_configuration(WGpuCanvasContext canvasContext);
#else
void wgpu_canvas_context_configure(WGpuCanvasContext canvasContext, const WGpuCanvasConfiguration *config NOTNULL, int width _WGPU_DEFAULT_VALUE(0), int height _WGPU_DEFAULT_VALUE(0));
#endif
void wgpu_canvas_context_unconfigure(WGpuCanvasContext canvasContext);

WGpuTexture wgpu_canvas_context_get_current_texture(WGpuCanvasContext canvasContext);

// Native Dawn implementation has a concept of a SwapChain. Web does not have this.
// Dawn SwapChain returns a TextureView, whereas GPUCanvasContext.getCurrentTexture() returns a Texture,
// so provide a convenient function wgpu_canvas_context_get_current_texture_view() to obtain
// a TextureView in a cross-platform manner.
#ifdef __EMSCRIPTEN__
#define wgpu_canvas_context_get_current_texture_view(canvasContext) wgpu_texture_create_view_simple(wgpu_canvas_context_get_current_texture((canvasContext)))
#else
WGpuTextureView wgpu_canvas_context_get_current_texture_view(WGpuCanvasContext canvasContext);
#endif

#ifndef __EMSCRIPTEN__
void wgpu_canvas_context_present(WGpuCanvasContext canvasContext);
#endif

/*
enum GPUCanvasAlphaMode {
    "opaque",
    "premultiplied",
};
*/
typedef int WGPU_CANVAS_ALPHA_MODE;
#define WGPU_CANVAS_ALPHA_MODE_INVALID 0
#define WGPU_CANVAS_ALPHA_MODE_OPAQUE 1
#define WGPU_CANVAS_ALPHA_MODE_PREMULTIPLIED 2

/*
enum GPUDeviceLostReason {
    "unknown",
    "destroyed",
};
*/
typedef int WGPU_DEVICE_LOST_REASON;
#define WGPU_DEVICE_LOST_REASON_UNKNOWN 0
#define WGPU_DEVICE_LOST_REASON_DESTROYED 1

/*
[Exposed=(Window, DedicatedWorker)]
interface GPUDeviceLostInfo {
    readonly attribute GPUDeviceLostReason reason;
    readonly attribute DOMString message;
};

partial interface GPUDevice {
    readonly attribute Promise<GPUDeviceLostInfo> lost;
};
*/
typedef void (*WGpuDeviceLostCallback)(WGpuDevice device, WGPU_DEVICE_LOST_REASON deviceLostReason, const char *message NOTNULL, void *userData);
void wgpu_device_set_lost_callback(WGpuDevice device, WGpuDeviceLostCallback callback, void *userData);

// Specifies the type of an error that occurred.
// N.b. the values of these should be kept in sync with values of WGPU_ERROR_FILTER_*. (except for the unknown error value)
typedef int WGPU_ERROR_TYPE;
#define WGPU_ERROR_TYPE_NO_ERROR      0
#define WGPU_ERROR_TYPE_OUT_OF_MEMORY 1
#define WGPU_ERROR_TYPE_VALIDATION    2
#define WGPU_ERROR_TYPE_INTERNAL      3
#define WGPU_ERROR_TYPE_UNKNOWN       4

/*
[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUError {
    readonly attribute DOMString message;
};

[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUValidationError : GPUError {
    constructor(DOMString message);
};

[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUOutOfMemoryError : GPUError {
    constructor(DOMString message);
};

[Exposed=(Window, DedicatedWorker), SecureContext]
interface GPUInternalError : GPUError {
    constructor(DOMString message);
};

enum GPUErrorFilter {
    "validation",
    "out-of-memory",
    "internal"
};
*/
typedef int WGPU_ERROR_FILTER;
#define WGPU_ERROR_FILTER_NO_ERROR      0
#define WGPU_ERROR_FILTER_OUT_OF_MEMORY 1
#define WGPU_ERROR_FILTER_VALIDATION    2
#define WGPU_ERROR_FILTER_INTERNAL      3

/*
partial interface GPUDevice {
    undefined pushErrorScope(GPUErrorFilter filter);
    Promise<GPUError?> popErrorScope();
};
*/
void wgpu_device_push_error_scope(WGpuDevice device, WGPU_ERROR_FILTER filter);

// Callback type for popped error scopes.
// errorType: The type of error that occurred, or WGPU_ERROR_FILTER_NO_ERROR.
// errorMessage: Points to string message of the error, or null pointer if no error occurred.
typedef void (*WGpuDeviceErrorCallback)(WGpuDevice device, WGPU_ERROR_TYPE errorType, const char *errorMessage, void *userData);
void wgpu_device_pop_error_scope_async(WGpuDevice device, WGpuDeviceErrorCallback callback, void *userData);

// Synchronously pop an error scope.
// Returns the type of an error that occurred, or WGPU_ERROR_FILTER_NO_ERROR if no error.
// dstErrorMessage: A pointer to a buffer area to receive the error message, if one exists.
// errorMessageLength: Number of bytes that can be written to dstErrorMessage.
WGPU_ERROR_TYPE wgpu_device_pop_error_scope_sync(WGpuDevice device, char *dstErrorMessage, int errorMessageLength);
/*
[
    Exposed=(Window, DedicatedWorker)
]
interface GPUUncapturedErrorEvent : Event {
    constructor(
        DOMString type,
        GPUUncapturedErrorEventInit gpuUncapturedErrorEventInitDict
    );
    [SameObject] readonly attribute GPUError error;
};

dictionary GPUUncapturedErrorEventInit : EventInit {
    required GPUError error;
};

partial interface GPUDevice {
    attribute EventHandler onuncapturederror;
};
*/

// registers a device uncapturederror event callback. Call with 0 pointer to unregister. Only one callback handler is supported, new call overwrites previous
void wgpu_device_set_uncapturederror_callback(WGpuDevice device, WGpuDeviceErrorCallback callback, void *userData);

/*
typedef [EnforceRange] unsigned long GPUBufferDynamicOffset;
typedef [EnforceRange] unsigned long GPUStencilValue;
typedef [EnforceRange] unsigned long GPUSampleMask;
typedef [EnforceRange] long GPUDepthBias;
*/
// These do not get their own typedefs for readability, but use uint32_t in headers.

/*
typedef [EnforceRange] unsigned long long GPUSize64;
*/
// No custom typedef for readability, use double_int53_t

/*
typedef [EnforceRange] unsigned long GPUIntegerCoordinate;
typedef [EnforceRange] unsigned long GPUIndex32;
typedef [EnforceRange] unsigned long GPUSize32;
*/
// These do not get their own typedefs for readability, but use uint32_t in headers.

/*
typedef [EnforceRange] long GPUSignedOffset32;
*/
// No custom typedef for readability, use int32_t.

/*
typedef unsigned long GPUFlagsConstant;
*/
// No custom typedef for readability, use uint32_t.

/*
dictionary GPUColorDict {
    required double r;
    required double g;
    required double b;
    required double a;
};
typedef (sequence<double> or GPUColorDict) GPUColor;
*/
typedef struct WGpuColor
{
  double r, g, b, a;
} WGpuColor;

/*
dictionary GPUOrigin2DDict {
    GPUIntegerCoordinate x = 0;
    GPUIntegerCoordinate y = 0;
};
typedef (sequence<GPUIntegerCoordinate> or GPUOrigin2DDict) GPUOrigin2D;
*/
typedef struct WGpuOrigin2D
{
  int x, y;
} WGpuOrigin2D;

/*
dictionary GPUOrigin3DDict {
    GPUIntegerCoordinate x = 0;
    GPUIntegerCoordinate y = 0;
    GPUIntegerCoordinate z = 0;
};
typedef (sequence<GPUIntegerCoordinate> or GPUOrigin3DDict) GPUOrigin3D;
*/
typedef struct WGpuOrigin3D
{
  int x, y, z;
} WGpuOrigin3D;

////////////////////////////////////////////////////////
// Sorted struct definitions for proper C parsing order:

/*
enum GPUCanvasToneMappingMode {
    "standard",
    "extended",
};
*/
typedef int WGPU_CANVAS_TONE_MAPPING_MODE;
#define WGPU_CANVAS_TONE_MAPPING_MODE_INVALID  0
#define WGPU_CANVAS_TONE_MAPPING_MODE_STANDARD 1
#define WGPU_CANVAS_TONE_MAPPING_MODE_EXTENDED 2

/*
dictionary GPUCanvasToneMapping {
  GPUCanvasToneMappingMode mode = "standard";
};
*/
typedef struct WGpuCanvasToneMapping
{
  WGPU_CANVAS_TONE_MAPPING_MODE mode; // = 'standard';
} WGpuCanvasToneMapping;
extern const WGpuCanvasToneMapping WGPU_CANVAS_TONE_MAPPING_DEFAULT_INITIALIZER;

/*
dictionary GPUCanvasConfiguration : GPUObjectDescriptorBase {
    required GPUDevice device;
    required GPUTextureFormat format;
    GPUTextureUsageFlags usage = 0x10;  // GPUTextureUsage.RENDER_ATTACHMENT
    sequence<GPUTextureFormat> viewFormats = [];
    PredefinedColorSpace colorSpace = "srgb";
    GPUCanvasToneMapping toneMapping = {};
    GPUCanvasAlphaMode alphaMode = "opaque";
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuCanvasConfiguration
{
  WGpuDevice device;
  WGPU_TEXTURE_FORMAT format;
  WGPU_TEXTURE_USAGE_FLAGS usage;
  int numViewFormats;
  WGPU_TEXTURE_FORMAT *viewFormats;
  _WGPU_PTR_PADDING(0);
  HTML_PREDEFINED_COLOR_SPACE colorSpace;
  WGpuCanvasToneMapping toneMapping;
  WGPU_CANVAS_ALPHA_MODE alphaMode;
  uint32_t unused_padding;
} WGpuCanvasConfiguration;
extern const WGpuCanvasConfiguration WGPU_CANVAS_CONFIGURATION_DEFAULT_INITIALIZER;

// N.b. this struct is malloc()ed in wgpu_canvas_context_get_configuration() in JS side, so if this size changes, remember to update JS side.
VERIFY_STRUCT_SIZE(WGpuCanvasConfiguration, 10*sizeof(uint32_t));

/*
dictionary GPURenderPassTimestampWrites {
    required GPUQuerySet querySet;
    GPUSize32 beginningOfPassWriteIndex;
    GPUSize32 endOfPassWriteIndex;
};
*/
typedef struct WGpuRenderPassTimestampWrites
{
  WGpuQuerySet querySet;
  int32_t beginningOfPassWriteIndex; // Important! Specify -1 to not perform a write at beginning of pass. Setting 0 will write to index 0. Ignored if querySet == 0.
  int32_t endOfPassWriteIndex;       // Important! Specify -1 to not perform a write at end of pass. Setting 0 will write to index 0. Ignored if querySet == 0.
} WGpuRenderPassTimestampWrites;
extern const WGpuRenderPassTimestampWrites WGPU_RENDER_PASS_TIMESTAMP_WRITES_DEFAULT_INITIALIZER;

/*
typedef sequence<GPURenderPassTimestampWrite> GPURenderPassTimestampWrites;

dictionary GPURenderPassDescriptor : GPUObjectDescriptorBase {
    required sequence<GPURenderPassColorAttachment?> colorAttachments;
    GPURenderPassDepthStencilAttachment depthStencilAttachment;
    GPUQuerySet occlusionQuerySet;
    GPURenderPassTimestampWrites timestampWrites = [];
    GPUSize64 maxDrawCount = 50000000;
};
*/
typedef struct _WGPU_ALIGN_TO_64BITS WGpuRenderPassDescriptor
{
  double_int53_t maxDrawCount; // If set to zero, the default value (50000000) will be used.
  const WGpuRenderPassColorAttachment *colorAttachments;
  _WGPU_PTR_PADDING(0);
  int numColorAttachments;
  WGpuRenderPassDepthStencilAttachment depthStencilAttachment;
  WGpuQuerySet occlusionQuerySet;
  WGpuRenderPassTimestampWrites timestampWrites;
} WGpuRenderPassDescriptor;
extern const WGpuRenderPassDescriptor WGPU_RENDER_PASS_DESCRIPTOR_DEFAULT_INITIALIZER;

typedef struct WGpuRenderPassColorAttachment
{
  WGpuObjectBase view; // WGpuTexture, or WGpuTextureView
  int depthSlice;
  WGpuObjectBase resolveTarget; // WGpuTexture, or WGpuTextureView. Describes the texture subresource that will receive the resolved output for this color attachment if view is multisampled.

  WGPU_STORE_OP storeOp; // Required, be sure to set to WGPU_STORE_OP_STORE (default) or WGPU_STORE_OP_DISCARD
  WGPU_LOAD_OP loadOp; // Either WGPU_LOAD_OP_LOAD (== default, 0) or WGPU_LOAD_OP_CLEAR.
  uint32_t unused_padding; // unused, added to pad the doubles in clearValue to 8-byte multiples.
  WGpuColor clearValue; // Used if loadOp == WGPU_LOAD_OP_CLEAR. Default value = { r = 0.0, g = 0.0, b = 0.0, a = 1.0 }
} WGpuRenderPassColorAttachment;
extern const WGpuRenderPassColorAttachment WGPU_RENDER_PASS_COLOR_ATTACHMENT_DEFAULT_INITIALIZER;

VERIFY_STRUCT_SIZE(WGpuRenderPassColorAttachment, 14*sizeof(uint32_t));

typedef struct WGpuCopyExternalImageSourceInfo
{
  WGpuObjectBase source; // must point to a WGpuImageBitmap (could also point to a HTMLVideoElement, HTMLCanvasElement or OffscreenCanvas, but those are currently unimplemented)
  WGpuOrigin2D origin;
  WGPU_BOOL flipY; // defaults to false.
} WGpuCopyExternalImageSourceInfo;
extern const WGpuCopyExternalImageSourceInfo WGPU_COPY_EXTERNAL_IMAGE_SOURCE_INFO_DEFAULT_INITIALIZER;

typedef struct WGpuTexelCopyTextureInfo
{
  WGpuTexture texture;
  uint32_t mipLevel;
  WGpuOrigin3D origin;
  WGPU_TEXTURE_ASPECT aspect;
} WGpuTexelCopyTextureInfo;
extern const WGpuTexelCopyTextureInfo WGPU_TEXEL_COPY_TEXTURE_INFO_DEFAULT_INITIALIZER;

typedef struct WGpuCopyExternalImageDestInfo
{
  // WGpuTexelCopyTextureInfo part:
  WGpuTexture texture;
  uint32_t mipLevel;
  WGpuOrigin3D origin;
  WGPU_TEXTURE_ASPECT aspect;

  HTML_PREDEFINED_COLOR_SPACE colorSpace; // = "srgb";
  WGPU_BOOL premultipliedAlpha; // = false;
} WGpuCopyExternalImageDestInfo;
extern const WGpuCopyExternalImageDestInfo WGPU_COPY_EXTERNAL_IMAGE_DEST_INFO_DEFAULT_INITIALIZER;

typedef struct WGpuDepthStencilState
{
  // Pass format == WGPU_TEXTURE_FORMAT_INVALID (integer value 0)
  // to disable depth+stenciling altogether.
  WGPU_TEXTURE_FORMAT format;

  WGPU_BOOL depthWriteEnabled;
  WGPU_COMPARE_FUNCTION depthCompare;

  uint32_t stencilReadMask;
  uint32_t stencilWriteMask;

  int32_t depthBias;
  float depthBiasSlopeScale;
  float depthBiasClamp;

  WGpuStencilFaceState stencilFront;
  WGpuStencilFaceState stencilBack;

  // Enable depth clamping (requires "depth-clamping" feature)
  WGPU_BOOL clampDepth;
} WGpuDepthStencilState;

VERIFY_STRUCT_SIZE(WGpuDepthStencilState, 17*sizeof(uint32_t));

typedef struct WGpuBlendState
{
  WGpuBlendComponent color;
  WGpuBlendComponent alpha;
} WGpuBlendState;

typedef struct WGpuColorTargetState
{
  WGPU_TEXTURE_FORMAT format;

  // The member field blend.operation is default initialized to WGPU_BLEND_OPERATION_DISABLED (integer value 0)
  // to disable alpha blending on this color target. Set blend.operation to e.g. WGPU_BLEND_OPERATION_ADD to enable
  // alpha blending.
  WGpuBlendState blend;

  WGPU_COLOR_WRITE_FLAGS writeMask;
} WGpuColorTargetState;
extern const WGpuColorTargetState WGPU_COLOR_TARGET_STATE_DEFAULT_INITIALIZER;

typedef struct _WGPU_ALIGN_TO_64BITS WGpuRenderPipelineDescriptor
{
  WGpuVertexState vertex;
  WGpuPrimitiveState primitive;
  WGpuDepthStencilState depthStencil;
  WGpuMultisampleState multisample;
  uint32_t unused_padding;
  WGpuFragmentState fragment;
  WGpuPipelineLayout layout; // Set to special value WGPU_AUTO_LAYOUT_MODE_AUTO to specify that automatic layout should be used.
  uint32_t unused_padding2;
} WGpuRenderPipelineDescriptor;
extern const WGpuRenderPipelineDescriptor WGPU_RENDER_PIPELINE_DESCRIPTOR_DEFAULT_INITIALIZER;

typedef struct WGpuBindGroupLayoutEntry
{
  uint32_t binding;
  WGPU_SHADER_STAGE_FLAGS visibility;
  WGPU_BIND_GROUP_LAYOUT_TYPE type;
  uint32_t unused_padding; // Explicitly present to pad 'buffer' to 64-bit alignment

  union {
    WGpuBufferBindingLayout buffer;
    WGpuSamplerBindingLayout sampler;
    WGpuTextureBindingLayout texture;
    WGpuStorageTextureBindingLayout storageTexture;
    WGpuExternalTextureBindingLayout externalTexture;
  } layout;
} WGpuBindGroupLayoutEntry;
extern const WGpuBindGroupLayoutEntry WGPU_BUFFER_BINDING_LAYOUT_ENTRY_DEFAULT_INITIALIZER;

////////////////////////////////////////////////////////////////
// Extensions to the WebGPU specification:

typedef WGpuObjectBase WGpuImageBitmap;

// Called when the ImageBitmap finishes loading. If loading fails, this callback will be called with width==height==0.
typedef void (*WGpuLoadImageBitmapCallback)(WGpuImageBitmap bitmap, int width, int height, void *userData);

void wgpu_load_image_bitmap_from_url_async(const char *url NOTNULL, WGPU_BOOL flipY, WGpuLoadImageBitmapCallback callback, void *userData);

// This function is available when building with JSPI enabled. It performs three things:
// 1) presents all canvases that have been rendered to from the current scope of execution.
// 2) yields back to browser's event loop with JSPI, so processes all pending browser events (keyboard, mouse, etc.)
// 3) sleeps the calling thread until the next requestAnimationFrame event.
void wgpu_present_all_rendering_and_wait_for_next_animation_frame(void);

#ifdef __EMSCRIPTEN__
// Creates a new OffscreenCanvas object that is not associated with any HTML Canvas element on the web page.
// Use this function to perform offline background rendering.
// This function can be called directly in a Wasm Worker/pthread. (i.e. one does not need to call it in a main
// thread and then post it to a Worker, although that is also possible)
// id: A custom ID number that you will use in other functions to refer to this Offscreen Canvas. You can
//     choose this ID any way you want, i.e. up to you to make it unique across multiple OffscreenCanvases you might create.
//     Value 0 is not a valid ID to assign.
// Call wgpuOffscreenCanvases[offscreenCanvasId].convertToBlob() or wgpuOffscreenCanvases[offscreenCanvasId].transferToImageBitmap()
// in hand-written JavaScript code to access the result of the rendering.
// Note that if you want to render to a visible Canvas on a web page from a background Worker, do not call this function,
// but instead call canvas_transfer_control_to_offscreen(). See samples/offscreen_canvas/offscreen_canvas.c for a full example.
void offscreen_canvas_create(OffscreenCanvasId id, int width, int height);

// Converts the given Canvas in HTML DOM (located via a given CSS selector) to be rendered via an OffscreenCanvas,
// by calling .transferControlToOffscreen() on it. Calling this function creates a new OffscreenCanvas object.
// Call this function at most once for any given HTML Canvas element.
// There is no browser API to undo the effects of this call (except to delete and recreate a new Canvas element)
// id: A custom ID to assign to the newly created OffscreenCanvas object, like with offscreen_canvas_create().
void canvas_transfer_control_to_offscreen(const char *canvasSelector NOTNULL, OffscreenCanvasId id);

// Transfers the ownership of the given OffscreenCanvas to the given Wasm Worker.
// id: The ID of an OffscreenCanvas created via canvas_transfer_control_to_offscreen().
// worker: The ID of a Wasm Worker created e.g. via emscripten_malloc_wasm_worker().
//         Pass special ID worker=EMSCRIPTEN_WASM_WORKER_ID_PARENT her to transfer ownership
//         of an OffscreenCanvas back from the current Worker thread its parent thread (the main thread).
// Note: After calling this function, the Offscreen Canvas no longer exists in the current thread, the effect being
//       as if offscreen_canvas_destroy(id) had been called in this thread.
void offscreen_canvas_post_to_worker(OffscreenCanvasId id, emscripten_wasm_worker_t worker);

// Same as above, but used when operating with pthreads.
void offscreen_canvas_post_to_pthread(OffscreenCanvasId id, pthread_t pthread);

// Returns true if the given Offscreen Canvas ID is valid and is owned by the calling thread.
// If an Offscreen Canvas with the given ID exists in the ownership of another thread, this function will return
// false.
WGPU_BOOL offscreen_canvas_is_valid(OffscreenCanvasId id);

// Releases reference to the given Offscreen Canvas in the calling thread, if one exists. A no-op if no canvas
// with given ID exists.
void offscreen_canvas_destroy(OffscreenCanvasId id);

// These functions return the width/height of render surface the given OffscreenCanvas element.
// The OffscreenCanvas must be owned by the calling thread.
// Note that this size does not refer to the visible size of the Canvas. The visible size is governed by a CSS
// style on the HTML page (i.e. canvas.style.width and canvas.style.height JS properties). To adjust the visible
// size on the HTML page, see the Emscripten function emscripten_get_element_css_size(). (which must be called
// on the main thread, and not on the Worker thread)
int offscreen_canvas_width(OffscreenCanvasId id);
int offscreen_canvas_height(OffscreenCanvasId id);
void offscreen_canvas_size(OffscreenCanvasId id, int *outWidth NOTNULL, int *outHeight NOTNULL);

// Sets the render surface size of the given OffscreenCanvas element.
// The OffscreenCanvas must be owned by the calling thread. Note this is not the visible CSS size, see above.
// Use the function emscripten_set_element_css_size() on the main thread to set the visible size.
void offscreen_canvas_set_size(OffscreenCanvasId id, int width, int height);

#endif

// EXPERIMENTAL: Not part of the ratified spec yet.
// setImmediateData: https://github.com/gpuweb/gpuweb/blob/main/proposals/immediate-data.md
// encoder: The current render pass, compute pass or render bundle encoder.
// offset: Offset into the GPU-side immediate data area to upload to. In range 0 - 64. Condition offset + size <= 64 must hold.
// ptr: CPU-side starting address of the data to upload.
// size: The number of bytes to upload. This must be a multiple of 4 bytes.
void wgpu_encoder_set_immediate_data(WGpuBindingCommandsMixin encoder, uint32_t offset, const void *ptr, uint32_t size);

#ifdef __cplusplus
} // ~extern "C"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
