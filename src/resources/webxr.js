var webxr = {
  $state: {},

  webxr_init: function(offset, msaa) {
    if (!navigator.xr) {
      return false;
    }

    state.layer = null;
    state.frame = null;
    state.canvas = null;
    state.camera = null;
    state.clipNear = .1;
    state.clipFar = 1000.0;
    state.referenceSpaceType = null;
    state.renderCallback = null;
    state.renderUserdata = null;
    state.animationFrame = null;
    state.displayTime = null;

    navigator.xr.requestSession('inline').then(function(session) {
      state.referenceSpaceType = 'viewer';
      session.requestReferenceSpace(state.referenceSpaceType).then(function(referenceSpace) {
        state.session = session;
        state.layer = new XRWebGLLayer(session, Module.preinitializedWebGLContext);

        var framebuffer = 0;

        if (state.layer.framebuffer) {
          framebuffer = GL.getNewId(GL.framebuffers);
          GL.framebuffers[framebuffer] = state.layer.framebuffer;
        }

        var sizeof_CanvasFlags = 16;
        var flags = Module.stackAlloc(sizeof_CanvasFlags);
        HEAPU8.fill(0, flags, flags + sizeof_CanvasFlags); // memset(&flags, 0, sizeof(CanvasFlags));
        var width = state.layer.framebufferWidth;
        var height = state.layer.framebufferHeight;
        state.canvas = Module['_lovrCanvasCreateFromHandle'](width, height, flags, framebuffer, 0, 0, 1, true);
        Module.stackRestore(flags);

        var sizeof_Camera = 264;
        state.camera = Module._malloc(sizeof_Camera);
        HEAPU32[(state.camera + 4) >> 2] = state.canvas; // state.camera.canvas = state.canvas

        session.updateRenderState({
          baseLayer: state.layer
        });

        state.animationFrame = session.requestAnimationFrame(function onFrame(t, frame) {
          state.animationFrame = session.requestAnimationFrame(onFrame);
          state.displayTime = t;
          state.frame = frame;

          var views = frame.getViewerPose(referenceSpace).views;

          var stereo = views.length > 1;
          HEAPU8[state.camera + 0] = stereo; // camera.stereo = stereo

          var matrices = (state.camera + 8) >> 2;
          HEAPF32.set(views[0].transform.inverse.matrix, matrices + 0);
          HEAPF32.set(views[0].projectionMatrix, matrices + 32);

          if (stereo) {
            HEAPF32.set(views[1].transform.inverse.matrix, matrices + 16);
            HEAPF32.set(views[1].projectionMatrix, matrices + 48);
          }

          Module['_lovrGraphicsSetCamera'](state.camera, true);

          if (state.renderCallback) {
            Module['dynCall_vi'](state.renderCallback, state.renderUserdata);
          }

          Module['_lovrGraphicsSetCamera'](0, false);
        });
      });
    });

    return true;
  },

  webxr_destroy: function() {
    function cleanup() {
      // TODO release canvas
      Module._free(state.camera|0);
    }

    if (state.session) {
      state.session.cancelAnimationFrame(state.animationFrame);
      state.session.end().then(cleanup);
    } else {
      cleanup();
    }
  },

  webxr_getName: function(name, size) {
    return false;
  },

  webxr_getOriginType: function() {
    if (state.referenceSpaceType === 'local-floor' || state.referenceSpaceType === 'bounded-floor') {
      return 1; /* ORIGIN_FLOOR */
    }

    return 0; /* ORIGIN_HEAD */
  },

  webxr_getDisplayTime: function() {
    return state.displayTime;
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
    if (!state.frame) {
      return 0;
    }

    return getViewerPose(state.frame).views.length;
  },

  webxr_getViewPose: function(index, position, orientation) {
    if (!state.frame) {
      return false;
    }
    var view = getViewerPose(state.frame).views[index];
    if (view) {
      HEAPF32[position >> 2 + 0] = view.transform.position.x;
      HEAPF32[position >> 2 + 1] = view.transform.position.y;
      HEAPF32[position >> 2 + 2] = view.transform.position.z;
      HEAPF32[position >> 2 + 3] = view.transform.position.w;
      HEAPF32[orientation >> 2 + 0] = view.transform.orientation.x;
      HEAPF32[orientation >> 2 + 1] = view.transform.orientation.y;
      HEAPF32[orientation >> 2 + 2] = view.transform.orientation.z;
      HEAPF32[orientation >> 2 + 3] = view.transform.orientation.w;
      return true;
    }
    return false;
  },

  webxr_getViewAngles: function(index, left, right, up, down) {
    return false;
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
    HEAPF32[width >> 2] = 0.0;
    HEAPF32[depth >> 2] = 0.0;
  },

  webxr_getBoundsGeometry: function(count) {
    return 0; /* NULL */ // TODO
  },

  webxr_getPose: function(device, position, orientation) {
    return false; // TODO
  },

  webxr_getVelocity: function(device, velocity, angularVelocity) {
    return false; // TODO
  },

  webxr_isDown: function(device, button, down, changed) {
    return false; // TODO
  },

  webxr_isTouched: function(device, button, touched) {
    return false; // TODO
  },

  webxr_getAxis: function(device, axis, value) {
    return false; // TODO
  },

  webxr_vibrate: function(device, strength, duration, frequency) {
    return false; // TODO
  },

  webxr_newModelData: function(device) {
    return 0; /* NULL */ // TODO
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
