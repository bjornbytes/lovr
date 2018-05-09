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
    sizeofRef: 16
  },

  $lovr: {
    WebVR: {
      display: null,
      gamepads: {},
      initialized: false,
      mirrored: true,
      ongamepadconnected: null,
      ongamepaddisconnected: null,
      renderCallback: null,
      renderUserdata: null,
      frameData: null,
      width: 0,
      height: 0,
      tempMatA: new Float32Array(16),
      tempMatB: new Float32Array(16),
      tempQuat: new Float32Array(4),

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
              Runtime.dynCall('vi', lovr.WebVR.renderCallback, lovr.WebVR.renderUserdata);
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

        window.addEventListener('gamepadconnected', function(gamepad) {
          if (display && gamepad.displayId === display.displayId && gamepad.pose) {
            gamepads[gamepad.index] = gamepad;
            lovr.WebVR.ongamepadconnected && lovr.WebVR.ongamepadconnected(gamepad.index);
          }
        });

        window.addEventListener('gamepaddisconnected', function(gamepad) {
          if (gamepads[gamepad.index]) {
            lovr.WebVR.ongamepaddisconnected && lovr.WebVR.ongamepaddisconnected(gamepad.index);
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
        var index = HEAPU32[(controller + sizeofRef) >> 2];
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
    return WebVR.display && WebVR.display.stageParameters ? C.ORIGIN_FLOOR : C.ORIGIN_HEAD;
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
    HEAPF32[clipNear >> 2] = lovr.WebVR.display ? lovr.WebVR.display.depthNear : 0;
    HEAPF32[clipFar >> 2] = lovr.WebVR.display ? lovr.WebVR.display.depthFar : 0;
  },

  webvrSetClipDistance: function(clipNear, clipFar) {
    if (lovr.WebVR.display) {
      lovr.WebVR.display.depthNear = clipNear;
      lovr.WebVR.display.depthFar = clipFar;
    }
  },

  webvrGetBoundsDimensions: function(width, depth) {
    var stage = lovr.WebVR.display && lovr.WebVR.display.stageParameters;
    HEAPF32[width >> 2] = stage ? stage.sizeX : 0;
    HEAPF32[depth >> 2] = stage ? stage.sizeZ : 0;
  },

  webvrGetPose: function(x, y, z, angle, ax, ay, az) {
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var pose = lovr.WebVR.frameData.pose;

    if (pose.position) {
      HEAPF32[x >> 2] = pose.position[0];
      HEAPF32[y >> 2] = pose.position[1];
      HEAPF32[z >> 2] = pose.position[2];

      if (sittingToStanding) {
        Module._mat4_transform(sittingToStanding, x, y, z);
      }
    } else {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
    }

    if (pose.orientation) {
      if (sittingToStanding) {
        Module._mat4_set(lovr.WebVR.tempMatA, sittingToStanding);
        Module._mat4_rotateQuat(lovr.WebVR.tempMatA, pose.orientation);
        Module._quat_fromMat4(lovr.WebVR.tempQuat, lovr.WebVR.tempMatA);
        Module._quat_getAngleAxis(lovr.WebVR.tempQuat, angle, ax, ay, az);
      } else {
        Module._quat_getAngleAxis(pose.orientation, angle, ax, ay, az);
      }
    } else {
      HEAPF32[angle >> 2] = HEAPF32[ax >> 2] = HEAPF32[ay >> 2] = HEAPF32[az >> 2] = 0;
    }
  },

  webvrGetEyePose: function(eye, x, y, z, angle, ax, ay, az) {
    var isLeft = eye === C.EYE_LEFT;
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var eyeParameters = lovr.WebVR.display && lovr.WebVR.display.getEyeParameters(isLeft ? 'left' : 'right');

    if (sittingToStanding) {
      Module._mat4_set(lovr.WebVR.tempMatA, sittingToStanding);
      Module._mat4_set(lovr.WebVR.tempMatB, isLeft ? lovr.WebVR.frameData.leftViewMatrix : lovr.WebVR.frameData.rightViewMatrix);
      Module._mat4_invert(lovr.WebVR.tempMatB);
      Module._mat4_multiply(lovr.WebVR.tempMatA, lovr.WebVR.tempMatB);
      Module._mat4_translate(lovr.WebVR.tempMatA, eyeParameters.offset[0], eyeParameters.offset[1], eyeParameters.offset[2]);
    } else {
      Module._mat4_set(lovr.WebVR.tempMatA, isLeft ? lovr.WebVR.frameData.leftViewMatrix : lovr.WebVR.frameData.rightViewMatrix);
      Module._mat4_invert(lovr.WebVR.tempMatA);
      Module._mat4_multiply(lovr.WebVR.tempMatA, lovr.WebVR.tempMatB);
      Module._mat4_translate(lovr.WebVR.tempMatA, eyeParameters.offset[0], eyeParameters.offset[1], eyeParameters.offset[2]);
    }

    HEAPF32[x >> 2] = lovr.WebVR.tempMatA[12];
    HEAPF32[y >> 2] = lovr.WebVR.tempMatA[13];
    HEAPF32[z >> 2] = lovr.WebVR.tempMatA[14];

    Module._quat_fromMat4(lovr.WebVR.tempQuat, lovr.WebVR.tempMatA);
    Module._quat_getAngleAxis(lovr.WebVR.tempQuat, angle, ax, ay, az);
  },

  webvrGetVelocity: function(x, y, z) {
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var pose = lovr.WebVR.frameData.pose;

    if (pose.linearVelocity) {
      HEAPF32[x >> 2] = pose.linearVelocity[0];
      HEAPF32[y >> 2] = pose.linearVelocity[1];
      HEAPF32[z >> 2] = pose.linearVelocity[2];

      if (sittingToStanding) {
        Module._mat4_transformDirection(sittingToStanding, x, y, z);
      }
    } else {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = 0;
    }
  },

  webvrGetAngularVelocity: function(x, y, z) {
    var sittingToStanding = lovr.WebVR.display && lovr.WebVR.display.stageParameters && lovr.WebVR.display.stageParameters.sittingToStandingTransform;
    var pose = lovr.WebVR.frameData.pose;

    if (pose.angularVelocity) {
      HEAPF32[x >> 2] = pose.angularVelocity[0];
      HEAPF32[y >> 2] = pose.angularVelocity[1];
      HEAPF32[z >> 2] = pose.angularVelocity[2];

      if (sittingToStanding) {
        Module._mat4_transformDirection(sittingToStanding, x, y, z);
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

    if (!gamepad || !gamepad.pose || !gamepad.pose.position || !gamepad.pose.orientation) {
      HEAPF32[x >> 2] = HEAPF32[y >> 2] = HEAPF32[z >> 2] = HEAPF32[angle >> 2] = HEAPF32[ax >> 2] = HEAPF32[ay >> 2] = HEAPF32[az >> 2] = 0;
      return;
    }

    HEAPF32[x >> 2] = gamepad.pose.position[0];
    HEAPF32[y >> 2] = gamepad.pose.position[1];
    HEAPF32[z >> 2] = gamepad.pose.position[2];

    if (sittingToStanding) {
      Module._mat4_transform(sittingToStanding, x, y, z);
      Module._mat4_set(lovr.WebVR.tempMatA, sittingToStanding);
      Module._mat4_rotateQuat(lovr.WebVR.tempMatA, gamepad.pose.orientation);
      Module._quat_fromMat4(lovr.WebVR.tempQuat, lovr.WebVR.tempMatA);
      Module._quat_getAngleAxis(lovr.WebVR.tempQuat, angle, ax, ay, az);
    } else {
      Module._quat_getAngleAxis(gamepad.pose.orientation, angle, ax, ay, az);
    }
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
