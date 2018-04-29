var LibraryLOVR = {
  $lovr: {
    WebVR: {
      ORIGIN_HEAD: 0,
      ORIGIN_FLOOR: 1,

      initialized: false,
      mirrored: true,
      renderCallback: null,
      renderUserdata: null,
      frameData: null,
      width: 0,
      height: 0,

      init: function() {
        if (lovr.WebVR.initialized) {
          return;
        }

        var display, canvas;
        lovr.WebVR.initialized = true;
        lovr.WebVR.canvas = canvas = Module['canvas'];
        lovr.WebVR.width = canvas.width;
        lovr.WebVR.height = canvas.height;
        lovr.WebVR.frameData = new VRFrameData();

        !function findDisplay() {
          navigator.getVRDisplays && navigator.getVRDisplays().then(function(displays) {
            lovr.WebVR.display = display = displays[0];
          });
        }();

        !function onResize() {
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
        }();

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
      }
    }
  },

  webvrInit: function() {
    lovr.WebVR.init();
  },

  webvrGetOriginType: function() {
    return WebVR.display && WebVR.display.stageParameters ? ORIGIN_FLOOR : ORIGIN_HEAD;
  },

  webvrIsMirrored: function() {
    return lovr.WebVR.mirrored;
  },

  webvrSetMirrored: function(mirror) {
    lovr.WebVR.mirrored = mirror;
  },

  webvrGetDisplayDimensions: function(width, height) {
    Module.setValue(width, lovr.WebVR.width, 'i32');
    Module.setValue(height, lovr.WebVR.height, 'i32');
  },

  webvrGetClipDistance: function(clipNear, clipFar) {
    Module.setValue(clipNear, lovr.WebVR.display ? lovr.WebVR.display.depthNear : 0, 'float');
    Module.setValue(clipFar, lovr.WebVR.display ? lovr.WebVR.display.depthFar : 0, 'float');
  },

  webvrSetClipDistance: function(clipNear, clipFar) {
    if (lovr.WebVR.display) {
      lovr.WebVR.display.depthNear = clipNear;
      lovr.WebVR.display.depthFar = clipFar;
    }
  },

  webvrGetBoundsDimensions: function(width, depth) {
    Module.setValue(width, lovr.WebVR.display && lovr.WebVR.display.stageParameters ? lovr.WebVR.display.stageParameters.sizeX : 0);
    Module.setValue(depth, lovr.WebVR.display && lovr.WebVR.display.stageParameters ? lovr.WebVR.display.stageParameters.sizeZ : 0);
  },

  webvrRenderTo: function(callback, userdata) {
    lovr.WebVR.renderCallback = callback;
    lovr.WebVR.renderUserdata = userdata;
  }
};

autoAddDeps(LibraryLOVR, '$lovr');
mergeInto(LibraryManager.library, LibraryLOVR);
