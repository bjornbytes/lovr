var LibraryLOVR = {
  $webvr: {
    buttonMap: {
      'OpenVR Gamepad': [1, 1, null, 0, 2],
      'Oculus Touch (Left)': [1, 1, 0, null, 2, null, null, null, 3, 4],
      'Oculus Touch (Right)': [1, 1, 0, null, 2, null, 3, 4, null, null],
      'Spatial Controller (Spatial Interaction Source) 045E-065D': [0, 0, 1, 3, 2, 4]
    },

    refreshGamepads: function(event) {
      if (event.gamepad.hand) {
        var device = ({
          'left': C.DEVICE_HAND_LEFT,
          'right': C.DEVICE_HAND_RIGHT
        })[event.gamepad.hand];

        if (device) {
          webvr.gamepads[device] = event.gamepad;
          webvr.poses[device] = event.gamepad.pose;
        }
      }
    }
  },

  webvr_init: function(offset, msaa) {
    if (webvr.initialized || !Module.lovrDisplay) {
      return false;
    }

    var a, b, c, d, e, canvas, display;
    webvr.initialized = true;
    webvr.display = display = Module.lovrDisplay;
    webvr.display.depthNear = .1;
    webvr.display.depthFar = 100;
    webvr.canvas = canvas = Module.canvas;
    webvr.frameData = new VRFrameData();
    webvr.gamepads = [];
    webvr.poses = [];
    webvr.offset = offset;
    webvr.poseTransform = Module._malloc(64);
    webvr.matA = a = Module._malloc(64);
    webvr.matB = b = Module._malloc(64);
    webvr.matC = c = Module._malloc(64);
    webvr.matD = d = Module._malloc(64);
    webvr.matE = e = Module._malloc(64);
    webvr.width = display.getEyeParameters('left').renderWidth * 2;
    webvr.height = display.getEyeParameters('left').renderHeight;
    Browser.setCanvasSize(webvr.width, webvr.height);

    webvr.onentervr = function() {
      if (!display.isPresenting) {
        display.requestPresent([{ source: canvas }]);
      }
    };

    webvr.onexitvr = function() {
      if (display.isPresenting) {
        display.exitPresent();
      }
    };

    webvr.frameId = display.requestAnimationFrame(function onAnimationFrame() {
      webvr.frameId = display.requestAnimationFrame(onAnimationFrame);
      display.getFrameData(webvr.frameData);
      webvr.poses[0] = webvr.frameData.pose;

      if (webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform) {
        HEAPF32.set(webvr.display.stageParameters.sittingToStandingTransform, webvr.poseTransform >> 2);
      } else {
        Module._mat4_identity(webvr.poseTransform);
        HEAPF32[webvr.poseTransform >> 2 + 13] = webvr.offset;
      }

      Module._mat4_set(e, webvr.poseTransform);
      Module._mat4_invert(e);
      HEAPF32.set(webvr.frameData.leftViewMatrix, a >> 2);
      HEAPF32.set(webvr.frameData.rightViewMatrix, b >> 2);
      HEAPF32.set(webvr.frameData.leftProjectionMatrix, c >> 2);
      HEAPF32.set(webvr.frameData.rightProjectionMatrix, d >> 2);
      Module._mat4_multiply(a, e);
      Module._mat4_multiply(b, e);
      Module._webvr_onAnimationFrame(a, b, c, d);

      if (display.isPresenting) {
        display.submitFrame();
      }
    });

    window.addEventListener('lovr.entervr', webvr.onentervr);
    window.addEventListener('lovr.exitvr', webvr.onexitvr);
    window.addEventListener('vrdisplaypresentchange', webvr.onvrdisplaypresentchange);
    window.addEventListener('gamepadconnected', webvr.refreshGamepads);
    window.addEventListener('gamepaddisconnected', webvr.refreshGamepads);
    return true;
  },

  webvr_destroy: function() {
    if (!webvr.initialized) {
      return;
    }

    webvr.initialized = false;
    Module._free(webvr.poseTransform);
    Module._free(webvr.matA);
    Module._free(webvr.matB);
    Module._free(webvr.matC);
    Module._free(webvr.matD);
    Module._free(webvr.matE);

    window.removeEventListener('lovr.entervr', webvr.onentervr);
    window.removeEventListener('lovr.exitvr', webvr.onexitvr);
    window.removeEventListener('vrdisplaypresentchange', webvr.onvrdisplaypresentchange);
    window.removeEventListener('gamepadconnected', webvr.refreshGamepads);
    window.removeEventListener('gamepaddisconnected', webvr.refreshGamepads);

    if (webvr.frameId) {
      webvr.display.cancelAnimationFrame(webvr.frameId);
    }
  },

  webvr_getName: function() {
    return false;
  },

  webvr_getOriginType: function() {
    return webvr.display.stageParameters ? C.ORIGIN_FLOOR : C.ORIGIN_HEAD;
  },

  webvr_getDisplayTime: function() {
    return webvr.frameData.timestamp / 1000;
  },

  webvr_getDisplayDimensions: function(width, height) {
    HEAPU32[width >> 2] = webvr.width;
    HEAPU32[height >> 2] = webvr.height;
  },

  webvr_getDisplayMask: function(count) {
    HEAPU32[count >> 2] = 0;
    return 0;
  },

  webvr_getViewCount: function() {
    return 2;
  },

  webvr_getViewPose: function(view, position, orientation) {
    return false; // TODO
  },

  webvr_getViewAngles: function(view, left, right, up, down) {
    return false; // TODO
  },

  webvr_getClipDistance: function(clipNear, clipFar) {
    HEAPF32[clipNear >> 2] = webvr.display.depthNear;
    HEAPF32[clipFar >> 2] = webvr.display.depthFar;
  },

  webvr_setClipDistance: function(clipNear, clipFar) {
    webvr.display.depthNear = clipNear;
    webvr.display.depthFar = clipFar;
  },

  webvr_getBoundsDimensions: function(width, depth) {
    var stage = webvr.display.stageParameters;
    if (stage) {
      HEAPF32[width >> 2] = stage.sizeX;
      HEAPF32[depth >> 2] = stage.sizeZ;
    } else {
      HEAPF32[width >> 2] = HEAPF32[depth >> 2] = 0;
    }
  },

  webvr_getBoundsGeometry: function(count) {
    HEAP32[count >> 2] = 0;
    return 0;
  },

  webvr_getPose: function(device, position, orientation) {
    var pose = webvr.poses[device];
    if (!pose) { return false; }

    if (pose.position) {
      HEAPF32.set(pose.position, position >> 2);
      Module._mat4_transform(webvr.poseTransform, position);
    } else {
      HEAPF32.fill(0, position >> 2, position >> 2 + 3);
    }

    if (pose.orientation) {
      HEAPF32.set(pose.orientation, orientation >> 2);
      Module._mat4_set(webvr.matA, webvr.poseTransform);
      Module._mat4_rotateQuat(webvr.matA, orientation);
      Module._quat_fromMat4(orientation, webvr.matA);
    } else {
      HEAPF32.fill(0, orientation >> 2, orientation >> 2 + 4);
    }

    return true;
  },

  webvr_getVelocity: function(device, velocity, angularVelocity) {
    var pose = webvr.poses[device];
    if (!pose) { return false; }

    if (pose.linearVelocity) {
      HEAPF32.set(pose.linearVelocity, velocity >> 2);
      Module._mat4_transformDirection(webvr.poseTransform, velocity);
    } else {
      HEAPF32.fill(0, velocity >> 2, velocity >> 2 + 3);
    }

    if (pose.angularVelocity) {
      HEAPF32.set(pose.angularVelocity, angularVelocity >> 2);
      Module._mat4_transformDirection(webvr.poseTransform, angularVelocity);
    } else {
      HEAPF32.fill(0, angularVelocity >> 2, angularVelocity >> 2 + 3);
    }

    return true;
  },

  webvr_isDown: function(device, button, down, changed) {
    var gamepad = webvr.gamepads[device];

    if (!gamepad || !gamepad.id || !webvr.buttonMap[gamepad.id] || !webvr.buttonMap[gamepad.id][button]) {
      return false;
    }

    HEAPF32[down >> 2] = gamepad.buttons[webvr.buttonMap[gamepad.id][button]].pressed;
    HEAPF32[changed >> 2] = false; // TODO
    return true;
  },

  webvr_isTouched: function(device, button, touched) {
    var gamepad = webvr.gamepads[device];

    if (!gamepad || !gamepad.id || !webvr.buttonMap[gamepad.id] || !webvr.buttonMap[gamepad.id][button]) {
      return false;
    }

    HEAPF32[touched >> 2] = gamepad.buttons[webvr.buttonMap[gamepad.id][button]].touched;
    return true;
  },

  webvr_getAxis: function(device, axis, value) {
    var gamepad = webvr.gamepads[device];

    if (!gamepad) {
      return false;
    }

    if (gamepad.id.startsWith('OpenVR')) {
      switch (axis) {
        case C.AXIS_TRIGGER: HEAPF32[value >> 2] = gamepad.buttons[1].value; return true;
        case C.AXIS_TOUCHPAD:
          HEAPF32[value >> 2 + 0] = gamepad.axes[0];
          HEAPF32[value >> 2 + 1] = gamepad.axes[1];
          return true;
        default: return false;
      }
    } else if (gamepad.id.startsWith('Oculus')) {
      switch (axis) {
        case C.AXIS_TRIGGER: HEAPF32[value >> 2] = gamepad.buttons[1].value; return true;
        case C.AXIS_GRIP: HEAPF32[value >> 2] = gamepad.buttons[2].value; return true;
        case C.AXIS_THUMBSTICK:
          HEAPF32[value >> 2 + 0] = gamepad.axes[0];
          HEAPF32[value >> 2 + 1] = gamepad.axes[1];
          return true;
        default: return false;
      }
    } else if (gamepad.id.startsWith('Spatial Controller')) {
      switch (axis) {
        case C.AXIS_TRIGGER: HEAPF32[value >> 2] = gamepad.buttons[0].value; return true;
        case C.AXIS_THUMBSTICK:
          HEAPF32[value >> 2 + 0] = gamepad.axes[0];
          HEAPF32[value >> 2 + 1] = gamepad.axes[1];
          return true;
        case C.AXIS_TOUCHPAD:
          HEAPF32[value >> 2 + 0] = gamepad.axes[2];
          HEAPF32[value >> 2 + 1] = gamepad.axes[3];
          return true;
        default: return false;
      }
    }

    return false;
  },

  webvr_vibrate: function(device, strength, duration, frequency) {
    var gamepad = webvr.gamepads[device];

    if (gamepad && gamepad.hapticActuators && gamepad.hapticActuators[0]) {
      gamepad.hapticActuators[0].pulse(strength, duration * 1000);
      return true;
    }

    return false;
  },

  webvr_newModelData: function(device) {
    return C.NULL;
  },

  webvr_update: function(dt) {
    //
  },

  $C: {
    NULL: 0,

    // HeadsetOrigin
    ORIGIN_HEAD: 0,
    ORIGIN_FLOOR: 1,

    // Device
    DEVICE_HAND_LEFT: 0,
    DEVICE_HAND_RIGHT: 1,

    // DeviceAxis
    AXIS_TRIGGER: 0,
    AXIS_THUMBSTICK: 1,
    AXIS_TOUCHPAD: 2,
    AXIS_PINCH: 3,
    AXIS_GRIP: 4
  }
};

autoAddDeps(LibraryLOVR, '$webvr');
autoAddDeps(LibraryLOVR, '$C');
mergeInto(LibraryManager.library, LibraryLOVR);
