#include "gpu.h"
#include <string.h>
#include <GL/glcorearb.h>

// Loader

#define GL_FOREACH(X)\
  X(glGetIntegerv, GLGETINTEGERV)\
  X(glEnable, GLENABLE)\
  X(glDisable, GLDISABLE)\
  X(glVertexBindingDivisor, GLVERTEXBINDINGDIVISOR)\
  X(glVertexAttribBinding, GLVERTEXATTRIBBINDING)\
  X(glVertexAttribFormat, GLVERTEXATTRIBFORMAT)\
  X(glVertexAttribIFormat, GLVERTEXATTRIBIFORMAT)\
  X(glBindVertexBuffer, GLBINDVERTEXBUFFER)\
  X(glCullFace, GLCULLFACE)\
  X(glFrontFace, GLFRONTFACE)\
  X(glPolygonOffset, GLPOLYGONOFFSET)\
  X(glDepthMask, GLDEPTHMASK)\
  X(glDepthFunc, GLDEPTHFUNC)\
  X(glColorMask, GLCOLORMASK)\
  X(glBlendFuncSeparate, GLBLENDFUNCSEPARATE)\
  X(glBlendEquationSeparate, GLBLENDEQUATIONSEPARATE)\
  X(glDrawArraysInstanced, GLDRAWARRAYSINSTANCED)\
  X(glDrawElementsInstancedBaseVertex, GLDRAWELEMENTSINSTANCEDBASEVERTEX)\
  X(glMultiDrawArraysIndirect, GLMULTIDRAWARRAYSINDIRECT)\
  X(glMultiDrawElementsIndirect, GLMULTIDRAWELEMENTSINDIRECT)\
  X(glDispatchCompute, GLDISPATCHCOMPUTE)\
  X(glGenVertexArrays, GLGENVERTEXARRAYS)\
  X(glDeleteVertexArrays, GLDELETEVERTEXARRAYS)\
  X(glBindVertexArray, GLBINDVERTEXARRAY)\
  X(glGenBuffers, GLGENBUFFERS)\
  X(glDeleteBuffers, GLDELETEBUFFERS)\
  X(glBindBuffer, GLBINDBUFFER)\
  X(glBufferStorage, GLBUFFERSTORAGE)\
  X(glMapBufferRange, GLMAPBUFFERRANGE)\
  X(glFlushMappedBufferRange, GLFLUSHMAPPEDBUFFERRANGE)\
  X(glUnmapBuffer, GLUNMAPBUFFER)\
  X(glInvalidateBufferData, GLINVALIDATEBUFFERDATA)\
  X(glGenTextures, GLGENTEXTURES)\
  X(glDeleteTextures, GLDELETETEXTURES)\
  X(glBindTexture, GLBINDTEXTURE)\
  X(glTexStorage2D, GLTEXSTORAGE2D)\
  X(glTexStorage3D, GLTEXSTORAGE3D)\
  X(glTexSubImage2D, GLTEXSUBIMAGE2D)\
  X(glTexSubImage3D, GLTEXSUBIMAGE3D)\
  X(glGenFramebuffers, GLGENFRAMEBUFFERS)\
  X(glDeleteFramebuffers, GLDELETEFRAMEBUFFERS)\
  X(glBindFramebuffer, GLBINDFRAMEBUFFER)\
  X(glFramebufferTexture2D, GLFRAMEBUFFERTEXTURE2D)\
  X(glFramebufferTextureLayer, GLFRAMEBUFFERTEXTURELAYER)\
  X(glCheckFramebufferStatus, GLCHECKFRAMEBUFFERSTATUS)\
  X(glDrawBuffers, GLDRAWBUFFERS)\
  X(glCreateProgram, GLCREATEPROGRAM)\
  X(glDeleteProgram, GLDELETEPROGRAM)\
  X(glUseProgram, GLUSEPROGRAM)\

#define GL_DECLARE(fn, upper) static PFN##upper##PROC fn;
#define GL_LOAD(fn, upper) fn = (PFN##upper##PROC) config->getProcAddress(#fn);

// Types

