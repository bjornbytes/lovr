#include "graphics/shader.h"

#pragma once

#define MAX_SHADER_BLOCK_UNIFORMS 32

typedef struct ShaderBlock ShaderBlock;

ShaderBlock* lovrShaderBlockCreate(Uniform uniforms[MAX_SHADER_BLOCK_UNIFORMS], int uniformCount);
void lovrShaderBlockDestroy(void* ref);
