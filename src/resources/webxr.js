var webxr = {
  $state: {},

  // Derived from github:immersive-web/webxr-input-profiles#5052b76
  $buttons: {
    'oculus-touch': {
      left: [0, 3, null, 1, null, null, null, 4, 5],
      right: [0, 3, null, 1, null, 4, 5, null, null],
    },
    'valve-index': [0, 3, 2, 1, null, 4, null],
    'microsoft-mixed-reality': [0, 3, 2, 1],
    'htc-vive': [0, null, 2, 1],
    'generic-trigger': [0],
    'generic-trigger-touchpad': [0, null, 2],
    'generic-trigger-thumbstick': [0, 3],
    'generic-trigger-touchpad-thumbstick': [0, 3, 2],
    'generic-trigger-squeeze': [0, null, null, 1],
    'generic-trigger-squeeze-touchpad': [0, null, 2, 1],
    'generic-trigger-squeeze-touchpad-thumbstick': [0, 3, 2, 1],
    'generic-trigger-squeeze-thumbstick': [0, 3, null, 1],
    'generic-hand-select': [0]
  },
  $axes: {
    'oculus-touch': { touchpad: false, thumbstick: true },
    'valve-index': { touchpad: true, thumbstick: true },
    'microsoft-mixed-reality': { touchpad: true, thumbstick: true },
    'htc-vive': { touchpad: true, thumbstick: false },
    'generic-trigger': { touchpad: false, thumbstick: false },
    'generic-trigger-touchpad': { touchpad: true, thumbstick: false },
    'generic-trigger-thumbstick': { touchpad: false, thumbstick: true },
    'generic-trigger-touchpad-thumbstick': { touchpad: true, thumbstick: true },
    'generic-trigger-squeeze': { touchpad: false, thumbstick: false },
    'generic-trigger-squeeze-touchpad': { touchpad: true, thumbstick: false },
    'generic-trigger-squeeze-touchpad-thumbstick': { touchpad: true, thumbstick: true },
    'generic-trigger-squeeze-thumbstick': { touchpad: false, thumbstick: true },
    'generic-hand-select': { touchpad: false, thumbstick: false },
  },

  $writePose: function(transform, position, orientation) {
    HEAPF32[(position >> 2) + 0] = transform.position.x;
    HEAPF32[(position >> 2) + 1] = transform.position.y;
    HEAPF32[(position >> 2) + 2] = transform.position.z;
    HEAPF32[(position >> 2) + 3] = transform.position.w;
    HEAPF32[(orientation >> 2) + 0] = transform.orientation.x;
    HEAPF32[(orientation >> 2) + 1] = transform.orientation.y;
    HEAPF32[(orientation >> 2) + 2] = transform.orientation.z;
    HEAPF32[(orientation >> 2) + 3] = transform.orientation.w;
  },

  webxr_init__deps: ['$buttons', '$axes'],
  webxr_init: function(supersample, offset, msaa) {
    if (!navigator.xr) {
      return false;
    }

    Module.lovr = Module.lovr || {};
    Module.lovr.enterVR = function() {
      var options = {
        requiredFeatures: ['local-floor'],
        optionalFeatures: ['bounded-floor', 'hand-tracking']
      };

      return navigator.xr.requestSession('immersive-vr', options).then(function(session) {
        var space = session.requestReferenceSpace('bounded-floor').catch(function() {
          return session.requestReferenceSpace('local-floor');
        });

        return Promise.all([space, Module.ctx.makeXRCompatible()]).then(function(data) {
          state.session = session;
          state.space = data[0];
          state.clipNear = .1;
          state.clipFar = 1000.0;
          state.boundsGeometry = 0; /* NULL */
          state.boundsGeometryCount = 0;
          state.layer = new XRWebGLLayer(session, Module.ctx);
          state.session.updateRenderState({ baseLayer: state.layer });
          state.fbo = GL.getNewId(GL.framebuffers);
          GL.framebuffers[state.fbo] = state.layer.framebuffer;

          // Canvas
          var sizeof_CanvasFlags = 16;
          var width = state.layer.framebufferWidth;
          var height = state.layer.framebufferHeight;
          var flags = Module.stackAlloc(sizeof_CanvasFlags);
          HEAPU8.fill(0, flags, flags + sizeof_CanvasFlags); // memset(&flags, 0, sizeof(CanvasFlags));
          HEAPU8[flags + 12] = 1; // flags.stereo
          state.canvas = Module['_lovrCanvasCreateFromHandle'](width, height, flags, state.fbo, 0, 0, 1, true);
          Module.stackRestore(flags);

          state.hands = [];
          state.lastButtonState = [];
          session.addEventListener('inputsourceschange', function(event) {
            state.hands.splice(0, state.hands.length);
            session.inputSources.forEach(function(inputSource) {
              state.hands[({ left: 1, right: 2 })[inputSource.handedness]] = inputSource;

              var profile = inputSource.profiles.find(function(profile) { return buttons[profile]; });

              if (!inputSource.gamepad || !profile) {
                inputSource.buttons = [];
                inputSource.axes = {};
                return;
              }

              inputSource.axes = axes[profile];
              var mapping = buttons[profile][inputSource.handedness] || buttons[profile] || [];
              inputSource.buttons = mapping.map(function(buttonIndex) {
                return inputSource.gamepad.buttons[buttonIndex];
              });
            });
          });

          session.addEventListener('end', function() {
            Module._free(state.boundsGeometry|0);

            if (state.canvas) {
              Module['_lovrCanvasDestroy'](state.canvas);
              Module._free(state.canvas - 4);
            }

            if (state.fbo) {
              GL.framebuffers[state.fbo].name = 0;
              GL.framebuffers[state.fbo] = null;
            }

            Browser.mainLoop.pause();
            Module['_webxr_detach']();
            Browser.requestAnimationFrame = window.requestAnimationFrame.bind(window);
            Browser.mainLoop.resume();
            state.session = null;
          });

          // Trick emscripten into using the session's requestAnimationFrame, also make ourselves
          // the headset driver using webxr_attach.  These are both undone when the session ends
          // so that the mouse/keyboard driver can be used when the session is inactive.
          Browser.mainLoop.pause();
          Module['_webxr_attach']();
          Browser.requestAnimationFrame = function(fn) {
            return session.requestAnimationFrame(function(t, frame) {
              state.displayTime = t;
              state.frame = frame;
              state.viewer = state.frame.getViewerPose(state.space);
              fn();
              state.hands.forEach(function(inputSource, i) {
                state.lastButtonState[i] = inputSource && inputSource.buttons.map(function(button) {
                  return button && button.pressed;
                });
              });
            });
          };
          Browser.mainLoop.resume();
          return session;
        });
      });
    };

    Module.lovr.exitVR = function() {
      return state.session ? state.session.end() : Promise.resolve();
    };

    // WebXR is not set as the display driver immediately, it uses webxr_attach to make itself the
    // headset driver when a session starts.
    return false;
  },

  webxr_destroy: function() {
    if (state.session) {
      state.session.end();
    }
  },

  webxr_getName: function(name, size) {
    return false;
  },

  webxr_getOriginType: function() {
    return 1; /* ORIGIN_FLOOR */
  },

  webxr_getDisplayTime: function() {
    return state.displayTime / 1000.0;
  },

  webxr_getDisplayDimensions: function(width, height) {
    HEAPU32[width >> 2] = state.layer.framebufferWidth;
    HEAPU32[height >> 2] = state.layer.framebufferHeight;
  },

  webxr_getDisplayFrequency: function() {
    return 0.0;
  },

  webxr_getDisplayMask: function() {
    return 0; /* NULL */
  },

  webxr_getViewCount: function() {
    return state.viewer ? state.viewer.views.length : 0;
  },

  webxr_getViewPose__deps: ['$writePose'],
  webxr_getViewPose: function(index, position, orientation) {
    var view = state.viewer && state.viewer.views[index];
    view && writePose(view.transform, position, orientation);
    return !!view;
  },

  webxr_getViewAngles: function(index, left, right, up, down) {
    return false; // TODO
  },

  webxr_getClipDistance: function(clipNear, clipFar) {
    HEAPF32[clipNear >> 2] = state.clipNear;
    HEAPF32[clipFar >> 2] = state.clipFar;
  },

  webxr_setClipDistance: function(clipNear, clipFar) {
    state.clipNear = clipNear;
    state.clipFar = clipFar;
    state.session.updateRenderState({
      clipNear: clipNear,
      clipFar: clipFar
    });
  },

  webxr_getBoundsDimensions: function(width, depth) {
    HEAPF32[width >> 2] = 0.0; // Unsupported, see #557
    HEAPF32[depth >> 2] = 0.0;
  },

  webxr_getBoundsGeometry: function(count) {
    if (!(state.space instanceof XRBoundedReferenceSpace)) {
      return 0; /* NULL */
    }

    var points = state.space.boundsGeometry;

    if (state.boundsGeometryCount < points.length) {
      Module._free(state.boundsGeometry|0);
      state.boundsGeometryCount = points.length;
      state.boundsGeometry = Module._malloc(4 * 4 * state.boundsGeometryCount);
      if (state.boundsGeometry === 0) {
        return 0; /* NULL */
      }
    }

    for (var i = 0; i < points.length; i++) {
      HEAPF32[(state.boundsGeometry >> 2) + 4 * i + 0] = points[i].x;
      HEAPF32[(state.boundsGeometry >> 2) + 4 * i + 1] = points[i].y;
      HEAPF32[(state.boundsGeometry >> 2) + 4 * i + 2] = points[i].z;
      HEAPF32[(state.boundsGeometry >> 2) + 4 * i + 3] = points[i].w;
    }

    HEAPU32[count >> 2] = points.length;
    return state.boundsGeometry;
  },

  webxr_getPose__deps: ['$writePose'],
  webxr_getPose: function(device, position, orientation) {
    if (device === 0 /* DEVICE_HEAD */) {
      state.viewer && writePose(state.viewer.transform, position, orientation);
      return !!state.viewer;
    }

    if (state.hands[device]) {
      var space = state.hands[device].gripSpace || state.hands[device].targetRaySpace;
      var pose = state.frame.getPose(space, state.space);
      pose && writePose(pose.transform, position, orientation);
      return !!pose;
    }

    return false;
  },

  webxr_getVelocity: function(device, velocity, angularVelocity) {
    return false; // Unsupported, see immersive-web/webxr#619
  },

  webxr_isDown: function(device, button, down, changed) {
    var b = state.hands[device] && state.hands[device].buttons[button];
    HEAPU8[down] = b && b.pressed;
    HEAPU8[changed] = b && (state.lastButtonState[device][button] ^ b.pressed);
    return !!b;
  },

  webxr_isTouched: function(device, button, touched) {
    var b = state.hands[device] && state.hands[device].buttons[button];
    HEAPU8[touched] = b && b.touched;
    return !!b;
  },

  webxr_getAxis: function(device, axis, value) {
    var hand = state.hands[device];

    if (!hand || !hand.gamepad) {
      return false;
    }

    switch (axis) {
      case 0: /* AXIS_TRIGGER */
      case 3: /* AXIS_GRIP */
        HEAPF32[value >> 2] = hand.buttons[axis] && hand.buttons[axis].value;
        return !!hand.buttons[axis];

      case 1: /* AXIS_THUMBSTICK */
        HEAPF32[(value >> 2) + 0] = hand.gamepad.axes[2];
        HEAPF32[(value >> 2) + 1] = hand.gamepad.axes[3];
        return hand.axes.thumbstick;

      case 2: /* AXIS_TOUCHPAD */
        HEAPF32[(value >> 2) + 0] = hand.gamepad.axes[0];
        HEAPF32[(value >> 2) + 1] = hand.gamepad.axes[1];
        return hand.axes.touchpad;

      default: return false;
    }
  },

  webxr_getSkeleton__deps: ['$writePose'],
  webxr_getSkeleton: function(device, poses) {
    var inputSource = state.hands[device];

    if (!inputSource || !inputSource.hand) {
      return false;
    }

    // WebXR does not have a palm joint but the skeleton otherwise matches up
    HEAPF32.fill(0, poses >> 2, (poses >> 2) + 8 * 26 /* HAND_JOINT_COUNT */);
    poses += 32;

    for (var i = 0; i < 25; i++) {
      if (inputSource.hand[i]) {
        var pose = state.frame.getJointPose(inputSource.hand[i], state.space);
        if (pose) {
          var position = poses + i * 32;
          writePose(pose.transform, position, position + 16);
        }
      }
    }

    return true;
  },

  // Not an official WebXR feature, but widely supported
  webxr_vibrate: function(device, strength, duration, frequency) {
    var hand = state.deviceMap[device];
    var actuator = hand && hand.gamepad && hand.gamepad.hapticActuators && hand.gamepad.hapticActuators[0];
    actuator && actuator.pulse(strength, duration * 1000);
    return !!actuator;
  },

  webxr_newModelData: function(device, animated) {
    return 0; /* NULL */
  },

  webxr_animate: function(device, model) {
    return false;
  },

  webxr_renderTo: function(callback, userdata) {
    var matrix = Module.stackAlloc(16 * 4);
    if (state.viewer) {
      var views = state.viewer.views;
      HEAPF32.set(views[0].transform.inverse.matrix, matrix >> 2);
      Module._lovrGraphicsSetViewMatrix(0, matrix);
      HEAPF32.set(views[1].transform.inverse.matrix, matrix >> 2);
      Module._lovrGraphicsSetViewMatrix(1, matrix);
      HEAPF32.set(views[0].projectionMatrix, matrix >> 2);
      Module._lovrGraphicsSetProjection(0, matrix);
      HEAPF32.set(views[1].projectionMatrix, matrix >> 2);
      Module._lovrGraphicsSetProjection(1, matrix);
    } else {
      HEAPF32.set([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], matrix >> 2);
      Module._lovrGraphicsSetViewMatrix(0, matrix);
      Module._lovrGraphicsSetViewMatrix(1, matrix);
      // TODO projection?
    }
    Module.stackRestore(matrix);
    Module._lovrGraphicsSetBackbuffer(state.canvas, true, true);
    Module.dynCall_vi(callback, userdata);
    Module._lovrGraphicsSetBackbuffer(0, false, false);
  },

  webxr_update: function(dt) {
    //
  }
};

autoAddDeps(webxr, '$state');
mergeInto(LibraryManager.library, webxr);
