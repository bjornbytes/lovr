#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/DisplayC.h>
#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include "vendor/vec/vec.h"

#ifndef LOVR_OSVR_TYPES
#define LOVR_OSVR_TYPES
typedef vec_t(OSVR_ClientInterface*) interface_vec_t;
#endif

OSVR_ClientContext ctx;

void initOsvr();
