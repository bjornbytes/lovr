#include <stdbool.h>

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 10
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Hangry Goose"

void lovrDestroy();
bool lovrRun(int argc, char** argv, int* status);
void lovrQuit(int status);
const char* lovrGetOS();
void lovrGetVersion(int* major, int* minor, int* patch);
