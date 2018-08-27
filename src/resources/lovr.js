var LibraryLOVR = {
  $webvr: {
    buttonMap: {
      'OpenVR Gamepad': [null, null, 3, 1, 2, 0],
      'Oculus Touch (Right)': [null, null, 3, 1, 2, 0, null, 3, 4],
      'Oculus Touch (Left)': [null, null, 3, 1, 2, 0, null, null, null, 3, 4],
      'Spatial Controller': [null, null, 3, 1, 2, 4]
    },

    init: function(offset, added, removed, pressed, released, mount) {
      if (webvr.initialized || !Module.lovrDisplay) {
        return;
      }

      var a, b, c, d, e, canvas, display;
      webvr.initialized = true;
      webvr.display = display = Module.lovrDisplay;
      webvr.canvas = canvas = Module.canvas;
      webvr.frameData = new VRFrameData();
      webvr.gamepads = [];
      webvr.lastGamepadState = [];
      webvr.mirrored = true;
      webvr.offset = offset;
      webvr.controlleradded = added;
      webvr.controllerremoved = removed;
      webvr.controllerpressed = pressed;
      webvr.controllerreleased = released;
      webvr.mount = mount;
      webvr.renderCallback = C.NULL;
      webvr.renderUserdata = C.NULL;
      webvr.matA = a = Module._malloc(64);
      webvr.matB = b = Module._malloc(64);
      webvr.matC = c = Module._malloc(64);
      webvr.matD = d = Module._malloc(64);
      webvr.matE = e = Module._malloc(64);
      webvr.quat = Module._malloc(16);
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

      webvr.onvrdisplaypresentchange = function() {
        Runtime.dynCall('vi', webvr.mount, [display.isPresenting]);
      };

      webvr.frameId = display.requestAnimationFrame(function onAnimationFrame() {
        webvr.frameId = display.requestAnimationFrame(onAnimationFrame);
        display.getFrameData(webvr.frameData);

        if (webvr.renderCallback) {
          var sittingToStanding = webvr.display.stageParameters && webvr.display.stageParameters.sittingToStandingTransform;

          if (sittingToStanding) {
            HEAPF32.set(sittingToStanding, e >> 2);
            Module._mat4_invert(e);
          } else {
            Module._mat4_identity(e);
            HEAPF32[(e + 4 * 13) >> 2] = -webvr.offset;
          }

          HEAPF32.set(webvr.frameData.leftViewMatrix, a >> 2);
          HEAPF32.set(webvr.frameData.rightViewMatrix, b >> 2);
          HEAPF32.set(webvr.frameData.leftProjectionMatrix, c >> 2);
          HEAPF32.set(webvr.frameData.rightProjectionMatrix, d >> 2);
          Module._mat4_multiply(a, e);
          Module._mat4_multiply(b, e);
          Runtime.dynCall('viiiii', webvr.renderCallback, [a, b, c, d, webvr.renderUserdata]);
        }

        if (display.isPresenting) {
          display.submitFrame();
        }
      });

      window.addEventListener('lovr.entervr', webvr.onentervr);
      window.addEventListener('lovr.exitvr', webvr.onexitvr);
      window.addEventListener('vrdisplaypresentchange', webvr.onvrdisplaypresentchange);
    },

    destroy: function() {
      if (!webvr.initialized) {
        return;
      }

      webvr.initialized = false;
      Module._free(webvr.matA);
      Module._free(webvr.matB);
      Module._free(webvr.matC);
      Module._free(webvr.matD);
      Module._free(webvr.matE);
      Module._free(webvr.quat);

      window.removeEventListener('lovr.entervr', webvr.onentervr);
      window.removeEventListener('lovr.exitvr', webvr.onexitvr);
      window.removeEventListener('vrdisplaypresentchange', webvr.onvrdisplaypresentchange);

      if (webvr.frameId) {
        webvr.display.cancelAnimationFrame(webvr.frameId);
      }
    },

    controllerToGamepad: function(controller) {
      return webvr.gamepads[HEAPU32[(controller + C.sizeofRef) >> 2]];
    }
  },

  webvrInit: function(offset, added, removed, pressed, released, mount) {
    if (Module.lovrDisplay) {
      webvr.init(offset, added, removed, pressed, released, mount);
      return true;
    } else {
      return false;
    }
  },

  webvrDestroy: function() {
    webvr.destroy();
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
    var pose = webvr.frameData.pose;
    var matA = webvr.matA;
    var matB = webvr.matB;
    var quat = webvr.quat;

    if (pose.position && pose.orientation) {
      HEAPF32.set(pose.orientation, quat >> 2);

      Module._mat4_identity(matA);

      if (sittingToStanding) {
        HEAPF32.set(sittingToStanding, matB >> 2);
        Module._mat4_multiply(matA, matB);
      }

      Module._mat4_translate(matA, pose.position[0], pose.position[1], pose.position[2]);
      Module._mat4_rotateQuat(matA, quat);
      Module._mat4_translate(matA, eyeParameters.offset[0], eyeParameters.offset[1], eyeParameters.offset[2]);

      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
      Module._mat4_transform(matA, x, y, z);

      Module._quat_fromMat4(quat, matA);
      Module._quat_getAngleAxis(quat, angle, ax, ay, az);
    } else {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
      HEAPF32[angle >> 2] = HEAPF32[ax >> 2] = HEAPF32[ay >> 2] = HEAPF32[az >> 2] = 0;
    }
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
    } else if (gamepad.id.startsWith('Spatial Controller')) {
      switch (axis) {
        case C.CONTROLLER_AXIS_TRIGGER: return gamepad.buttons[1].value;
        case C.CONTROLLER_AXIS_TOUCHPAD_X: return gamepad.axes[2];
        case C.CONTROLLER_AXIS_TOUCHPAD_Y: return gamepad.axes[3];
        default: return 0;
      }
    }

    return 0;
  },

  webvrControllerIsDown: function(controller, button) {
    var gamepad = webvr.controllerToGamepad(controller);
    var buttonMap = webvr.buttonMap[gamepad.id] || webvr.buttonMap[gamepad.id.substr(0, 18)]; // substr for Spatial Controller prefix
    return gamepad && buttonMap && buttonMap[button] && gamepad.buttons[buttonMap[button]].pressed;
  },

  webvrControllerIsTouched: function(controller, button) {
    var gamepad = webvr.controllerToGamepad(controller);
    var buttonMap = webvr.buttonMap[gamepad.id] || webvr.buttonMap[gamepad.id.substr(0, 18)]; // substr for Spatial Controller prefix
    return gamepad && buttonMap && buttonMap[button] && gamepad.buttons[buttonMap[button]].touched;
  },

  webvrControllerVibrate: function(controller, duration, power) {
    var gamepad = webvr.controllerToGamepad(controller);

    if (gamepad && gamepad.hapticActuators && gamepad.hapticActuators[0]) {
      gamepad.hapticActuators[0].pulse(power, duration * 1000);
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
    var gamepads = navigator.getGamepads();

    // Poll for new gamepads
    for (var i = 0; i < gamepads.length; i++) {
      var gamepad = gamepads[i];
      if (gamepad && gamepad.pose && !webvr.gamepads[gamepad.index] && gamepad.displayId === webvr.display.displayId && gamepad.pose.position && gamepad.pose.orientation) {
        webvr.gamepads[gamepad.index] = gamepad;
        webvr.lastGamepadState[gamepad.index] = [];
        for (var i = 0; i < gamepad.buttons.length; i++) {
          webvr.lastGamepadState[gamepad.index][i] = gamepad.buttons[i].pressed;
        }
        webvr.controlleradded && Runtime.dynCall('vi', webvr.controlleradded, [gamepad.index]);
      }
    }

    // Process existing gamepads, checking for disconnection and button state changes
    for (var index in webvr.gamepads) {
      var gamepad = webvr.gamepads[index];
      if (!gamepad.connected || !gamepad.pose.position || !gamepad.pose.orientation) {
        webvr.controllerremoved && Runtime.dynCall('vi', webvr.controllerremoved, [gamepad.index]);
        delete webvr.lastGamepadState[gamepad.index];
        delete webvr.gamepads[gamepad.index];
      } else {
        var lastState = webvr.lastGamepadState[index];
        var buttonMap = webvr.buttonMap[gamepad.id] || webvr.buttonMap[gamepad.id.substr(0, 18)]; // substr for Spatial Controller prefix
        for (var button in buttonMap) {
          var buttonIndex = buttonMap[button];
          if (buttonIndex !== null) {
            var pressed = gamepad.buttons[buttonIndex].pressed;
            if (lastState[buttonIndex] !== pressed) {
              lastState[buttonIndex] = pressed;
              if (pressed) {
                Runtime.dynCall('vii', webvr.controllerpressed, [gamepad.index, button]);
              } else {
                Runtime.dynCall('vii', webvr.controllerreleased, [gamepad.index, button]);
              }
            }
          }
        }
      }
    }
  },

  $C: {
    NULL: 0,
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
  }
};

autoAddDeps(LibraryLOVR, '$webvr');
autoAddDeps(LibraryLOVR, '$C');
mergeInto(LibraryManager.library, LibraryLOVR);
