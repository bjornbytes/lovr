#pragma once

enum {
  PROFILE_SIMPLE,
  PROFILE_VIVE,
  PROFILE_TOUCH,
  PROFILE_GO,
  PROFILE_INDEX,
  MAX_PROFILES
};

enum {
  ACTION_HAND_POSE,
  ACTION_TRIGGER_DOWN,
  ACTION_TRIGGER_TOUCH,
  ACTION_TRIGGER_AXIS,
  ACTION_TRACKPAD_DOWN,
  ACTION_TRACKPAD_TOUCH,
  ACTION_TRACKPAD_X,
  ACTION_TRACKPAD_Y,
  ACTION_THUMBSTICK_DOWN,
  ACTION_THUMBSTICK_TOUCH,
  ACTION_THUMBSTICK_X,
  ACTION_THUMBSTICK_Y,
  ACTION_MENU_DOWN,
  ACTION_MENU_TOUCH,
  ACTION_GRIP_DOWN,
  ACTION_GRIP_TOUCH,
  ACTION_GRIP_AXIS,
  ACTION_VIBRATE,
  MAX_ACTIONS
};

static const char* interactionProfiles[] = {
  [PROFILE_SIMPLE] = "/interaction_profiles/khr/simple_controller",
  [PROFILE_VIVE] = "/interaction_profiles/htc/vive_controller",
  [PROFILE_TOUCH] = "/interaction_profiles/oculus/touch_controller",
  [PROFILE_GO] = "/interaction_profiles/oculus/go_controller",
  [PROFILE_INDEX] = "/interaction_profiles/valve/index_controller"
};

