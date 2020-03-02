// Functions on the Lovr side of the wall, called from the Oculus side of the wall.
#pragma once

#include "../../core/util.h"
#include "data/modelData.h"
#include <stdbool.h>

// What's going on here:
// At the moment, it's not easy to statically link LovrApp_NativeActivity with lovr.
// In order to prevent NativeActivity and Lovr from having to start including each other's
// headers and maybe run into linking problems, all communication between the two
// happens through the functions and data structures in this file.

typedef struct {
  int width;
  int height;
} BridgeLovrDimensions;

typedef struct {
  float x;
  float y;
  float z;
  float q[4];
} BridgeLovrPose;

typedef struct {
  float x;
  float y;
  float z;
  float ax;
  float ay;
  float az;
} BridgeLovrAngularVector;

typedef struct {
  BridgeLovrAngularVector velocity;
  BridgeLovrAngularVector acceleration;
} BridgeLovrMovement;

typedef struct {
  float x;
  float y;
} BridgeLovrTrackpad;

// Bit identical with VrApi_Input.h ovrButton
typedef enum
{
  BRIDGE_LOVR_BUTTON_NONE = 0,

  BRIDGE_LOVR_BUTTON_GOSHOULDER = 0x00000001, // "Set for trigger pulled on the Gear VR and Go Controllers"
  BRIDGE_LOVR_BUTTON_A          = 0x00000001, // A
  BRIDGE_LOVR_BUTTON_B          = 0x00000002, // B
  BRIDGE_LOVR_BUTTON_X          = 0x00000100, // X
  BRIDGE_LOVR_BUTTON_Y          = 0x00000200, // Y
  BRIDGE_LOVR_BUTTON_TOUCHPAD   = 0x00100000, // "Set for touchpad click on the Gear VR and Go Controllers"
  BRIDGE_LOVR_BUTTON_MENU       = 0x00100000, // On Go this is touchpad, on the Quest it is the menu button
  BRIDGE_LOVR_BUTTON_GOMENU     = 0x00200000, // "Back button on the headset or Gear VR Controller (only set when a short press comes up)"
  BRIDGE_LOVR_BUTTON_GRIP       = 0x04000000, // Quest grip
  BRIDGE_LOVR_BUTTON_SHOULDER   = 0x20000000, // Quest shoulders
  BRIDGE_LOVR_BUTTON_JOYSTICK   = 0x80000000, // Quest joystick click-down
} BridgeLovrButton;

// Bit identical with VrApi_Input.h ovrButton
typedef enum
{
  BRIDGE_LOVR_TOUCH_NONE = 0,

  BRIDGE_LOVR_TOUCH_A             = 0x00000001, // "The A button has a finger resting on it."
  BRIDGE_LOVR_TOUCH_B             = 0x00000002, // "The B button has a finger resting on it."
  BRIDGE_LOVR_TOUCH_X             = 0x00000004, // "The X button has a finger resting on it."
  BRIDGE_LOVR_TOUCH_Y             = 0x00000008, // "The Y button has a finger resting on it."
  BRIDGE_LOVR_TOUCH_TOUCHPAD      = 0x00000010, // "The TrackPad has a finger resting on it."
  BRIDGE_LOVR_TOUCH_JOYSTICK      = 0x00000020, // "The Joystick has a finger resting on it."
  BRIDGE_LOVR_TOUCH_TRIGGER       = 0x00000040, // "The Index Trigger has a finger resting on it."
  BRIDGE_LOVR_TOUCH_FACE_ANTI     = 0x00000100, // "None of A, B, X, Y, or Joystick has a finger/thumb in proximity to it"
  BRIDGE_LOVR_TOUCH_TRIGGER_ANTI  = 0x00000200, // "The finger is sufficiently far away from the trigger to not be considered in proximity to it."
} BridgeLovrTouch;

// Bit identical with VrApi_Input.h ovrControllerCapabilties
typedef enum {
  BRIDGE_LOVR_HAND_LEFT = 0x00000004,
  BRIDGE_LOVR_HAND_RIGHT = 0x00000008,
} BridgeLovrHand;

// Values identical with headset.h HeadsetType
typedef enum
{
  BRIDGE_LOVR_DEVICE_UNKNOWN,
  BRIDGE_LOVR_DEVICE_GEAR = 3,
  BRIDGE_LOVR_DEVICE_GO = 4,
  BRIDGE_LOVR_DEVICE_QUEST = 5,
} BridgeLovrDevice;

typedef struct {
  bool handset;
  BridgeLovrHand hand;
  BridgeLovrPose pose;
  BridgeLovrMovement movement;
  BridgeLovrTrackpad trackpad;
  float trigger, grip;
  BridgeLovrButton buttonDown;
  BridgeLovrTouch  buttonTouch;
  BridgeLovrButton buttonChanged;
} BridgeLovrController;

#define BRIDGE_LOVR_CONTROLLERMAX 3

// Data passed from Lovr_NativeActivity to BridgeLovr at update time
typedef struct {
  double displayTime; // Projected

  BridgeLovrPose lastHeadPose;
  BridgeLovrMovement lastHeadMovement;
  float eyeViewMatrix[2][16];
  float projectionMatrix[2][16];

  float boundsWidth;
  float boundsDepth;

  int controllerCount;
  BridgeLovrController controllers[BRIDGE_LOVR_CONTROLLERMAX];
} BridgeLovrUpdateData;

typedef bool BridgeLovrVibrateFunction(int controller, float strength, float duration);

// Data passed from Lovr_NativeActivity to BridgeLovr at init time
typedef struct {
  const char *writablePath;
  const char *apkPath;
  BridgeLovrDimensions suggestedEyeTexture;
  float displayFrequency;
  double zeroDisplayTime;
  BridgeLovrDevice deviceType;
  BridgeLovrVibrateFunction* vibrateFunction; // Returns true on success
  unsigned int textureHandles[4];
  unsigned int textureCount;
  ModelData* handModels[2];
} BridgeLovrInitData;

LOVR_EXPORT void bridgeLovrInit(BridgeLovrInitData *initData);

LOVR_EXPORT void bridgeLovrUpdate(BridgeLovrUpdateData *updateData);

typedef struct {
  int eye;
  int framebuffer;
  unsigned int textureIndex;
} BridgeLovrDrawData;

LOVR_EXPORT void bridgeLovrDraw(BridgeLovrDrawData *drawData);

LOVR_EXPORT void bridgeLovrPaused(bool paused);

LOVR_EXPORT void bridgeLovrClose();
