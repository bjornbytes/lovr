// Intended to be included by fake.c

// FIXME: Detect OS X, not just Darwin
#ifdef __APPLE__

#include <3DconnexionClient/ConnexionClient.h>
#include <3DconnexionClient/ConnexionClientAPI.h>
#include <limits.h>

// OSErr and natural_t are old Carbon types. Including carbon.h however has many downsides including namespace collisions.
typedef unsigned int natural_t; // This is incorrect, but it's what the 3DConnexion API expects.
typedef short OSErr;

uint16_t connexionClient;

enum {
  SI_TX = 0, SI_TY, SI_TZ, SI_RX, SI_RY, SI_RZ, SI_MAX
};

static void spacemouseNullDeviceAddedHandler  (unsigned int connection) {}
static void spacemouseNullDeviceRemovedHandler  (unsigned int connection) {}
static void spacemouseNullMessageHandler    (unsigned int connection, natural_t messageType, void *messageArgument);

static float deadZone = 0.05;

static float spacemouseToFloat(long v) {
  float scaled = (float)v * 32 / (-SHRT_MIN); // Normalize [-1024..1024] to [-1..1], which is the range on mac
  if (fabs(scaled) < deadZone)
    return 0;
  return scaled;
}

static struct {
  bool newEvent;
  float axis[6];
} spacemouseState;

static void spacemouseMessageHandler(unsigned int connection, natural_t messageType, void *_messageArgument) {
  ConnexionDeviceState *messageArgument = _messageArgument;

    switch (messageType)
    {
        case kConnexionMsgDeviceState: {
            //UE_LOG(LogSpaceNav3DController, Display, TEXT("Got axis data: TX %05d TY %05d TZ %05d RX %05d RY %05d RZ %05d"), (int)state->axis[0], (int)state->axis[1], (int)state->axis[2], (int)state->axis[3], (int)state->axis[4], (int)state->axis[5]);

          // TODO: Buttons
          for(int c = 0; c < SI_MAX; c++)
            spacemouseState.axis[c] = spacemouseToFloat(messageArgument->axis[c]);

            spacemouseState.newEvent = true;
        } break;
    }
}

static void spacemouseInit() {
  bzero(&spacemouseState, sizeof(spacemouseState)); // Rezero in case restart

  OSErr result = SetConnexionHandlers(spacemouseMessageHandler, spacemouseNullDeviceAddedHandler, spacemouseNullDeviceRemovedHandler, false);
  if (result) {
    fprintf(stderr, "Spacemouse: Failed to register with 3DConnexion API.");
    return;
  }

    unsigned char appName[12] = "\004Lovr"; // This has to be a Pascal string
    connexionClient = RegisterConnexionClient('UNRL', appName,
                                              kConnexionClientModeTakeOver, kConnexionMaskAll);
    SetConnexionClientButtonMask(connexionClient, kConnexionMaskAllButtons);
}

static void spacemouseDestroy() {
  UnregisterConnexionClient(connexionClient);
  connexionClient = 0;
}

// Apply a curve to a [-1..1] value
static float accel(float x, float p) {
  float ax = fabs(x);
  if (ax > deadZone) {
    bool neg = x < 0.0f;
    ax = (ax-deadZone)/(1.0f-deadZone);
    ax = pow(ax, p);
    return ax*(neg?-1.0f:1.0f);
  }
  return x;
}

static void spacemouseUpdate(float *v) { // v is an array of length 3
  v[0] += accel(spacemouseState.axis[SI_TX], 2) * 16.0f;
  v[1] += accel(-spacemouseState.axis[SI_TZ], 2) * 16.0f;
  v[2] += accel(spacemouseState.axis[SI_TY], 2) * 16.0f;

  state.yaw += accel(-spacemouseState.axis[SI_RZ], 1.5) / 12.0f;
  state.pitch += accel(spacemouseState.axis[SI_RX], 1.5) / 12.0f;

  if (state.pitch < -M_PI / 2.0) {
    state.pitch = -M_PI / 2.0;
  }

  if (state.pitch > M_PI / 2.0) {
    state.pitch = M_PI / 2.0;
  }
}

#else

#error "Attempted to compile with SPACEMOUSE_SUPPORT, but this is only supported on Macintosh"

#endif