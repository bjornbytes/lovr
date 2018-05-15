#include "lovr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "--version")) {
    printf("LOVR %d.%d.%d (%s)\n", LOVR_VERSION_MAJOR, LOVR_VERSION_MINOR, LOVR_VERSION_PATCH, LOVR_VERSION_ALIAS);
    exit(0);
  }

  int status;
  while (1) {
    if (!lovrRun(argc, argv, &status)) {
      return status;
    }
  }
}
