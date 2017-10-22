#include "graphics/shader.h"
#include "graphics/shaders.h"
#include "graphics/graphics.h"
#include "math/mat4.h"
#include <stdio.h>
#include <stdlib.h>

static UniformType getUniformType(GLenum type, const char* debug) {
  switch (type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
      return UNIFORM_FLOAT;

    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
      return UNIFORM_INT;

    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
      return UNIFORM_MATRIX;

    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
      return UNIFORM_SAMPLER;

    default:
      lovrThrow("Unsupported type for uniform '%s'", debug);
      return UNIFORM_FLOAT;
  }
}

static int getUniformComponents(GLenum type) {
  switch (type) {
    case GL_FLOAT:
    case GL_INT:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
      return 1;

    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_FLOAT_MAT2:
      return 2;

    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_FLOAT_MAT3:
      return 3;

    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
    case GL_FLOAT_MAT4:
      return 4;

    default:
      return 1;
  }
}

static GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, (const GLchar**)&source, NULL);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if (!isShaderCompiled) {
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char* log = malloc(logLength);
    glGetShaderInfoLog(shader, logLength, &logLength, log);
    lovrThrow("Could not compile shader %s", log);
  }

  return shader;
}

static GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader) {
  GLuint shader = glCreateProgram();

  if (vertexShader) {
    glAttachShader(shader, vertexShader);
  }

  if (fragmentShader) {
    glAttachShader(shader, fragmentShader);
  }

  glBindAttribLocation(shader, LOVR_SHADER_POSITION, "lovrPosition");
  glBindAttribLocation(shader, LOVR_SHADER_NORMAL, "lovrNormal");
  glBindAttribLocation(shader, LOVR_SHADER_TEX_COORD, "lovrTexCoord");

  glLinkProgram(shader);

  int isShaderLinked;
  glGetProgramiv(shader, GL_LINK_STATUS, (int*)&isShaderLinked);
  if (!isShaderLinked) {
    int logLength;
    glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char* log = malloc(logLength);
    glGetProgramInfoLog(shader, logLength, &logLength, log);
    lovrThrow("Could not link shader %s", log);
  }

  glDetachShader(shader, vertexShader);
  glDeleteShader(vertexShader);
  glDetachShader(shader, fragmentShader);
  glDeleteShader(fragmentShader);

  return shader;
}

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource) {
  Shader* shader = lovrAlloc(sizeof(Shader), lovrShaderDestroy);
  if (!shader) return NULL;

  // Vertex
  vertexSource = vertexSource == NULL ? lovrDefaultVertexShader : vertexSource;
  char fullVertexSource[4096];
  snprintf(fullVertexSource, sizeof(fullVertexSource), "%s\n%s\n%s", lovrShaderVertexPrefix, vertexSource, lovrShaderVertexSuffix);
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, fullVertexSource);

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrDefaultFragmentShader : fragmentSource;
  char fullFragmentSource[4096];
  snprintf(fullFragmentSource, sizeof(fullFragmentSource), "%s\n%s\n%s", lovrShaderFragmentPrefix, fragmentSource, lovrShaderFragmentSuffix);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fullFragmentSource);

  // Link
  GLuint id = linkShaders(vertexShader, fragmentShader);
  shader->id = id;

  lovrGraphicsBindProgram(id);

  // Uniform introspection
  GLint uniformCount;
  int textureSlot = 0;
  GLsizei bufferSize = LOVR_MAX_UNIFORM_LENGTH / sizeof(GLchar);
  map_init(&shader->uniforms);
  glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &uniformCount);
  for (int i = 0; i < uniformCount; i++) {
    Uniform uniform;
    glGetActiveUniform(id, i, bufferSize, NULL, &uniform.count, &uniform.glType, uniform.name);

    char* subscript = strchr(uniform.name, '[');
    if (subscript) {
      *subscript = '\0';
    }

    uniform.index = i;
    uniform.location = glGetUniformLocation(id, uniform.name);
    uniform.type = getUniformType(uniform.glType, uniform.name);
    uniform.components = getUniformComponents(uniform.glType);
    uniform.baseTextureSlot = (uniform.type == UNIFORM_SAMPLER) ? textureSlot : -1;

    switch (uniform.type) {
      case UNIFORM_FLOAT:
        uniform.size = uniform.components * uniform.count * sizeof(float);
        uniform.value.data = malloc(uniform.size);
        break;

      case UNIFORM_INT:
        uniform.size = uniform.components * uniform.count * sizeof(int);
        uniform.value.data = malloc(uniform.size);
        break;

      case UNIFORM_MATRIX:
        uniform.size = uniform.components * uniform.components * uniform.count * sizeof(int);
        uniform.value.data = malloc(uniform.size);
        break;

      case UNIFORM_SAMPLER:
        uniform.size = uniform.components * uniform.count * MAX(sizeof(Texture*), sizeof(int));
        uniform.value.data = malloc(uniform.size);

        // Use the value for ints to bind texture slots, but use the value for textures afterwards.
        for (int i = 0; i < uniform.count; i++) {
          uniform.value.ints[i] = uniform.baseTextureSlot + i;
        }
        glUniform1iv(uniform.location, uniform.count, uniform.value.ints);
        break;
    }

    memset(uniform.value.data, 0, uniform.size);

    size_t offset = 0;
    for (int j = 0; j < uniform.count; j++) {
      int location = uniform.location;

      if (uniform.count > 1) {
        char name[LOVR_MAX_UNIFORM_LENGTH];
        snprintf(name, LOVR_MAX_UNIFORM_LENGTH, "%s[%d]", uniform.name, j);
        location = glGetUniformLocation(id, name);
      }

      switch (uniform.type) {
        case UNIFORM_FLOAT:
          glGetUniformfv(id, location, &uniform.value.floats[offset]);
          offset += uniform.components;
          break;

        case UNIFORM_INT:
          glGetUniformiv(id, location, &uniform.value.ints[offset]);
          offset += uniform.components;
          break;

        case UNIFORM_MATRIX:
          glGetUniformfv(id, location, &uniform.value.floats[offset]);
          offset += uniform.components * uniform.components;
          break;

        default:
          break;
      }
    }

    map_set(&shader->uniforms, uniform.name, uniform);
    textureSlot += (uniform.type == UNIFORM_SAMPLER) ? uniform.count : 0;
  }

  return shader;
}