struct gpu_buffer {
  GLuint id;
  GLenum target;
  uint8_t* data;
  uint64_t size;
};

struct gpu_texture {
  GLuint id;
  GLenum target;
  GLenum format;
  GLenum pixelFormat;
  GLenum pixelType;
};

struct gpu_canvas {
  GLuint id;
};

struct gpu_shader {
  GLuint id;
};

struct gpu_pipeline {
  gpu_pipeline_info info;
};

static struct {
  GLuint vertexArray;
  GLsizei bufferStrides[16];
  gpu_pipeline_info cache;
  gpu_pipeline* pipeline;
  gpu_canvas* canvas;
} state;

// Setup

GL_FOREACH(GL_DECLARE)

bool gpu_init(gpu_config* config) {
  GL_FOREACH(GL_LOAD);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
  glGenVertexArrays(1, &state.vertexArray);
  glBindVertexArray(state.vertexArray);
  return true;
}

void gpu_destroy(void) {
  glDeleteVertexArrays(1, &state.vertexArray);
}

void gpu_frame_wait(void) {
  //
}

void gpu_frame_finish(void) {
  //
}

void gpu_render_begin(gpu_canvas* canvas) {
  glBindFramebuffer(GL_FRAMEBUFFER, canvas->id);
  state.canvas = canvas;
}

void gpu_render_finish(void) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  state.canvas = NULL;
}

