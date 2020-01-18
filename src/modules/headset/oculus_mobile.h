#pragma once

extern char *lovrOculusMobileWritablePath;

typedef struct {
  bool live;
  float confidence;
  float handScale;
  BridgeLovrPose pose;
  BridgeLovrStringList bones;
  BridgeLovrPoseList handPoses;
  BridgeLovrFloatList fingerConfidence;
} LovrOculusMobileHands;

LovrOculusMobileHands lovrOculusMobileHands[2];
