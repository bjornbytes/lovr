#include <stdbool.h>

#define LOVR_VERSION_MAJOR 0
#define LOVR_VERSION_MINOR 9
#define LOVR_VERSION_PATCH 1
#define LOVR_VERSION_ALIAS "Fluffy Cuttlefish"

void lovrDestroy();
bool lovrRun(int argc, char** argv, int* status);
void lovrQuit(int status);
const char* lovrGetOS();
void lovrGetVersion(int* major, int* minor, int* patch);