void gpu_set_pipeline(gpu_pipeline* pipeline) {
  if (state.pipeline == pipeline) {
    return;
  }

  gpu_pipeline_info* my = &state.cache;
  gpu_pipeline_info* new = &pipeline->info;

  if (my->shader != new->shader) {
    glUseProgram(new->shader->id);
    my->shader = new->shader;
  }

  for (uint32_t i = 0; i < 16; i++) {
    if (my->buffers[i].divisor != new->buffers[i].divisor) {
      glVertexBindingDivisor(i, new->buffers[i].divisor);
      my->buffers[i].divisor = new->buffers[i].divisor;
    }
  }

  for (uint32_t i = 0; i < 16; i++) {
    static const struct { GLint count; GLenum type; GLboolean normalized; } formats[] = {
      [GPU_FLOAT_F32] = { 1, GL_FLOAT, GL_FALSE },
      [GPU_VEC2_F32] = { 2, GL_FLOAT, GL_FALSE },
      [GPU_VEC2_U16N] = { 2, GL_UNSIGNED_SHORT, GL_TRUE },
      [GPU_VEC2_I16N] = { 2, GL_SHORT, GL_TRUE },
      [GPU_VEC3_F32] = { 3, GL_FLOAT, GL_FALSE },
      [GPU_VEC4_F32] = { 4, GL_FLOAT, GL_FALSE },
      [GPU_VEC4_F16] = { 4, GL_HALF_FLOAT, GL_FALSE },
      [GPU_VEC4_U16N] = { 4, GL_UNSIGNED_SHORT, GL_TRUE },
      [GPU_VEC4_I16N] = { 4, GL_SHORT, GL_TRUE },
      [GPU_VEC4_U8N] = { 4, GL_UNSIGNED_BYTE, GL_TRUE },
      [GPU_VEC4_I8N] = { 4, GL_BYTE, GL_TRUE },
      [GPU_UINT_U32] = { 1, GL_UNSIGNED_INT },
      [GPU_UVEC2_U32] = { 2, GL_UNSIGNED_INT },
      [GPU_UVEC3_U32] = { 3, GL_UNSIGNED_INT },
      [GPU_UVEC4_U32] = { 4, GL_UNSIGNED_INT },
      [GPU_INT_I32] = { 1, GL_INT },
      [GPU_IVEC2_I32] = { 2, GL_INT },
      [GPU_IVEC3_I32] = { 3, GL_INT },
      [GPU_IVEC4_I32] = { 4, GL_INT }
    };
    if (memcmp(&my->attributes[i], &new->attributes[i], sizeof(gpu_attribute))) {
      GLuint location = new->attributes[i].location;
      GLint size = formats[new->attributes[i].format].count;
      GLenum type = formats[new->attributes[i].format].type;
      GLboolean normalized = formats[new->attributes[i].format].normalized;
      GLuint offset = new->attributes[i].offset;
      glVertexAttribBinding(location, new->attributes[i].buffer);
      if (new->attributes[i].format >= GPU_UINT_U32) {
        glVertexAttribIFormat(location, size, type, offset);
      } else {
        glVertexAttribFormat(location, size, type, normalized, offset);
      }
      memcpy(&my->attributes[i], &new->attributes[i], sizeof(gpu_attribute));
    }
  }

  my->drawMode = new->drawMode;

  if (my->cullMode != new->cullMode) {
    if (new->cullMode == GPU_CULL_NONE) {
      glDisable(GL_CULL_FACE);
    } else {
      glEnable(GL_CULL_FACE);
      glCullFace(new->cullMode == GPU_CULL_FRONT ? GL_FRONT : GL_BACK);
    }
    my->cullMode = new->cullMode;
  }

  if (my->winding != new->winding) {
    glFrontFace(new->winding == GPU_WINDING_CCW ? GL_CCW : GL_CW);
    my->winding = new->winding;
  }

  if (my->depthOffset != new->depthOffset || my->depthOffsetSloped != new->depthOffsetSloped) {
    glPolygonOffset(new->depthOffsetSloped, new->depthOffset);
    my->depthOffset = new->depthOffset;
    my->depthOffsetSloped = new->depthOffsetSloped;
  }

  if (my->depthWrite != new->depthWrite) {
    glDepthMask(new->depthWrite);
    my->depthWrite = new->depthWrite;
  }

  if (my->depthTest != new->depthTest) {
    static const GLenum funcs[] = {
      [GPU_COMPARE_EQUAL] = GL_EQUAL,
      [GPU_COMPARE_NEQUAL] = GL_NOTEQUAL,
      [GPU_COMPARE_LESS] = GL_LESS,
      [GPU_COMPARE_LEQUAL] = GL_LEQUAL,
      [GPU_COMPARE_GREATER] = GL_GREATER,
      [GPU_COMPARE_GEQUAL] = GL_GEQUAL
    };
    if (new->depthTest == GPU_COMPARE_NONE) {
      glDisable(GL_DEPTH_TEST);
    } else {
      if (my->depthTest == GPU_COMPARE_NONE) {
        glEnable(GL_DEPTH_TEST);
      }
      glDepthFunc(funcs[new->depthTest]);
    }
    my->depthTest = new->depthTest;
  }

  // stencil

  if (my->alphaToCoverage != new->alphaToCoverage) {
    if (new->alphaToCoverage) {
      glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
      glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    my->alphaToCoverage = new->alphaToCoverage;
  }

  if (my->colorMask != new->colorMask) {
    glColorMask(new->colorMask & 0x8, new->colorMask & 0x4, new->colorMask & 0x2, new->colorMask & 0x1);
    my->colorMask = new->colorMask;
  }

  if (my->blend.enabled != new->blend.enabled) {
    if (new->blend.enabled) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
    my->blend.enabled = new->blend.enabled;
  }

  if (memcmp(&my->blend, &new->blend, sizeof(my->blend))) {
    static const GLenum factors[] = {
      [GPU_BLEND_ZERO] = GL_ZERO,
      [GPU_BLEND_ONE] = GL_ONE,
      [GPU_BLEND_SRC_COLOR] = GL_SRC_COLOR,
      [GPU_BLEND_ONE_MINUS_SRC_COLOR] = GL_ONE_MINUS_SRC_COLOR,
      [GPU_BLEND_SRC_ALPHA] = GL_SRC_ALPHA,
      [GPU_BLEND_ONE_MINUS_SRC_ALPHA] = GL_ONE_MINUS_SRC_ALPHA,
      [GPU_BLEND_DST_COLOR] = GL_DST_COLOR,
      [GPU_BLEND_ONE_MINUS_DST_COLOR] = GL_ONE_MINUS_DST_COLOR,
      [GPU_BLEND_DST_ALPHA] = GL_DST_ALPHA,
      [GPU_BLEND_ONE_MINUS_DST_ALPHA] = GL_ONE_MINUS_DST_ALPHA
    };
    static const GLenum opps[] = {
      [GPU_BLEND_ADD] = GL_FUNC_ADD,
      [GPU_BLEND_SUB] = GL_FUNC_SUBTRACT,
      [GPU_BLEND_RSUB] = GL_FUNC_REVERSE_SUBTRACT,
      [GPU_BLEND_MIN] = GL_MIN,
      [GPU_BLEND_MAX] = GL_MAX
    };
    glBlendFuncSeparate(factors[new->blend.color.src], factors[new->blend.color.dst], factors[new->blend.alpha.src], factors[new->blend.alpha.dst]);
    glBlendEquationSeparate(opps[new->blend.color.op], opps[new->blend.alpha.op]);
    memcpy(&my->blend, &new->blend, sizeof(my->blend));
  }

  state.pipeline = pipeline;
}

void gpu_set_vertex_buffers(gpu_buffer** buffers, uint64_t* offsets, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) { // Not convinced multibind would be any better here
    glBindVertexBuffer(i, buffers[i] ? buffers[i]->id : 0, offsets[i], state.bufferStrides[i]);
  }
}

