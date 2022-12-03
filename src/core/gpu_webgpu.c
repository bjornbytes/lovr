#include "gpu.h"
#include <webgpu/webgpu.h>
#include <emscripten/html5_webgpu.h>
#include <string.h>

static struct {
  WGPUDevice device;
} state;

bool gpu_init(gpu_config* config) {
  state.device = emscripten_webgpu_get_device();
  return !!state.device;
}

void gpu_destroy(void) {
  if (state.device) wgpuDeviceDestroy(state.device);
  memset(&state, 0, sizeof(state));
}
