var webxr = {
  $state: {},

  webxr_init: function(offset, msaa) {
    if (!navigator.xr) {
      return false;
    }

    state.sessions = {};
    state.session = null;
    state.frame = null;
    state.clipNear = .1;
    state.clipFar = 1000.0;
    state.renderCallback = null;
    state.renderUserdata = null;
    state.camera = Module._malloc(264 /* sizeof(Camera) */);
    state.boundsGeometry = 0; /* NULL */
    state.boundsGeometryCount = 0;

    var mappings = {
      'oculus-touch-left': [0, 3, null, 1, null, null, null, 4, 5],
      'oculus-touch-right': [0, 3, null, 1, null, 4, 5, null, null],
      'valve-index': [0, 3, 2, 1, null, 4, null, 4, null],
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
      'generic-hand-select': [0],
    };

    function startSession(mode, options) {
      return navigator.xr.requestSession(mode, options).then(function(session) {
        var spaces = {
          'inline': ['viewer'],
          'immersive-vr': ['bounded-floor', 'local-floor']
        };

        // This is confusing but it basically keeps trying to request successive reference spaces
        // until one succeeds.  $space is a promise that resolves to a reference space
        var $space = spaces[mode].reduce(function(chain, spaceType) {
          return chain.catch(function() {
            session.spaceType = spaceType;
            return session.requestReferenceSpace(spaceType);
          });
        }, Promise.reject());

        session.inputSources = [];

        return $space.then(function(space) {
          state.session = session;
          state.sessions[mode] = session;
          session.layer = new XRWebGLLayer(session, Module.preinitializedWebGLContext);
          session.updateRenderState({
            baseLayer: session.layer,
            inlineVerticalFieldOfView: mode === 'inline' ? (67.0 * Math.PI / 180.0) : undefined
          });

          if (session.spaceType.includes('floor')) {
            session.space = space;
          } else {
            session.space = space.getOffsetReferenceSpace(new XRRigidTransform({ y: -offset }));
          }

          session.framebufferId = 0;

          if (session.layer.framebuffer) {
            session.framebufferId = GL.getNewId(GL.framebuffers);
            GL.framebuffers[session.framebufferId] = session.layer.framebuffer;
          }

          var sizeof_CanvasFlags = 16;
          var flags = Module.stackAlloc(sizeof_CanvasFlags);
          HEAPU8.fill(0, flags, flags + sizeof_CanvasFlags); // memset(&flags, 0, sizeof(CanvasFlags));
          HEAPU8[flags + 12] = mode === 'inline' ? 0 : 1; // flags.stereo
          var width = session.layer.framebufferWidth;
          var height = session.layer.framebufferHeight;
          session.canvas = Module['_lovrCanvasCreateFromHandle'](width, height, flags, session.framebufferId, 0, 0, 1, true);
          Module.stackRestore(flags);

          session.animationFrame = session.requestAnimationFrame(function onFrame(t, frame) {
            session.animationFrame = session.requestAnimationFrame(onFrame);
            session.displayTime = t;
            session.frame = frame;
            session.viewer = frame.getViewerPose(session.space);

            if (!state.renderCallback) return;

            var views = session.viewer.views;
            var stereo = views.length > 1;
            var matrices = (state.camera + 8) >> 2;
            HEAPU8[state.camera + 0] = stereo; // camera.stereo = stereo
            HEAPU32[(state.camera + 4) >> 2] = session.canvas; // camera.canvas = session.canvas
            HEAPF32.set(views[0].transform.inverse.matrix, matrices + 0);
            HEAPF32.set(views[0].projectionMatrix, matrices + 32);
            if (stereo) {
              HEAPF32.set(views[1].transform.inverse.matrix, matrices + 16);
              HEAPF32.set(views[1].projectionMatrix, matrices + 48);
            }

            Module['_lovrGraphicsSetCamera'](state.camera, true);
            Module['dynCall_vi'](state.renderCallback, state.renderUserdata);
            Module['_lovrGraphicsSetCamera'](0, false);
          });

          session.addEventListener('inputsourceschange', function(event) {
            session.inputSources.forEach(function(inputSource, i) {
              if (event.removed.includes(inputSource)) {
                session.inputSources[i] = null;
              }
            });

            event.added.forEach(function(inputSource) {
              if (inputSource.handedness === 'left') {
                session.inputSources[1 /* DEVICE_HAND_LEFT */] = inputSource;
              } else if (inputSource.handedness === 'right') {
                session.inputSources[2 /* DEVICE_HAND_RIGHT */] = inputSource;
              }

              for (var i = 0; i < inputSource.profiles.length; i++) {
                var profile = inputSource.profiles[i];

                // So far Oculus touch controllers are the only "meaningfully handed" controllers
                // If more appear then a more general approach should be used
                if (profile === 'oculus-touch') {
                   profile = profile + '-' + inputSource.handedness;
                }

                if (mappings[profile]) {
                  inputSource.mapping = mappings[profile];
                  break;
                }
              }
            });
          });

          session.addEventListener('end', function() {
            delete state.sessions[session.mode];

            if (session.canvas) {
              Module['_lovrCanvasDestroy'](session.canvas);
              Module._free(session.canvas - 4);
            }

            if (session.framebufferId) {
              GL.framebuffers[session.framebufferId].name = 0;
              GL.framebuffers[session.framebufferId] = null;
            }

            // If the immersive session ends (for any reason), switch back to the inline session
            if (session.mode === 'immersive-vr') {
              state.session = state.sessions.inline;
            }
          });

          return session;
        });
      });
    }

    Module.lovr = Module.lovr || {};
    Module.lovr.enterVR = function() {
      return startSession('immersive-vr', {
        requiredFeatures: ['local-floor'],
        optionalFeatures: ['bounded-floor']
      });
    };

    Module.lovr.exitVR = function() {
      return (state.session && state.session.mode === 'immersive-vr') ? state.session.end() : Promise.resolve();
    };

    startSession('inline');

    return true;
  },

  webxr_destroy: function() {
    for (mode in state.sessions) {
      state.sessions[mode].end();
    }

    Module._free(state.camera|0);
    Module._free(state.boundsGeometry|0);
  },

  webxr_getName: function(name, size) {
    return false;
  },

  webxr_getOriginType: function() {
    if (!state.session) return 0;
    return state.session.spaceType.includes('floor') ? 1 /* ORIGIN_FLOOR */ : 0 /* ORIGIN_HEAD */;
  },

  webxr_getDisplayTime: function() {
    return state.session ? (state.session.displayTime / 1000.0) : 0;
  },

  webxr_getDisplayDimensions: function(width, height) {
    if (state.session) {
      HEAPU32[width >> 2] = state.session.layer.framebufferWidth;
      HEAPU32[height >> 2] = state.session.layer.framebufferHeight;
    } else {
      HEAPU32[width >> 2] = 0;
      HEAPU32[height >> 2] = 0;
    }
  },

  webxr_getDisplayFrequency: function() {
    return 0.0;
  },

  webxr_getDisplayMask: function() {
    return 0; /* NULL */
  },

  webxr_getViewCount: function() {
    if (!state.session) {
      return 0;
    }

    return state.session.viewer.views.length;
  },

  webxr_getViewPose: function(index, position, orientation) {
    if (!state.session || !state.session.viewer) {
      return false;
    }

    var view = state.session.viewer.views[index];
    if (state.session.viewer.views[index]) {
      var transform = view.transform;
      HEAPF32[position >> 2 + 0] = transform.position.x;
      HEAPF32[position >> 2 + 1] = transform.position.y;
      HEAPF32[position >> 2 + 2] = transform.position.z;
      HEAPF32[position >> 2 + 3] = transform.position.w;
      HEAPF32[orientation >> 2 + 0] = transform.orientation.x;
      HEAPF32[orientation >> 2 + 1] = transform.orientation.y;
      HEAPF32[orientation >> 2 + 2] = transform.orientation.z;
      HEAPF32[orientation >> 2 + 3] = transform.orientation.w;
      return true;
    }

    return false;
  },

  webxr_getViewAngles: function(index, left, right, up, down) {
    return false; // TODO
  },

  webxr_getClipDistance: function(clipNear, clipFar) {
    HEAPF32[clipNear >> 2] = state.clipNear;
    HEAPF32[clipFar >> 2] = state.clipFar;
  },

  webxr_setClipDistance: function(clipNear, clipFar) {
    if (!state.session) return;
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
    if (!state.session || !(state.session.space instanceof XRBoundedReferenceSpace)) {
      return 0; /* NULL */
    }

    var points = state.session.space.boundsGeometry;

    if (state.boundsGeometryCount < points.length) {
      Module._free(state.boundsGeometry|0);
      state.boundsGeometry = Module._malloc(4 * 4 * points.length);
      if (state.boundsGeometry === 0) {
        return state.boundsGeometry;
      }
    }

    for (var i = 0; i < points.length; i++) {
      HEAPF32.set(points[i], state.boundsGeometry + 4 * i);
    }

    return state.boundsGeometry;
  },

  webxr_getPose: function(device, position, orientation) {
    if (!state.session || !state.session.viewer) return false;

    if (device === 0 /* DEVICE_HEAD */) {
      var transform = state.session.viewer.transform;
      HEAPF32[position >> 2 + 0] = transform.position.x;
      HEAPF32[position >> 2 + 1] = transform.position.y;
      HEAPF32[position >> 2 + 2] = transform.position.z;
      HEAPF32[position >> 2 + 3] = transform.position.w;
      HEAPF32[orientation >> 2 + 0] = transform.orientation.x;
      HEAPF32[orientation >> 2 + 1] = transform.orientation.y;
      HEAPF32[orientation >> 2 + 2] = transform.orientation.z;
      HEAPF32[orientation >> 2 + 3] = transform.orientation.w;
      return true;
    }

    if (state.session.inputSources[device]) {
      var inputSource = state.session.inputSources[device];
      var space = inputSource.gripSpace || inputSource.targetRaySpace;
      var transform = state.session.frame.getPose(space, state.session.space).transform;
      HEAPF32[position >> 2 + 0] = transform.position.x;
      HEAPF32[position >> 2 + 1] = transform.position.y;
      HEAPF32[position >> 2 + 2] = transform.position.z;
      HEAPF32[position >> 2 + 3] = transform.position.w;
      HEAPF32[orientation >> 2 + 0] = transform.orientation.x;
      HEAPF32[orientation >> 2 + 1] = transform.orientation.y;
      HEAPF32[orientation >> 2 + 2] = transform.orientation.z;
      HEAPF32[orientation >> 2 + 3] = transform.orientation.w;
      return true;
    }

    return false;
  },

  webxr_getVelocity: function(device, velocity, angularVelocity) {
    return false; // Unsupported, see #619
  },

  webxr_isDown: function(device, button, down, changed) {
    if (!state.session) return false;

    var inputSource = state.session.inputSources[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.mapping || !inputSource.mapping[button]) {
      return false;
    }

    HEAPU32[down >> 2] = inputSource.gamepad.buttons[inputSource.mapping[button]].pressed ? 1 : 0;
    HEAPU32[changed >> 2] = 0; // TODO
    return true;
  },

  webxr_isTouched: function(device, button, touched) {
    if (!state.session) return false;

    var inputSource = state.session.inputSources[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.mapping || !inputSource.mapping[button]) {
      return false;
    }

    HEAPU32[touched >> 2] = inputSource.gamepad.buttons[inputSource.mapping[button]].touched ? 1 : 0;
    return true;
  },

  webxr_getAxis: function(device, axis, value) {
    if (!state.session) return false;

    var inputSource = state.session.inputSources[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.mapping) {
      return false;
    }

    switch (axis) {
      // These 1D axes are queried as buttons in the Gamepad API
      // The DeviceAxis enumerants match the DeviceButton ones, so they're interchangeable
      case 0: /* AXIS_TRIGGER */
      case 3: /* AXIS_GRIP */
        if (inputSource.mapping[axis]) {
          HEAPF32[value >> 2] = inputSource.gamepad.buttons[inputSource.mapping[axis]].value;
          return true;
        }
        return false;

      case 1: /* AXIS_THUMBSTICK */
        HEAPF32[value >> 2 + 0] = inputSource.gamepad.axes[2];
        HEAPF32[value >> 2 + 1] = inputSource.gamepad.axes[3];
        return true;

      case 2: /* AXIS_TOUCHPAD */
        HEAPF32[value >> 2 + 0] = inputSource.gamepad.axes[0];
        HEAPF32[value >> 2 + 1] = inputSource.gamepad.axes[1];
        return true;

      default:
        return false;
    }
  },

  webxr_vibrate: function(device, strength, duration, frequency) {
    if (!state.session) return false;

    var inputSource = state.session.inputSources[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.gamepad.hapticActuators || !inputSource.gamepad.hapticActuators[0]) {
      return false;
    }

    // Not technically an official WebXR feature, but widely supported
    inputSource.gamepad.hapticActuators[0].pulse(strength, duration * 1000);
    return true;
  },

  webxr_newModelData: function(device, animated) {
    return 0; /* NULL */
  },

  webxr_animate: function(device, model) {
    return false;
  },

  webxr_renderTo: function(callback, userdata) {
    state.renderCallback = callback;
    state.renderUserdata = userdata;
  },

  webxr_update: function(dt) {
    //
  }
};

autoAddDeps(webxr, '$state');
mergeInto(LibraryManager.library, webxr);