void gpu_set_index_buffer(gpu_buffer* buffer, uint64_t offset) {
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->id);
}

static const GLenum drawModes[] = {
  [GPU_DRAW_POINTS] = GL_POINTS,
  [GPU_DRAW_LINES] = GL_LINES,
  [GPU_DRAW_LINE_STRIP] = GL_LINE_STRIP,
  [GPU_DRAW_TRIANGLES] = GL_TRIANGLES,
  [GPU_DRAW_TRIANGLE_STRIP] = GL_TRIANGLE_STRIP
};

void gpu_draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex) {
  GLenum mode = drawModes[state.pipeline->info.drawMode];
  glDrawArraysInstanced(mode, firstVertex, vertexCount, instanceCount);
}

void gpu_draw_indexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t baseVertex) {
  GLenum mode = drawModes[state.pipeline->info.drawMode];
  GLenum type = state.pipeline->info.indexStride == GPU_INDEX_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  GLvoid* offset = (GLvoid*) ((uint64_t) firstIndex * (type == GL_UNSIGNED_SHORT ? 2 : 4));
  glDrawElementsInstancedBaseVertex(mode, indexCount, type, offset, instanceCount, baseVertex);
}

void gpu_draw_indirect(gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  GLenum mode = drawModes[state.pipeline->info.drawMode];
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer->id);
  glMultiDrawArraysIndirect(mode, (GLvoid*) offset, drawCount, 0);
}

void gpu_draw_indirect_indexed(gpu_buffer* buffer, uint64_t offset, uint32_t drawCount) {
  GLenum mode = drawModes[state.pipeline->info.drawMode];
  GLenum type = state.pipeline->info.indexStride == GPU_INDEX_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer->id);
  glMultiDrawElementsIndirect(mode, type, (GLvoid*) offset, drawCount, 0);
}

void gpu_compute(gpu_shader* shader, uint32_t x, uint32_t y, uint32_t z) {
  glUseProgram(shader->id);
  glDispatchCompute(x, y, z);
}

void gpu_get_features(gpu_features* features) {
  features->anisotropy = true;
  features->dxt = true;
}

void gpu_get_limits(gpu_limits* limits) {
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limits->textureSize);
  glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &limits->framebufferSize[0]);
  glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &limits->framebufferSize[1]);
  glGetIntegerv(GL_MAX_FRAMEBUFFER_SAMPLES, &limits->framebufferSamples);
}

// Buffer

size_t gpu_sizeof_buffer(void) {
  return sizeof(gpu_buffer);
}