#define action(id, name, type) { XR_TYPE_ACTION_CREATE_INFO, NULL, id, type, 2, NULL, name }
static XrActionCreateInfo actionCreateInfo[] = {
  [ACTION_HAND_POSE] = action("hand_pose", "Hand Pose", XR_ACTION_TYPE_POSE_INPUT),
  [ACTION_TRIGGER_DOWN] = action("trigger_down", "Trigger Down", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_TRIGGER_TOUCH] = action("trigger_touch", "Trigger Touch", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_TRIGGER_AXIS] = action("trigger_axis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT),
  [ACTION_TRACKPAD_DOWN] = action("trackpad_down", "Trackpad Down", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_TRACKPAD_TOUCH] = action("trackpad_touch", "Trackpad Touch", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_TRACKPAD_X] = action("trackpad_x", "Trackpad X", XR_ACTION_TYPE_FLOAT_INPUT),
  [ACTION_TRACKPAD_Y] = action("trackpad_y", "Trackpad Y", XR_ACTION_TYPE_FLOAT_INPUT),
  [ACTION_THUMBSTICK_DOWN] = action("thumbstick_down", "Thumbstick Down", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_THUMBSTICK_TOUCH] = action("thumbstick_touch", "Thumbstick Touch", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_THUMBSTICK_X] = action("thumbstick_x", "Thumbstick X", XR_ACTION_TYPE_FLOAT_INPUT),
  [ACTION_THUMBSTICK_Y] = action("thumbstick_y", "Thumbstick Y", XR_ACTION_TYPE_FLOAT_INPUT),
  [ACTION_MENU_DOWN] = action("menu_down", "Menu Down", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_MENU_TOUCH] = action("menu_touch", "Menu Touch", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_GRIP_DOWN] = action("grip_down", "Grip Down", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_GRIP_TOUCH] = action("grip_touch", "Grip Touch", XR_ACTION_TYPE_BOOLEAN_INPUT),
  [ACTION_GRIP_AXIS] = action("grip_axis", "Grip Axis", XR_ACTION_TYPE_FLOAT_INPUT),
  [ACTION_VIBRATE] = action("vibrate", "Vibrate", XR_ACTION_TYPE_VIBRATION_OUTPUT),
};
#undef action

static const char* bindings[MAX_PROFILES][MAX_ACTIONS][2] = {
  [PROFILE_SIMPLE] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/select/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/select/click",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  },

  [PROFILE_VIVE] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_DOWN][0] = "/user/hand/left/input/trackpad/click",
    [ACTION_TRACKPAD_DOWN][1] = "/user/hand/right/input/trackpad/click",
    [ACTION_TRACKPAD_TOUCH][0] = "/user/hand/left/input/trackpad/touch",
    [ACTION_TRACKPAD_TOUCH][1] = "/user/hand/right/input/trackpad/touch",
    [ACTION_TRACKPAD_X][0] = "/user/hand/left/input/trackpad/x",
    [ACTION_TRACKPAD_X][1] = "/user/hand/right/input/trackpad/x",
    [ACTION_TRACKPAD_Y][0] = "/user/hand/left/input/trackpad/y",
    [ACTION_TRACKPAD_Y][1] = "/user/hand/right/input/trackpad/y",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/menu/click",
    [ACTION_GRIP_DOWN][0] = "/user/hand/left/input/squeeze/click",
    [ACTION_GRIP_DOWN][1] = "/user/hand/right/input/squeeze/click",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  },

  [PROFILE_TOUCH] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRIGGER_TOUCH][0] = "/user/hand/left/input/trigger/touch",
    [ACTION_TRIGGER_TOUCH][1] = "/user/hand/right/input/trigger/touch",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_THUMBSTICK_DOWN][0] = "/user/hand/left/input/thumbstick/click",
    [ACTION_THUMBSTICK_DOWN][1] = "/user/hand/right/input/thumbstick/click",
    [ACTION_THUMBSTICK_TOUCH][0] = "/user/hand/left/input/thumbstick/touch",
    [ACTION_THUMBSTICK_TOUCH][1] = "/user/hand/right/input/thumbstick/touch",
    [ACTION_THUMBSTICK_X][0] = "/user/hand/left/input/thumbstick/x",
    [ACTION_THUMBSTICK_X][1] = "/user/hand/right/input/thumbstick/x",
    [ACTION_THUMBSTICK_Y][0] = "/user/hand/left/input/thumbstick/y",
    [ACTION_THUMBSTICK_Y][1] = "/user/hand/right/input/thumbstick/y",
    [ACTION_MENU_DOWN][0] = "/user/hand/left/input/menu/click",
    [ACTION_MENU_DOWN][1] = "/user/hand/right/input/system/click",
    [ACTION_GRIP_DOWN][0] = "/user/hand/left/input/squeeze/value",
    [ACTION_GRIP_DOWN][1] = "/user/hand/right/input/squeeze/value",
    [ACTION_GRIP_AXIS][0] = "/user/hand/left/input/squeeze/value",
    [ACTION_GRIP_AXIS][1] = "/user/hand/right/input/squeeze/value",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  },

  [PROFILE_GO] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRACKPAD_DOWN][0] = "/user/hand/left/input/trackpad/click",
    [ACTION_TRACKPAD_DOWN][1] = "/user/hand/right/input/trackpad/click",
    [ACTION_TRACKPAD_TOUCH][0] = "/user/hand/left/input/trackpad/touch",
    [ACTION_TRACKPAD_TOUCH][1] = "/user/hand/right/input/trackpad/touch",
    [ACTION_TRACKPAD_X][0] = "/user/hand/left/input/trackpad/x",
    [ACTION_TRACKPAD_X][1] = "/user/hand/right/input/trackpad/x",
    [ACTION_TRACKPAD_Y][0] = "/user/hand/left/input/trackpad/y",
    [ACTION_TRACKPAD_Y][1] = "/user/hand/right/input/trackpad/y"
  },

  [PROFILE_INDEX] = {
    [ACTION_HAND_POSE][0] = "/user/hand/left/input/grip/pose",
    [ACTION_HAND_POSE][1] = "/user/hand/right/input/grip/pose",
    [ACTION_TRIGGER_DOWN][0] = "/user/hand/left/input/trigger/click",
    [ACTION_TRIGGER_DOWN][1] = "/user/hand/right/input/trigger/click",
    [ACTION_TRIGGER_TOUCH][0] = "/user/hand/left/input/trigger/touch",
    [ACTION_TRIGGER_TOUCH][1] = "/user/hand/right/input/trigger/touch",
    [ACTION_TRIGGER_AXIS][0] = "/user/hand/left/input/trigger/value",
    [ACTION_TRIGGER_AXIS][1] = "/user/hand/right/input/trigger/value",
    [ACTION_TRACKPAD_DOWN][0] = "/user/hand/left/input/trackpad/force",
    [ACTION_TRACKPAD_DOWN][1] = "/user/hand/right/input/trackpad/force",
    [ACTION_TRACKPAD_TOUCH][0] = "/user/hand/left/input/trackpad/touch",
    [ACTION_TRACKPAD_TOUCH][1] = "/user/hand/right/input/trackpad/touch",
    [ACTION_TRACKPAD_X][0] = "/user/hand/left/input/trackpad/x",
    [ACTION_TRACKPAD_X][1] = "/user/hand/right/input/trackpad/x",
    [ACTION_TRACKPAD_Y][0] = "/user/hand/left/input/trackpad/y",
    [ACTION_TRACKPAD_Y][1] = "/user/hand/right/input/trackpad/y",
    [ACTION_THUMBSTICK_DOWN][0] = "/user/hand/left/input/thumbstick/click",
    [ACTION_THUMBSTICK_DOWN][1] = "/user/hand/right/input/thumbstick/click",
    [ACTION_THUMBSTICK_TOUCH][0] = "/user/hand/left/input/thumbstick/touch",
    [ACTION_THUMBSTICK_TOUCH][1] = "/user/hand/right/input/thumbstick/touch",
    [ACTION_THUMBSTICK_X][0] = "/user/hand/left/input/thumbstick/x",
    [ACTION_THUMBSTICK_X][1] = "/user/hand/right/input/thumbstick/x",
    [ACTION_THUMBSTICK_Y][0] = "/user/hand/left/input/thumbstick/y",
    [ACTION_THUMBSTICK_Y][1] = "/user/hand/right/input/thumbstick/y",
    [ACTION_GRIP_AXIS][0] = "/user/hand/left/input/squeeze/value",
    [ACTION_GRIP_AXIS][1] = "/user/hand/right/input/squeeze/value",
    [ACTION_VIBRATE][0] = "/user/hand/left/output/haptic",
    [ACTION_VIBRATE][1] = "/user/hand/right/output/haptic"
  }
};
