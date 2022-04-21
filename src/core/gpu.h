#include <stdbool.h>

typedef struct {
  bool debug;
  void (*callback)(void* userdata, const char* message, bool error);
  void* userdata;
} gpu_config;

bool gpu_init(gpu_config* config);
void gpu_destroy(void);