bool gpu_buffer_init(gpu_buffer* buffer, gpu_buffer_info* info) {
  if (info->usage & GPU_BUFFER_USAGE_VERTEX) buffer->target = GL_ARRAY_BUFFER;
  else if (info->usage & GPU_BUFFER_USAGE_INDEX) buffer->target = GL_ELEMENT_ARRAY_BUFFER;
  else if (info->usage & GPU_BUFFER_USAGE_UNIFORM) buffer->target = GL_UNIFORM_BUFFER;
  else if (info->usage & GPU_BUFFER_USAGE_COMPUTE) buffer->target = GL_SHADER_STORAGE_BUFFER;
  else if (info->usage & GPU_BUFFER_USAGE_COPY) buffer->target = GL_COPY_READ_BUFFER;
  else if (info->usage & GPU_BUFFER_USAGE_PASTE) buffer->target = GL_COPY_WRITE_BUFFER;
  else buffer->target = GL_TRANSFORM_FEEDBACK_BUFFER; // Haha no one uses this r-right
  glGenBuffers(1, &buffer->id);
  glBindBuffer(buffer->target, buffer->id);
  GLbitfield flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage(buffer->target, info->size, info->data, flags);
  buffer->data = glMapBufferRange(buffer->target, 0, info->size, flags);
  buffer->size = info->size;
  return true;
}

void gpu_buffer_destroy(gpu_buffer* buffer) {
  glDeleteBuffers(1, &buffer->id);
}

uint8_t* gpu_buffer_map(gpu_buffer* buffer, uint64_t offset, uint64_t size) {
  return buffer->data + offset;
}

void gpu_buffer_discard(gpu_buffer* buffer) {
  glInvalidateBufferData(buffer->id);
}

// Texture

static const GLenum textureTargets[] = {
  [GPU_TEXTURE_TYPE_2D] = GL_TEXTURE_2D,
  [GPU_TEXTURE_TYPE_3D] = GL_TEXTURE_3D,
  [GPU_TEXTURE_TYPE_CUBE] = GL_TEXTURE_CUBE_MAP,
  [GPU_TEXTURE_TYPE_ARRAY] = GL_TEXTURE_2D_ARRAY
};

static const struct { GLenum format, pixelFormat, pixelType; } textureFormats[] = {
  [GPU_TEXTURE_FORMAT_RGBA8] = { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },
  [GPU_TEXTURE_FORMAT_RGBA4] = { GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE },
  [GPU_TEXTURE_FORMAT_R16F] = { GL_R16F, GL_RED, GL_HALF_FLOAT },
  [GPU_TEXTURE_FORMAT_RG16F] = { GL_RG16F, GL_RG, GL_HALF_FLOAT },
  [GPU_TEXTURE_FORMAT_RGBA16F] = { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT },
  [GPU_TEXTURE_FORMAT_R32F] = { GL_R32F, GL_RED, GL_FLOAT },
  [GPU_TEXTURE_FORMAT_RG32F] = { GL_RG32F, GL_RG, GL_FLOAT },
  [GPU_TEXTURE_FORMAT_RGBA32F] = { GL_RGBA32F, GL_RGBA, GL_FLOAT },
  [GPU_TEXTURE_FORMAT_RGB10A2] = { GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV },
  [GPU_TEXTURE_FORMAT_RG11B10F] = { GL_R11F_G11F_B10F, GL_RGBA, GL_UNSIGNED_INT_10F_11F_11F_REV },
  [GPU_TEXTURE_FORMAT_D16] = { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT },
  [GPU_TEXTURE_FORMAT_D32F] = { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT },
  [GPU_TEXTURE_FORMAT_D24S8] = { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 }
};

size_t gpu_sizeof_texture(void) {
  return sizeof(gpu_texture);
}

bool gpu_texture_init(gpu_texture* texture, gpu_texture_info* info) {
  texture->target = textureTargets[info->type];
  texture->format = textureTormats[info->format].format;
  texture->pixelFormat = textureTormats[info->format].pixelFormat;
  texture->pixelType = textureTormats[info->format].pixelType;
  glGenTextures(1, &texture->id);
  glBindTexture(texture->target, texture->id);
  if (info->type == GPU_TEXTURE_TYPE_2D || info->type == GPU_TEXTURE_TYPE_CUBE) {
    glTexStorage2D(texture->target, info->mipmaps, texture->format, info->size[0], info->size[1]);
  } else {
    uint32_t depth = info->type == GPU_TEXTURE_TYPE_ARRAY ? info->layers : info->size[2];
    glTexStorage3D(texture->target, info->mipmaps, texture->format, info->size[0], info->size[1], depth);
  }
  return true;
}

