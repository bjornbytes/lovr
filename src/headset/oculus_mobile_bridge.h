// Functions on the Lovr side of the wall, called from the Oculus side of the wall.
#pragma once

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
	float x; float y; float z; float q[4];
} BridgeLovrPose;

typedef struct {
	float x; float y; float z; float ax; float ay; float az;
} BridgeLovrVel;

typedef struct {
	float x; float y;
} BridgeLovrTrackpad;

// Bit identical with VrApi_Input.h ovrButton
typedef enum
{
	BridgeLovrButtonNone = 0,

	BridgeLovrButtonShoulder = 0x00000001,	// "Set for trigger pulled on the Gear VR and Go Controllers"
	BridgeLovrButtonTouchpad = 0x00100000,	// "Set for touchpad click on the Gear VR and Go Controllers"
	BridgeLovrButtonMenu     = 0x00200000,	// "Back button on the headset or Gear VR Controller (only set when a short press comes up)"
} BridgeLovrButton;

// Data passed from Lovr_NativeActivity to BridgeLovr at update time
typedef struct {
	double displayTime; // Projected

	BridgeLovrPose lastHeadPose;
	BridgeLovrVel lastHeadVelocity;
	float eyeViewMatrix[2][16];
	float projectionMatrix[2][16];

	// TODO: Controller object
	bool goPresent;
	BridgeLovrPose goPose;
	BridgeLovrVel goVelocity;
	BridgeLovrTrackpad goTrackpad;
	bool goTrackpadTouch;
	BridgeLovrButton goButtonDown;
	BridgeLovrButton goButtonTouch;
} BridgeLovrUpdateData;

// Data passed from Lovr_NativeActivity to BridgeLovr at init time
typedef struct {
	const char *writablePath;
	const char *apkPath;
	BridgeLovrDimensions suggestedEyeTexture;
	double zeroDisplayTime;
} BridgeLovrInitData;

void bridgeLovrInit(BridgeLovrInitData *initData);

void bridgeLovrUpdate(BridgeLovrUpdateData *updateData);

typedef struct {
	int eye;
	int framebuffer;
} BridgeLovrDrawData;

void bridgeLovrDraw(BridgeLovrDrawData *drawData);

void bridgeLovrPaused(bool paused);

void bridgeLovrClose();

