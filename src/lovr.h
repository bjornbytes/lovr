#include <stdbool.h>

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 11
#define LOVR_VERSION_PATCH 0
#define LOVR_VERSION_ALIAS "Ginormous Giraffe"

void lovrDestroy();
bool lovrRun(int argc, char** argv, int* status);
void lovrQuit(int status);
const char* lovrGetOS();
void lovrGetVersion(int* major, int* minor, int* patch);