bool gpu_texture_init_view(gpu_texture* texture, gpu_texture_view_info* info) {
  texture->target = textureTargets[info->type];
  texture->format = textureTormats[info->format].format;
  texture->pixelFormat = textureTormats[info->format].pixelFormat;
  texture->pixelType = textureTormats[info->format].pixelType;
  glGenTextures(1, &texture->id);
  glTextureView(texture->id, texture->target, info->source, texture->format, info->baseMipmap, info->mipmapCount, info->baseLayer, info->layerCount);
  return true;
}

void gpu_texture_destroy(gpu_texture* texture) {
  glDeleteTextures(1, &texture->id);
}

void gpu_texture_write(gpu_texture* texture, uint8_t* data, uint16_t offset[4], uint16_t extent[4], uint16_t mip) {
  glBindTexture(texture->target, texture->id);
  int x = offset[0], y = offset[1], z = offset[2], i = offset[3];
  int w = extent[0], h = extent[1], d = extent[2], n = extent[3];
  GLenum format = texture->pixelFormat;
  GLenum type = texture->pixelType;
  switch (texture->target) {
    case GL_TEXTURE_2D: glTexSubImage2D(GL_TEXTURE_2D, mip, x, y, w, h, format, type, data);
    case GL_TEXTURE_3D: glTexSubImage3D(GL_TEXTURE_3D, mip, x, y, z, w, h, d, format, type, data);
    case GL_TEXTURE_CUBE_MAP: glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + z, mip, x, y, w, h, format, type, data);
    case GL_TEXTURE_2D_ARRAY: glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip, x, y, i, w, h, n, format, type, data);
    default: break;
  }
}

// Canvas

size_t gpu_sizeof_canvas(void) {
  return sizeof(gpu_canvas);
}

bool gpu_canvas_init(gpu_canvas* canvas, gpu_canvas_info* info) {
  glGenFramebuffers(1, &canvas->id);
  glBindFramebuffer(GL_FRAMEBUFFER, canvas->id);
  GLsizei bufferCount = 0;
  GLenum buffers[4] = { GL_NONE };
  for (uint32_t i = 0; i < 4 && info->color[i].texture; i++, bufferCount++) {
    buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    gpu_color_attachment* attachment = &info->color[i];
    switch (attachment->texture->target) {
      case GL_TEXTURE_2D: glFramebufferTexture2D(GL_FRAMEBUFFER, buffers[i], GL_TEXTURE_2D, attachment->texture->id, attachment->level); break;
      case GL_TEXTURE_3D: glFramebufferTextureLayer(GL_FRAMEBUFFER, buffers[i], attachment->texture->id, attachment->level, attachment->layer); break;
      case GL_TEXTURE_CUBE_MAP: glFramebufferTexture2D(GL_FRAMEBUFFER, buffers[i], GL_TEXTURE_CUBE_MAP_POSITIVE_X + attachment->layer, attachment->texture->id, attachment->level); break;
      case GL_TEXTURE_2D_ARRAY: glFramebufferTextureLayer(GL_FRAMEBUFFER, buffers[i], attachment->texture->id, attachment->level, attachment->layer); break;
    }
  }
  glDrawBuffers(bufferCount, buffers);
  if (info->depth.texture) {
    GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT; // Depends on texture format
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, info->depth.texture->id, 0);
  }
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void gpu_canvas_destroy(gpu_canvas* canvas) {
  glDeleteFramebuffers(1, &canvas->id);
}

// Shader

bool gpu_shader_init(gpu_shader* shader, gpu_shader_info* info) {
  shader->id = glCreateProgram();
  return true;
}

void gpu_shader_destroy(gpu_shader* shader) {
  glDeleteProgram(shader->id);
}

// Pipeline

bool gpu_pipeline_init(gpu_pipeline* pipeline, gpu_pipeline_info* info) {
  pipeline->info = *info;
  return true;
}

void gpu_pipeline_destroy(gpu_pipeline* pipeline) {
  //
}