Shader* lovrShaderCreateDefault(DefaultShader type) {
  switch (type) {
    case SHADER_DEFAULT: return lovrShaderCreate(NULL, NULL);
    case SHADER_SKYBOX: return lovrShaderCreate(lovrSkyboxVertexShader, lovrSkyboxFragmentShader); break;
    case SHADER_FONT: return lovrShaderCreate(NULL, lovrFontFragmentShader);
    case SHADER_FULLSCREEN: return lovrShaderCreate(lovrNoopVertexShader, NULL);
    default: lovrThrow("Unknown default shader type");
  }
}

void lovrShaderDestroy(const Ref* ref) {
  Shader* shader = containerof(ref, Shader);
  glDeleteProgram(shader->id);
  map_deinit(&shader->uniforms);
  free(shader);
}

void lovrShaderBind(Shader* shader) {
  map_iter_t iter = map_iter(&shader->uniforms);
  const char* key;
  while ((key = map_next(&shader->uniforms, &iter)) != NULL) {
    Uniform* uniform = map_get(&shader->uniforms, key);

    if (uniform->type != UNIFORM_SAMPLER && !uniform->dirty) {
      continue;
    }

    uniform->dirty = 0;
    int count = uniform->count;
    void* data = uniform->value.data;

    switch (uniform->type) {
      case UNIFORM_FLOAT:
        switch (uniform->components) {
          case 1: glUniform1fv(uniform->location, count, data); break;
          case 2: glUniform2fv(uniform->location, count, data); break;
          case 3: glUniform3fv(uniform->location, count, data); break;
          case 4: glUniform4fv(uniform->location, count, data); break;
        }
        break;

      case UNIFORM_INT:
        switch (uniform->components) {
          case 1: glUniform1iv(uniform->location, count, data); break;
          case 2: glUniform2iv(uniform->location, count, data); break;
          case 3: glUniform3iv(uniform->location, count, data); break;
          case 4: glUniform4iv(uniform->location, count, data); break;
        }
        break;

      case UNIFORM_MATRIX:
        switch (uniform->components) {
          case 2: glUniformMatrix2fv(uniform->location, count, GL_FALSE, data); break;
          case 3: glUniformMatrix3fv(uniform->location, count, GL_FALSE, data); break;
          case 4: glUniformMatrix4fv(uniform->location, count, GL_FALSE, data); break;
        }
        break;

      case UNIFORM_SAMPLER:
        for (int i = 0; i < count; i++) {
          TextureType type = uniform->glType == GL_SAMPLER_2D ? TEXTURE_2D : TEXTURE_CUBE;
          lovrGraphicsBindTexture(uniform->value.textures[i], type, uniform->baseTextureSlot + i);
        }
        break;
    }
  }
}

int lovrShaderGetAttributeId(Shader* shader, const char* name) {
  if (!shader) {
    return -1;
  }

  return glGetAttribLocation(shader->id, name);
}

Uniform* lovrShaderGetUniform(Shader* shader, const char* name) {
  return map_get(&shader->uniforms, name);
}

static void lovrShaderSetUniform(Shader* shader, const char* name, UniformType type, void* data, int count, size_t size, const char* debug) {
  Uniform* uniform = map_get(&shader->uniforms, name);
  if (!uniform) {
    return;
  }

  lovrAssert(uniform->type == type, "Unable to send %ss to uniform %s", debug, uniform->name);
  lovrAssert(count * size == uniform->size, "Expected %d %ss for uniform %s, got %d", uniform->count, debug, uniform->name, count);

  if (!uniform->dirty && !memcmp(uniform->value.data, data, count * size)) {
    return;
  }

  memcpy(uniform->value.data, data, count * size);
  uniform->dirty = 1;
}

void lovrShaderSetFloat(Shader* shader, const char* name, float* data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_FLOAT, data, count, sizeof(float), "float");
}

void lovrShaderSetInt(Shader* shader, const char* name, int* data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_INT, data, count, sizeof(int), "int");
}

void lovrShaderSetMatrix(Shader* shader, const char* name, float* data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_MATRIX, data, count, sizeof(float), "float");
}

void lovrShaderSetTexture(Shader* shader, const char* name, Texture** data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_SAMPLER, data, count, sizeof(Texture*), "texture");
}
