#include "joystick.h"
#include "../glfw.h"
#include "../vendor/vec/vec.h"

void lovrJoysticksInit();
void lovrJoysticksOnJoystickChanged();
int lovrJoysticksGetJoystickCount();
void lovrJoysticksGetJoysticks(joystick_vec_t* joysticks);
