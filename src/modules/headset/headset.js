var headset = {
  $state: {},

  lovrHeadsetInit(config) {
    state.poses = [];
    state.lastButtons = [];
    state.buttons = [];

    // Per-device simulator state
    for (var i = 0; i < 32; i++) {
      state.poses[i] = {
        position: [0, 0, 0],
        orientation: [0, 0, 0, 1]
      };

      state.lastButtons[i] = 0;
      state.buttons[i] = 0;
    }

    state.poses[0].position[1] = 1.7;

    state.clipNear = .01;
    state.clipFar = 0;

    return true;
  },

  lovrHeadsetDestroy() {
    //
  },

  lovrHeadsetConnect() {
    return true;
  },

  lovrHeadsetIsConnected() {
    return false;
  },

  lovrHeadsetGetName(name, length) {
    return false;
  },

  lovrHeadsetGetDriver(name, length) {
    return false;
  },

  lovrHeadsetGetFeatures(features) {
    // TODO
  },

  lovrHeadsetIsSeated() {
    return false;
  },

  lovrHeadsetStart() {
    return false;
  },

  lovrHeadsetStop() {
    return false;
  },

  lovrHeadsetIsActive() {
    return false;
  },

  lovrHeadsetIsVisible() {
    return false;
  },

  lovrHeadsetIsFocused() {
    return false;
  },

  lovrHeadsetIsMounted() {
    return true;
  },

  lovrHeadsetPollEvents() {
    return true;
  },

  lovrHeadsetUpdate(dt) {
    HEAPF64[dt >> 3] = Module['_lovrTimerGetDelta']();
    return true;
  },

  lovrHeadsetGetDeltaTime() {
    return Module['_lovrTimerGetDelta']();
  },

  lovrHeadsetGetDisplayTime() {
    return Module['_lovrTimerGetTime']();
  },

  lovrHeadsetGetDisplayDimensions(width, height) {
    HEAPU32[width >> 2] = Module.canvas.width;
    HEAPU32[height >> 2] = Module.canvas.height;
  },

  lovrHeadsetGetRefreshRates(count) {
    HEAPU32[count >> 2] = 0;
    return 0;
  },

  lovrHeadsetGetRefreshRate() {
    return 0.0;
  },

  lovrHeadsetSetRefreshRate(rate) {
    return false;
  },

  lovrHeadsetGetFoveation(level, dynamic) {
    return false;
  },

  lovrHeadsetSetFoveation(level, dynamic) {
    return false;
  },

  lovrHeadsetIsPassthroughSupported(mode) {
    return mode == 0;
  },

  lovrHeadsetGetPassthrough() {
    return 0;
  },

  lovrHeadsetSetPassthrough(mode) {
    return mode == 0;
  },

  lovrHeadsetGetViewCount() {
    return 1;
  },

  lovrHeadsetGetViewPose(view, position, orientation) {
    HEAPF32[(position >> 2) + 0] = state.poses[0].position[0];
    HEAPF32[(position >> 2) + 1] = state.poses[0].position[1];
    HEAPF32[(position >> 2) + 2] = state.poses[0].position[2];
    HEAPF32[(orientation >> 2) + 0] = state.poses[0].orientation[0];
    HEAPF32[(orientation >> 2) + 1] = state.poses[0].orientation[1];
    HEAPF32[(orientation >> 2) + 2] = state.poses[0].orientation[2];
    HEAPF32[(orientation >> 2) + 3] = state.poses[0].orientation[3];
    return view == 0;
  },

  lovrHeadsetGetViewAngles(view, left, right, up, down) {
    const aspect = Module.canvas.width / Module.canvas.height;
    const fov = .7;
    HEAPF32[left >> 2] = Math.atan(Math.tan(fov) * aspect);
    HEAPF32[right >> 2] = Math.atan(Math.tan(fov) * aspect);
    HEAPF32[up >> 2] = fov;
    HEAPF32[down >> 2] = fov;
    return view == 0;
  },

  lovrHeadsetGetClipDistance(near, far) {
    HEAPF32[near >> 2] = state.clipNear;
    HEAPF32[far >> 2] = state.clipFar;
  },

  lovrHeadsetSetClipDistance(near, far) {
    state.clipNear = near;
    state.clipFar = far;
  },

  lovrHeadsetGetBoundsDimensions(width, height) {
    HEAPF32[width >> 2] = 0;
    HEAPF32[height >> 2] = 0;
  },

  lovrHeadsetGetPose(device, position, orientation) {
    HEAPF32[(position >> 2) + 0] = state.poses[device].position[0];
    HEAPF32[(position >> 2) + 1] = state.poses[device].position[1];
    HEAPF32[(position >> 2) + 2] = state.poses[device].position[2];
    HEAPF32[(orientation >> 2) + 0] = state.poses[device].orientation[0];
    HEAPF32[(orientation >> 2) + 1] = state.poses[device].orientation[1];
    HEAPF32[(orientation >> 2) + 2] = state.poses[device].orientation[2];
    HEAPF32[(orientation >> 2) + 3] = state.poses[device].orientation[3];
    return true;
  },

  lovrHeadsetGetVelocity(device, linear, angular) {
    HEAPF32[(linear >> 2) + 0] = 0;
    HEAPF32[(linear >> 2) + 1] = 0;
    HEAPF32[(linear >> 2) + 2] = 0;
    HEAPF32[(angular >> 2) + 0] = 0;
    HEAPF32[(angular >> 2) + 1] = 0;
    HEAPF32[(angular >> 2) + 2] = 0;
  },

  lovrHeadsetIsDown(device, button, down, changed) {
    const mask = 1 << button;
    HEAPU32[down >> 2] = !!(state.buttons[device] & mask);
    HEAPU32[changed >> 2] = !!((state.lastButtons[device] & mask) ^ (state.buttons[device] & mask));
    return true;
  },

  lovrHeadsetIsTouched(device, button, down, changed) {
    return false;
  },

  lovrHeadsetGetAxis(device, axis, value) {
    return false;
  },

  lovrHeadsetGetSkeleton(device, poses, source) {
    return false;
  },

  lovrHeadsetGetBattery(device, level, charging) {
    return false;
  },

  lovrHeadsetVibrate(device, strength, duration, frequency) {
    return false;
  },

  lovrHeadsetStopVibration(device) {
    //
  },

  lovrHeadsetGetModelKeys(count) {
    HEAPU32[count >> 2] = 0;
    return 0;
  },

  lovrHeadsetNewModelData(key) {
    return 0;
  },

  lovrHeadsetGetModelPose(model, position, orientation) {
    return false;
  },

  lovrHeadsetAnimate(model) {
    return false;
  },

  lovrHeadsetSetBackground(width, height, layers) {
    return 0;
  },

  lovrHeadsetGetLayers(count, main) {
    HEAPU32[count >> 2] = 0;
    HEAPU32[main >> 2] = 1;
    return 0;
  },

  lovrHeadsetSetLayers(layers, count, main) {
    return false;
  },

  lovrHeadsetGetTexture(texture) {
    return Module['_lovrGraphicsGetWindowTexture'](texture);
  },

  lovrHeadsetGetDepthTexture(texture) {
    HEAPU32[texture >> 2] = 0;
    return true;
  },

  $mat4_fromPoseInverse(m, position, orientation) {
    const x = -orientation[0], y = -orientation[1], z = -orientation[2], w = orientation[3];
    const m00 = 1 - 2 * y * y - 2 * z * z;
    const m01 = 2 * x * y + 2 * w * z;
    const m02 = 2 * x * z - 2 * w * y;
    const m10 = 2 * x * y - 2 * w * z;
    const m11 = 1 - 2 * x * x - 2 * z * z;
    const m12 = 2 * y * z + 2 * w * x;
    const m20 = 2 * x * z + 2 * w * y;
    const m21 = 2 * y * z - 2 * w * x;
    const m22 = 1 - 2 * x * x - 2 * y * y;
    HEAPF32.set([
      m00, m01, m02, 0,
      m10, m11, m12, 0,
      m20, m21, m22, 0,
      -(m00 * position[0] + m10 * position[1] + m20 * position[2]),
      -(m01 * position[0] + m11 * position[1] + m21 * position[2]),
      -(m02 * position[0] + m12 * position[1] + m22 * position[2]),
      1
    ], m >> 2);
  },

  $mat4_fov(m, fovx, fovy, near, far) {
    const a = 1 / Math.tan(fovx);
    const b = -1 / Math.tan(fovy);
    const c = far === 0 ? 0 : far / (near - far);
    const d = far === 0 ? near : (near * far) / (near - far);
    HEAPF32.set([
      a, 0, 0, 0,
      0, b, 0, 0,
      0, 0, c, -1,
      0, 0, d, 0
    ], m >> 2);
  },

  lovrHeadsetGetPass__deps: ['$stackSave', '$stackAlloc', '$stackRestore', '$mat4_fromPoseInverse', '$mat4_fov'],
  lovrHeadsetGetPass(out) {
    if (!Module['_lovrGraphicsGetWindowPass'](out)) {
      return false;
    }

    const pass = HEAPU32[out >> 2];

    if (pass === 0) {
      return true;
    }

    const stack = stackSave();
    const viewMatrix = stackAlloc(64);
    const projection = stackAlloc(64);

    const fov = .7;
    const aspect = Module.canvas.width / Module.canvas.height;
    const fovx = Math.atan(Math.tan(fov) * aspect);
    const fovy = fov;

    mat4_fromPoseInverse(viewMatrix, state.poses[0].position, state.poses[0].orientation);
    mat4_fov(projection, fovx, fovy, state.clipNear, state.clipFar);

    Module['_lovrPassSetViewMatrix'](pass, 0, viewMatrix);
    Module['_lovrPassSetProjection'](pass, 0, projection);

    stackRestore(stack);
    return true;
  },

  lovrHeadsetSubmit() {
    return true;
  },

  lovrHeadsetSetPose(device, position, orientation) {
    state.poses[device].position[0] = HEAPF32[(position >> 2) + 0];
    state.poses[device].position[1] = HEAPF32[(position >> 2) + 1];
    state.poses[device].position[2] = HEAPF32[(position >> 2) + 2];
    state.poses[device].orientation[0] = HEAPF32[(orientation >> 2) + 0];
    state.poses[device].orientation[1] = HEAPF32[(orientation >> 2) + 1];
    state.poses[device].orientation[2] = HEAPF32[(orientation >> 2) + 2];
    state.poses[device].orientation[3] = HEAPF32[(orientation >> 2) + 3];
  },

  lovrHeadsetSetButton(device, button, down) {
    state.poses[device].buttons &= ~(1 << button);
    state.poses[device].buttons |= (down << button);
  },

  lovrLayerCreate(info) {
    return 0;
  },

  lovrLayerDestroy(ref) {
    //
  },

  lovrLayerGetOrigin(layer) {
    return -1;
  },

  lovrLayerSetOrigin(layer) {
    //
  },

  lovrLayerGetPose(layer, position, orientation) {
    //
  },

  lovrLayerSetPose(layer, position, orientation) {
    //
  },

  lovrLayerGetDimensions(layer, width, height) {
    //
  },

  lovrLayerSetDimensions(layer, width, height) {
    //
  },

  lovrLayerGetCurve(layer) {
    return 0;
  },

  lovrLayerSetCurve(layer, curve) {
    return false;
  },

  lovrLayerGetColor(layer, color) {
    //
  },

  lovrLayerSetColor(layer, color) {
    //
  },

  lovrLayerGetViewport(layer, viewport) {
    //
  },

  lovrLayerSetViewport(layer, viewport) {
    //
  },

  lovrLayerGetTexture(layer) {
    return 0;
  },

  lovrLayerGetPass(layer) {
    return 0;
  }
};

autoAddDeps(headset, '$state');
addToLibrary(headset);
