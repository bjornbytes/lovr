var LibraryLOVR = {
  $C: {
    ORIGIN_HEAD: 0,
    ORIGIN_FLOOR: 1,
    HEADSET_UNKNOWN: 0,
    EYE_LEFT: 0,
    EYE_RIGHT: 1,
    HAND_UNKNOWN: 0,
    HAND_LEFT: 1,
    HAND_RIGHT: 2,
    CONTROLLER_AXIS_TRIGGER: 0,
    CONTROLLER_AXIS_GRIP: 1,
    CONTROLLER_AXIS_TOUCHPAD_X: 2,
    CONTROLLER_AXIS_TOUCHPAD_Y: 3,
    sizeofRef: 8
  },

  $webvr: {
    display: null,
    gamepads: {},
    lastGamepadState: {},
    initialized: false,
    mirrored: true,
    oncontrolleradded: 0,
    oncontrollerremoved: 0,
    oncontrollerpressed: 0,
    oncontrollerreleased: 0,
    onmount: 0,
    renderCallback: 0,
    renderUserdata: 0,
    frameData: null,
    width: 0,
    height: 0,
    matA: null,
    matB: null,
    quat: null,
    buttonMap: {
      'OpenVR Gamepad': [null, null, 3, 1, 2, 0],
      'Oculus Touch (Right)': [null, null, 3, 1, 2, 0, null, 3, 4],
      'Oculus Touch (Left)': [null, null, 3, 1, 2, 0, null, null, null, 3, 4]
    },

    init: function() {
      if (webvr.initialized || !Module.lovrDisplay) {
        return;
      }

      var canvas, display, gamepads = webvr.gamepads;
      webvr.initialized = true;
      webvr.display = display = Module.lovrDisplay;
      webvr.canvas = canvas = Module.canvas;
      webvr.frameData = new VRFrameData();
      webvr.matA = Module._malloc(64);
      webvr.matB = Module._malloc(64);
      webvr.quat = Module._malloc(16);

      var eyeParams = display.getEyeParameters('left');
      webvr.width = eyeParams.renderWidth;
      webvr.height = eyeParams.renderHeight;
      canvas.width = webvr.width * 2;
      canvas.height = webvr.height;

      display.requestAnimationFrame(function onAnimationFrame() {
        display.requestAnimationFrame(onAnimationFrame);
        display.getFrameData(webvr.frameData);

        if (webvr.renderCallback) {
          Runtime.dynCall('vi', webvr.renderCallback, [webvr.renderUserdata]);
        }

        if (display.isPresenting) {
          display.submitFrame();
        }
      });

      window.addEventListener('gamepadconnected', function(event) {
        var gamepad = event.gamepad;
        if (gamepad.displayId === display.displayId && gamepad.pose) {
          gamepads[gamepad.index] = gamepad;
          webvr.lastGamepadState[gamepad.index] = gamepad.buttons.map(function(button) { return button.pressed; });
          webvr.oncontrolleradded && Runtime.dynCall('vi', webvr.oncontrolleradded, [gamepad.index]);
        }
      });

      window.addEventListener('gamepaddisconnected', function(event) {
        var gamepad = event.gamepad;
        if (gamepads[gamepad.index]) {
          webvr.oncontrollerremoved && Runtime.dynCall('vi', webvr.oncontrollerremoved, [gamepad.index]);
          delete webvr.lastGamepadState[gamepad.index];
          delete gamepads[gamepad.index];
        }
      });

      window.addEventListener('lovr.entervr', function() {
        if (!display.isPresenting) {
          display.requestPresent([{ source: canvas }]);
        }
      });

      window.addEventListener('lovr.exitvr', function() {
        if (display.isPresenting) {
          display.exitPresent();
        }
      });

      window.addEventListener('vrdisplaypresentchange', function() {
        Runtime.dynCall('vi', webvr.onmount, [display.isPresenting]);
      });
    },

    controllerToGamepad: function(controller) {
      var index = HEAPU32[(controller + C.sizeofRef) >> 2];
      return webvr.gamepads[index];
    }
  },

  webvrInit: function() {
    if (Module.lovrDisplay) {
      webvr.init();
      return true;
    } else {
      return false;
    }
  },

  webvrSetCallbacks: function(added, removed, pressed, released, mount) {
    webvr.oncontrolleradded = added;
    webvr.oncontrollerremoved = removed;
    webvr.oncontrollerpressed = pressed;
    webvr.oncontrollerreleased = released;
    webvr.onmount = mount;
  },

  webvrGetType: function() {
    return C.HEADSET_UNKNOWN;
  },

  webvrGetOriginType: function() {
    return webvr.display.stageParameters ? C.ORIGIN_FLOOR : C.ORIGIN_HEAD;
  },

  webvrIsMounted: function() {
    return webvr.display.isPresenting;
  },

  webvrIsMirrored: function() {
    return webvr.mirrored;
  },

  webvrSetMirrored: function(mirror) {
    webvr.mirrored = mirror;
  },

  webvrGetDisplayDimensions: function(width, height) {
    HEAP32[width >> 2] = webvr.width;
    HEAP32[height >> 2] = webvr.height;
  },

  webvrGetClipDistance: function(clipNear, clipFar) {
    HEAPF32[clipNear >> 2] = webvr.display.depthNear;
    HEAPF32[clipFar >> 2] = webvr.display.depthFar;
  },

  webvrSetClipDistance: function(clipNear, clipFar) {
    webvr.display.depthNear = clipNear;
    webvr.display.depthFar = clipFar;
  },

  webvrGetBoundsDimensions: function(width, depth) {
    var stage = webvr.display.stageParameters;
    if (stage) {
      HEAPF32[width >> 2] = stage.sizeX;
      HEAPF32[depth >> 2] = stage.sizeZ;
    } else {
      HEAPF32[width >> 2] = HEAPF32[depth >> 2] = 0;
    }
  },

  webvrGetPose: function(x, y, z, angle, ax, ay, az) {
    var sittingToStanding = webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform;
    var pose = webvr.frameData.pose;
    var matA = webvr.matA;
    var quat = webvr.quat;

    if (pose.position) {
      HEAPF32[x >> 2] = pose.position[0];
      HEAPF32[y >> 2] = pose.position[1];
      HEAPF32[z >> 2] = pose.position[2];

      if (sittingToStanding) {
        HEAPF32.set(sittingToStanding, matA >> 2);
        Module._mat4_transform(matA, x, y, z);
      }
    } else {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
    }

    if (pose.orientation) {
      HEAPF32.set(pose.orientation, quat >> 2);

      if (sittingToStanding) {
        HEAPF32.set(sittingToStanding, matA >> 2);
        Module._mat4_rotateQuat(matA, quat);
        Module._quat_fromMat4(quat, matA);
        Module._quat_getAngleAxis(quat, angle, ax, ay, az);
      } else {
        Module._quat_getAngleAxis(quat, angle, ax, ay, az);
      }
    } else {
      HEAPF32[angle >> 2] = HEAPF32[ax >> 2] = HEAPF32[ay >> 2] = HEAPF32[az >> 2] = 0;
    }
  },

  webvrGetEyePose: function(eye, x, y, z, angle, ax, ay, az) {
    var isLeft = eye === C.EYE_LEFT;
    var sittingToStanding = webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform;
    var eyeParameters = webvr.display.getEyeParameters(isLeft ? 'left' : 'right');
    var matA = webvr.matA;
    var matB = webvr.matB;
    var quat = webvr.quat;

    if (sittingToStanding) {
      HEAPF32.set(sittingToStanding, matA >> 2);
      HEAPF32.set(isLeft ? webvr.frameData.leftViewMatrix : webvr.frameData.rightViewMatrix, matB >> 2);
      Module._mat4_invert(matB);
      Module._mat4_multiply(matA, matB);
    } else {
      HEAPF32.set(isLeft ? webvr.frameData.leftViewMatrix : webvr.frameData.rightViewMatrix, matA >> 2);
      Module._mat4_invert(matA);
    }

    Module._mat4_translate(matA, eyeParameters.offset[0], eyeParameters.offset[1], eyeParameters.offset[2]);
    HEAPF32[x >> 2] = matA[12];
    HEAPF32[y >> 2] = matA[13];
    HEAPF32[z >> 2] = matA[14];

    Module._quat_fromMat4(quat, matA);
    Module._quat_getAngleAxis(quat, angle, ax, ay, az);
  },

  webvrGetVelocity: function(x, y, z) {
    var sittingToStanding = webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform;
    var pose = webvr.frameData.pose;
    var matA = webvr.matA;

    if (pose.linearVelocity) {
      HEAPF32[x >> 2] = pose.linearVelocity[0];
      HEAPF32[y >> 2] = pose.linearVelocity[1];
      HEAPF32[z >> 2] = pose.linearVelocity[2];

      if (sittingToStanding) {
        HEAPF32.set(sittingToStanding, matA >> 2);
        Module._mat4_transformDirection(matA, x, y, z);
      }
    } else {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
    }
  },

  webvrGetAngularVelocity: function(x, y, z) {
    var sittingToStanding = webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform;
    var pose = webvr.frameData.pose;
    var matA = webvr.matA;

    if (pose.angularVelocity) {
      HEAPF32[x >> 2] = pose.angularVelocity[0];
      HEAPF32[y >> 2] = pose.angularVelocity[1];
      HEAPF32[z >> 2] = pose.angularVelocity[2];

      if (sittingToStanding) {
        HEAPF32.set(sittingToStanding, matA >> 2);
        Module._mat4_transformDirection(matA, x, y, z);
      }
    } else {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
    }
  },

  webvrControllerIsConnected: function(controller) {
    var gamepad = webvr.controllerToGamepad(controller);
    return gamepad && gamepad.connected;
  },

  webvrControllerGetHand: function(controller) {
    var gamepad = webvr.controllerToGamepad(controller);
    var handMap = { '': 0, left: 1, right: 2 };
    return gamepad && handMap[gamepad.hand || ''];
  },

  webvrControllerGetPose: function(controller, x, y, z, angle, ax, ay, az) {
    var gamepad = webvr.controllerToGamepad(controller);
    var sittingToStanding = webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform;
    var matA = webvr.matA;
    var quat = webvr.quat;

    if (!gamepad || !gamepad.pose || !gamepad.pose.position || !gamepad.pose.orientation) {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
      HEAPF32[angle >> 2] = HEAPF32[ax >> 2] = HEAPF32[ay >> 2] = HEAPF32[az >> 2] = 0;
      return;
    }

    HEAPF32[x >> 2] = gamepad.pose.position[0];
    HEAPF32[y >> 2] = gamepad.pose.position[1];
    HEAPF32[z >> 2] = gamepad.pose.position[2];

    HEAPF32.set(gamepad.pose.orientation, quat >> 2);
    if (sittingToStanding) {
      HEAPF32.set(sittingToStanding, matA >> 2);
      Module._mat4_transform(matA, x, y, z);
      Module._mat4_rotateQuat(matA, quat);
      Module._quat_fromMat4(quat, matA);
      Module._quat_getAngleAxis(quat, angle, ax, ay, az);
    } else {
      Module._quat_getAngleAxis(quat, angle, ax, ay, az);
    }
  },

  webvrControllerGetAxis: function(controller, axis) {
    var gamepad = webvr.controllerToGamepad(controller);

    if (!gamepad) {
      return 0;
    }

    if (gamepad.id.startsWith('OpenVR')) {
      switch (axis) {
        case C.CONTROLLER_AXIS_TRIGGER: return gamepad.buttons[1].value;
        case C.CONTROLLER_AXIS_TOUCHPAD_X: return gamepad.axes[0];
        case C.CONTROLLER_AXIS_TOUCHPAD_Y: return gamepad.axes[1];
        default: return 0;
      }
    } else if (gamepad.id.startsWith('Oculus')) {
      switch (axis) {
        case C.CONTROLLER_AXIS_TRIGGER: return gamepad.axes[3 /* ? */];
        case C.CONTROLLER_AXIS_GRIP: return gamepad.axes[2 /* ? */];
        case C.CONTROLLER_AXIS_TOUCHPAD_X: return gamepad.axes[0];
        case C.CONTROLLER_AXIS_TOUCHPAD_Y: return gamepad.axes[1];
        default: return 0;
      }
    }

    return 0;
  },

  webvrControllerIsDown: function(controller, button) {
    var gamepad = webvr.controllerToGamepad(controller);
    var buttonMap = webvr.buttonMap;
    return gamepad && buttonMap[gamepad.id] && buttonMap[gamepad.id][button] && gamepad.buttons[buttonMap[gamepad.id][button]].pressed;
  },

  webvrControllerIsTouched: function(controller, button) {
    var gamepad = webvr.controllerToGamepad(controller);
    var buttonMap = webvr.buttonMap;
    return gamepad && buttonMap[gamepad.id] && buttonMap[gamepad.id][button] && gamepad.buttons[buttonMap[gamepad.id][button]].touched;
  },

  webvrControllerVibrate: function(controller, duration, power) {
    var gamepad = webvr.controllerToGamepad(controller);

    if (gamepad && gamepad.hapticActuators && gamepad.hapticActuators[0]) {
      gamepad.hapticActuators[0].pulse(power, duration);
    }
  },

  webvrControllerNewModelData: function(controller) {
    return 0;
  },

  webvrSetRenderCallback: function(callback, userdata) {
    webvr.renderCallback = callback;
    webvr.renderUserdata = userdata;
  },

  webvrUpdate: function(dt) {
    for (var index in webvr.gamepads) {
      var gamepad = webvr.gamepads[index];
      var lastState = webvr.lastGamepadState[index];
      var buttonMap = webvr.buttonMap[gamepad.id];
      for (var button in buttonMap) {
        if (buttonMap[button]) {
          var pressed = gamepad.buttons[buttonMap[button]].pressed;
          if (lastState[buttonIndex] !== pressed) {
            lastState[buttonIndex] = pressed;
            if (pressed) {
              Runtime.dynCall('vii', webvr.oncontrollerpressed, [gamepad.index, button]);
            } else {
              Runtime.dynCall('vii', webvr.oncontrollerreleased, [gamepad.index, button]);
            }
          }
        }
      }
    }
  }
};

autoAddDeps(LibraryLOVR, '$webvr');
autoAddDeps(LibraryLOVR, '$C');
mergeInto(LibraryManager.library, LibraryLOVR);
