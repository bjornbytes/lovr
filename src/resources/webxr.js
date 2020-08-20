var webxr = {
  $state: {},

  webxr_init: function(offset, msaa) {
    if (!navigator.xr) {
      return false;
    }

    state.session = null;
    state.clipNear = .1;
    state.clipFar = 1000.0;
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
      'generic-hand-select': [0]
    };

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

          session.space = data[0];
          session.layer = new XRWebGLLayer(session, Module.ctx);
          session.updateRenderState({ baseLayer: session.layer });
          session.framebufferId = GL.getNewId(GL.framebuffers);
          GL.framebuffers[session.framebufferId] = session.layer.framebuffer;

          // Canvas
          var sizeof_Camera = 264;
          var sizeof_CanvasFlags = 16;
          var width = session.layer.framebufferWidth;
          var height = session.layer.framebufferHeight;
          var flags = Module.stackAlloc(sizeof_CanvasFlags);
          HEAPU8.fill(0, flags, flags + sizeof_CanvasFlags); // memset(&flags, 0, sizeof(CanvasFlags));
          HEAPU8[flags + 12] = 1; // flags.stereo
          session.canvas = Module['_lovrCanvasCreateFromHandle'](width, height, flags, session.framebufferId, 0, 0, 1, true);
          session.camera = Module._malloc(sizeof_Camera);
          Module.stackRestore(flags);

          session.deviceMap = [];
          session.addEventListener('inputsourceschange', function(event) {
            session.deviceMap.splice(0, session.deviceMap.length);
            for (var i = 0; i < session.inputSources.length; i++) {
              var hands = { left: 1, right: 2 };
              session.deviceMap[hands[inputSource.handedness]] = inputSource;

              for (var i = 0; i < inputSource.profiles.length; i++) {
                var profile = inputSource.profiles[i];

                // So far Oculus touch controllers are the only "meaningfully handed" controllers
                // If more appear, a more general approach should be used
                if (profile === 'oculus-touch') {
                   profile = profile + '-' + inputSource.handedness;
                }

                if (mappings[profile]) {
                  inputSource.mapping = mappings[profile];
                  break;
                }
              }
            }
          });

          session.addEventListener('end', function() {
            if (session.canvas) {
              Module['_lovrCanvasDestroy'](session.canvas);
              Module._free(session.canvas - 4);
              Module._free(session.camera|0);
            }

            if (session.framebufferId) {
              GL.framebuffers[session.framebufferId].name = 0;
              GL.framebuffers[session.framebufferId] = null;
            }

            Browser.mainLoop.pause();
            Module['_webxr_detach']();
            Browser.requestAnimationFrame = window.requestAnimationFrame.bind(window);
            Browser.mainLoop.resume();
            state.session = null;
          });

          // Trick emscripten into using the session's requestAnimationFrame
          Browser.mainLoop.pause();
          Module['_webxr_attach']();
          Browser.requestAnimationFrame = function(fn) {
            return session.requestAnimationFrame(function(t, frame) {
              session.displayTime = t;
              session.frame = frame;
              session.viewer = session.frame.getViewerPose(session.space);
              fn();
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

    // WebXR is not set as the display driver at initialization time, so that the VR simulator can
    // be used on the DOM's canvas before the session starts.  When the session starts, it uses the
    // webxr_attach function to make itself the display driver for the duration of the session.
    return false;
  },

  webxr_destroy: function() {
    if (state.session) {
      state.session.end();
    }

    Module._free(state.boundsGeometry|0);
  },

  webxr_getName: function(name, size) {
    return false;
  },

  webxr_getOriginType: function() {
    return 1; /* ORIGIN_FLOOR */
  },

  webxr_getDisplayTime: function() {
    return state.session.displayTime / 1000.0;
  },

  webxr_getDisplayDimensions: function(width, height) {
    HEAPU32[width >> 2] = state.session.layer.framebufferWidth;
    HEAPU32[height >> 2] = state.session.layer.framebufferHeight;
  },

  webxr_getDisplayFrequency: function() {
    return 0.0;
  },

  webxr_getDisplayMask: function() {
    return 0; /* NULL */
  },

  webxr_getViewCount: function() {
    return state.session.viewer.views.length;
  },

  webxr_getViewPose: function(index, position, orientation) {
    var view = state.session.viewer.views[index];
    if (view) {
      var transform = view.transform;
      HEAPF32[(position >> 2) + 0] = transform.position.x;
      HEAPF32[(position >> 2) + 1] = transform.position.y;
      HEAPF32[(position >> 2) + 2] = transform.position.z;
      HEAPF32[(position >> 2) + 3] = transform.position.w;
      HEAPF32[(orientation >> 2) + 0] = transform.orientation.x;
      HEAPF32[(orientation >> 2) + 1] = transform.orientation.y;
      HEAPF32[(orientation >> 2) + 2] = transform.orientation.z;
      HEAPF32[(orientation >> 2) + 3] = transform.orientation.w;
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
    if (!(state.session.space instanceof XRBoundedReferenceSpace)) {
      return 0; /* NULL */
    }

    var points = state.session.space.boundsGeometry;

    if (state.boundsGeometryCount < points.length) {
      Module._free(state.boundsGeometry|0);
      state.boundsGeometryCount = points.length;
      state.boundsGeometry = Module._malloc(4 * 4 * state.boundsGeometryCount);
      if (state.boundsGeometry === 0) {
        return state.boundsGeometry;
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

  webxr_getPose: function(device, position, orientation) {
    if (device === 0 /* DEVICE_HEAD */) {
      var transform = state.session.viewer.transform;
      HEAPF32[(position >> 2) + 0] = transform.position.x;
      HEAPF32[(position >> 2) + 1] = transform.position.y;
      HEAPF32[(position >> 2) + 2] = transform.position.z;
      HEAPF32[(position >> 2) + 3] = transform.position.w;
      HEAPF32[(orientation >> 2) + 0] = transform.orientation.x;
      HEAPF32[(orientation >> 2) + 1] = transform.orientation.y;
      HEAPF32[(orientation >> 2) + 2] = transform.orientation.z;
      HEAPF32[(orientation >> 2) + 3] = transform.orientation.w;
      return true;
    }

    if (state.session.deviceMap[device]) {
      var inputSource = state.session.deviceMap[device];
      var space = inputSource.gripSpace || inputSource.targetRaySpace;
      var pose = state.session.frame.getPose(space, state.session.space);
      if (pose) {
        HEAPF32[(position >> 2) + 0] = pose.transform.position.x;
        HEAPF32[(position >> 2) + 1] = pose.transform.position.y;
        HEAPF32[(position >> 2) + 2] = pose.transform.position.z;
        HEAPF32[(position >> 2) + 3] = pose.transform.position.w;
        HEAPF32[(orientation >> 2) + 0] = pose.transform.orientation.x;
        HEAPF32[(orientation >> 2) + 1] = pose.transform.orientation.y;
        HEAPF32[(orientation >> 2) + 2] = pose.transform.orientation.z;
        HEAPF32[(orientation >> 2) + 3] = pose.transform.orientation.w;
        return true;
      }
    }

    return false;
  },

  webxr_getVelocity: function(device, velocity, angularVelocity) {
    return false; // Unsupported, see immersive-web/webxr#619
  },

  webxr_isDown: function(device, button, down, changed) {
    var inputSource = state.session.deviceMap[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.mapping) {
      return false;
    }

    HEAPU32[down >> 2] = inputSource.gamepad.buttons[inputSource.mapping[button]].pressed ? 1 : 0;
    HEAPU32[changed >> 2] = 0; // TODO
    return true;
  },

  webxr_isTouched: function(device, button, touched) {
    var inputSource = state.session.deviceMap[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.mapping) {
      return false;
    }

    HEAPU32[touched >> 2] = inputSource.gamepad.buttons[inputSource.mapping[button]].touched ? 1 : 0;
    return true;
  },

  webxr_getAxis: function(device, axis, value) {
    var inputSource = state.session.deviceMap[device];
    if (!inputSource || !inputSource.gamepad || !inputSource.mapping) {
      return false;
    }

    switch (axis) {
      // These 1D axes are queried as buttons in the Gamepad API
      // The DeviceAxis enumerants match the DeviceButton ones, so they're interchangeable
      case 0: /* AXIS_TRIGGER */
      case 3: /* AXIS_GRIP */
        HEAPF32[value >> 2] = inputSource.gamepad.buttons[inputSource.mapping[axis]].value;
        return true;

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

  webxr_getSkeleton: function(device, poses) {
    var inputSource = state.session.deviceMap[device];
    if (!inputSource || !inputSource.hand) {
      return false;
    }

    // There are 26 total hand joints, each with an 8-float pose
    HEAPF32.fill(0, poses >> 2, (poses >> 2) + 26 * 8);

    // WebXR has 25 joints, it's missing JOINT_PALM but otherwise it matches up perfectly
    for (var i = 0; i < 25; i++) {
      if (inputSource.hand[i]) {
        var jointPose = state.session.frame.getJointPose(inputSource.hand[i], state.session.space);
        if (jointPose) {
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 0] = jointPose.transform.position.x;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 1] = jointPose.transform.position.y;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 2] = jointPose.transform.position.z;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 3] = jointPose.transform.position.w;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 4] = jointPose.transform.orientation.x;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 5] = jointPose.transform.orientation.y;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 6] = jointPose.transform.orientation.z;
          HEAPF32[(poses >> 2) + (i + 1) * 8 + 7] = jointPose.transform.orientation.w;
        }
      }
    }

    return true;
  },

  webxr_vibrate: function(device, strength, duration, frequency) {
    var inputSource = state.session.deviceMap[device];
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
    var views = state.session.viewer.views;
    var camera = state.session.camera;
    var stereo = views.length > 1;
    var matrices = (camera + 8) >> 2;
    HEAPU8[camera + 0] = stereo; // camera.stereo = stereo
    HEAPU32[(camera + 4) >> 2] = state.session.canvas; // camera.canvas = session.canvas
    HEAPF32.set(views[0].transform.inverse.matrix, matrices + 0);
    HEAPF32.set(views[0].projectionMatrix, matrices + 32);
    if (stereo) {
      HEAPF32.set(views[1].transform.inverse.matrix, matrices + 16);
      HEAPF32.set(views[1].projectionMatrix, matrices + 48);
    }

    Module['_lovrGraphicsSetCamera'](camera, true);
    Module['dynCall_vi'](callback, userdata);
    Module['_lovrGraphicsSetCamera'](0, false);
  },

  webxr_update: function(dt) {
    //
  }
};

autoAddDeps(webxr, '$state');
mergeInto(LibraryManager.library, webxr);
