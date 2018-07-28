#include "graphics/shader.h"

#pragma once

#define MAX_SHADER_BLOCK_UNIFORMS 32

typedef struct ShaderBlock ShaderBlock;

ShaderBlock* lovrShaderBlockCreate(vec_uniform_t* uniforms);
void lovrShaderBlockDestroy(void* ref);
