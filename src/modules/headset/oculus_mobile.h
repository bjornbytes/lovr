#pragma once

extern char *lovrOculusMobileWritablePath;

#ifdef LOVR_ENABLE_OCULUS_AUDIO
void lovrOculusMobileRecreatePose(void* ovrposestatef);
#endif
