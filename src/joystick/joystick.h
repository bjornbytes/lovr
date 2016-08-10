#include "../osvr.h"
#include "../vendor/vec/vec.h"

#ifndef LOVR_JOYSTICK_TYPES
#define LOVR_JOYSTICK_TYPES
typedef struct {
  int isTracked;
  int glfwIndex;
  OSVR_ClientInterface osvrTrackerInterface;
  interface_vec_t osvrAxisInterfaces;
  interface_vec_t osvrButtonInterfaces;
} Joystick;
typedef vec_t(Joystick*) joystick_vec_t;
#endif

void lovrJoystickDestroy(Joystick* joystick);
void lovrJoystickGetAngularAcceleration(Joystick* joystick, float* w, float* x, float* y, float* z);
void lovrJoystickGetAngularVelocity(Joystick* joystick, float* w, float* x, float* y, float* z);
void lovrJoystickGetAxes(Joystick* joystick, vec_float_t* result);
float lovrJoystickGetAxis(Joystick* joystick, int index);
int lovrJoystickGetAxisCount(Joystick* joystick);
int lovrJoystickGetButtonCount(Joystick* joystick);
void lovrJoystickGetLinearAcceleration(Joystick* joystick, float* x, float* y, float* z);
void lovrJoystickGetLinearVelocity(Joystick* joystick, float* x, float* y, float* z);
const char* lovrJoystickGetName(Joystick* joystick);
void lovrJoystickGetOrientation(Joystick* joystick, float* w, float* x, float* y, float* z);
void lovrJoystickGetPosition(Joystick* joystick, float* x, float* y, float* z);
int lovrJoystickIsDown(Joystick* joystick, int index);
int lovrJoystickIsTracked(Joystick* joystick);
