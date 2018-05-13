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
    sizeofRef: 8,
  },

  $lovr: {
    WebVR: {
      display: null,
      gamepads: {},
      initialized: false,
      mirrored: true,
      ongamepadconnected: 0,
      ongamepaddisconnected: 0,
      renderCallback: 0,
      renderUserdata: 0,
      frameData: null,
      width: 0,
      height: 0,
      matA: null,
      matB: null,
      quat: null,
      buttonMap: {
        'OpenVR Gamepad': [null, null, 3, 1, 2, 0, null, null, null, null, null],
        'Oculus Touch (Right)': [null, null, null, 1, 2, 0, null, 3, 4],
        'Oculus Touch (Left)': [null, null, null, 1, 2, 0, null, null, null, 3, 4]
      },

      init: function() {
        if (lovr.WebVR.initialized) {
          return;
        }

        var display, canvas, gamepads = lovr.WebVR.gamepads;
        lovr.WebVR.initialized = true;
        lovr.WebVR.canvas = canvas = Module['canvas'];
        lovr.WebVR.width = canvas.width;
        lovr.WebVR.height = canvas.height;
        lovr.WebVR.frameData = new VRFrameData();
        lovr.WebVR.matA = Module._malloc(64);
        lovr.WebVR.matB = Module._malloc(64);
        lovr.WebVR.quat = Module._malloc(16);

        function findDisplay() {
          navigator.getVRDisplays && navigator.getVRDisplays().then(function(displays) {
            lovr.WebVR.display = display = displays[0];
          });
        }

        function onResize() {
          if (display && display.isPresenting) {
            var eyeParams = display.getEyeParameters('left');
            lovr.WebVR.width = eyeParams.renderWidth;
            lovr.WebVR.height = eyeParams.renderHeight;
            canvas.width = lovr.WebVR.width * 2;
            canvas.height = lovr.WebVR.height;
          } else {
            canvas.width = lovr.WebVR.width = canvas.parentElement.offsetWidth * window.devicePixelRatio;
            canvas.height = lovr.WebVR.height = canvas.parentElement.offsetHeight * window.devicePixelRatio;
          }
        }

        window.requestAnimationFrame(function onAnimationFrame() {
          if (display) {
            display.requestAnimationFrame(onAnimationFrame);
            display.getFrameData(lovr.WebVR.frameData);

            if (display.isPresenting && lovr.WebVR.renderCallback) {
              Runtime.dynCall('vi', lovr.WebVR.renderCallback, [lovr.WebVR.renderUserdata]);
              display.submitFrame();
            }
          } else {
            window.requestAnimationFrame(onAnimationFrame);
          }
        });

        window.addEventListener('vrdisplayconnect', function(event) {
          if (!display) {
            lovr.WebVR.display = display = event.display;
          }
        });

        window.addEventListener('vrdisplaydisconnect', function(event) {
          if (!display || event.display === display) {
            lovr.WebVR.display = display = null;
            findDisplay();
          }
        });

        window.addEventListener('gamepadconnected', function(event) {
          var gamepad = event.gamepad;
          if (display && gamepad.displayId === display.displayId && gamepad.pose) {
            gamepads[gamepad.index] = gamepad;
            lovr.WebVR.ongamepadconnected && Runtime.dynCall('vi', lovr.WebVR.ongamepadconnected, [gamepad.index]);
          }
        });

        window.addEventListener('gamepaddisconnected', function(event) {
          var gamepad = event.gamepad;
          if (gamepads[gamepad.index]) {
            lovr.WebVR.ongamepaddisconnected && Runtime.dynCall('vi', lovr.WebVR.ongamepaddisconnected, [gamepad.index]);
            delete gamepads[gamepad.index];
          }
        });

        window.addEventListener('lovr.entervr', function() {
          if (display && !display.isPresenting) {
            display.requestPresent([{ source: canvas }]);
          }
        });

        window.addEventListener('lovr.exitvr', function() {
          if (display && display.isPresenting) {
            display.exitPresent();
          }
        });

        window.addEventListener('vrdisplaypresentchange', onResize);
        window.addEventListener('resize', onResize);

        findDisplay();
        onResize();
      },

      controllerToGamepad: function(controller) {
        var index = HEAPU32[(controller + C.sizeofRef) >> 2];
        return lovr.WebVR.gamepads[index];
      }
    }
  },

  webvrInit: function() {
    lovr.WebVR.init();
  },

  webvrSetControllerAddedCallback: function(callback) {
    lovr.WebVR.ongamepadconnected = callback;
  },

  webvrSetControllerRemovedCallback: function(callback) {
    lovr.WebVR.ongamepaddisconnected = callback;
  },

  webvrGetType: function() {
    return C.HEADSET_UNKNOWN;
  },

  webvrGetOriginType: function() {
    return lovr.WebVR.display && lovr.WebVR.display.stageParameters ? C.ORIGIN_FLOOR : C.ORIGIN_HEAD;
  },

  webvrIsMounted: function() {
    // Best we can do I think
    return lovr.WebVR.display && lovr.WebVR.display.isPresenting;
  },

  webvrIsMirrored: function() {
    return lovr.WebVR.mirrored;
  },

  webvrSetMirrored: function(mirror) {
    lovr.WebVR.mirrored = mirror;
  },

  webvrGetDisplayDimensions: function(width, height) {
    HEAP32[width >> 2] = lovr.WebVR.width;
    HEAP32[height >> 2] = lovr.WebVR.height;
  },

  webvrGetClipDistance: function(clipNear, clipFar) {
    if (lovr.WebVR.display) {
      HEAPF32[clipNear >> 2] = lovr.WebVR.display.depthNear;
      HEAPF32[clipFar >> 2] = lovr.WebVR.display.depthFar;
    } else {
      HEAPF32[clipNear >> 2] = HEAPF32[clipFar >> 2] = 0;
    }
  },

  webvrSetClipDistance: function(clipNear, clipFar) {
    if (lovr.WebVR.display) {
      lovr.WebVR.display.depthNear = clipNear;
      lovr.WebVR.display.depthFar = clipFar;
    }
  },

  webvrGetBoundsDimensions: function(width, depth) {
    var stage = lovr.WebVR.display && lovr.WebVR.display.stageParameters;
    if (stage) {
      HEAPF32[width >> 2] = stage.sizeX;
      HEAPF32[depth >> 2] = stage.sizeZ;
    } else {
      HEAPF32[width >> 2] = HEAPF32[depth >> 2] = 0;
    }
  },

  webvrGetPose: function(x, y, z, angle, ax, ay, az) {
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var pose = lovr.WebVR.frameData.pose;
    var matA = lovr.WebVR.matA;
    var quat = lovr.WebVR.quat;

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
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var eyeParameters = lovr.WebVR.display && lovr.WebVR.display.getEyeParameters(isLeft ? 'left' : 'right');
    var matA = lovr.WebVR.matA;
    var matB = lovr.WebVR.matB;
    var quat = lovr.WebVR.quat;

    if (sittingToStanding) {
      HEAPF32.set(sittingToStanding, matA >> 2);
      HEAPF32.set(isLeft ? lovr.WebVR.frameData.leftViewMatrix : lovr.WebVR.frameData.rightViewMatrix, matB >> 2);
      Module._mat4_invert(matB);
      Module._mat4_multiply(matA, matB);
    } else {
      HEAPF32.set(isLeft ? lovr.WebVR.frameData.leftViewMatrix : lovr.WebVR.frameData.rightViewMatrix, matA >> 2);
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
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var pose = lovr.WebVR.frameData.pose;
    var matA = lovr.WebVR.matA;

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
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var pose = lovr.WebVR.frameData.pose;
    var matA = lovr.WebVR.matA;

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
    var gamepad = lovr.WebVR.controllerToGamepad(controller);
    return gamepad && gamepad.connected;
  },

  webvrControllerGetHand: function(controller) {
    var gamepad = lovr.WebVR.controllerToGamepad(controller);
    var handMap = { '': 0, left: 1, right: 2 };
    return gamepad && handMap[gamepad.hand || ''];
  },

  webvrControllerGetPose: function(controller, x, y, z, angle, ax, ay, az) {
    var gamepad = lovr.WebVR.controllerToGamepad(controller);
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var matA = lovr.WebVR.matA;
    var quat = lovr.WebVR.quat;

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
    var gamepad = lovr.WebVR.controllerToGamepad(controller);

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
    var gamepad = lovr.WebVR.controllerToGamepad(controller);
    var buttonMap = lovr.WebVR.buttonMap;
    return gamepad && buttonMap[gamepad.id] && gamepad.buttons[buttonMap[gamepad.id]].pressed;
  },

  webvrControllerIsTouched: function(controller, button) {
    var gamepad = lovr.WebVR.controllerToGamepad(controller);
    var buttonMap = lovr.WebVR.buttonMap;
    return gamepad && buttonMap[gamepad.id] && gamepad.buttons[buttonMap[gamepad.id]].touched;
  },

  webvrControllerVibrate: function(controller, duration, power) {
    var gamepad = lovr.WebVR.controllerToGamepad(controller);

    if (gamepad && gamepad.hapticActuators && gamepad.hapticActuators[0]) {
      gamepad.hapticActuators[0].pulse(power, duration);
    }
  },

  webvrControllerNewModelData: function(controller) {
    return 0;
  },

  webvrRenderTo: function(callback, userdata) {
    lovr.WebVR.renderCallback = callback;
    lovr.WebVR.renderUserdata = userdata;
  }
};

autoAddDeps(LibraryLOVR, '$lovr');
autoAddDeps(LibraryLOVR, '$C');
mergeInto(LibraryManager.library, LibraryLOVR);
