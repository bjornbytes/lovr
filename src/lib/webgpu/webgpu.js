#if parseInt(EMSCRIPTEN_VERSION.split('.')[0]) < 3 || (parseInt(EMSCRIPTEN_VERSION.split('.')[0]) == 3 && parseInt(EMSCRIPTEN_VERSION.split('.')[1]) < 1) || (parseInt(EMSCRIPTEN_VERSION.split('.')[0]) == 3 && parseInt(EMSCRIPTEN_VERSION.split('.')[1]) == 1 && parseInt(EMSCRIPTEN_VERSION.split('.')[2]) < 35)
// Emscripten 3.1.35 or newer is needed because of https://github.com/emscripten-core/emscripten/issues/19469
#error "wasm_webgpu requires building with Emscripten 3.1.35 or newer. Please update"
#endif

#if MIN_FIREFOX_VERSION < 149
#warning "Firefox browser before version 149 has issues with WebGPU (e.g. https://bugzilla.mozilla.org/show_bug.cgi?id=2023423 )"
#endif

{{{
  globalThis.wassert = function(condition) {
    if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG)) return `assert(${condition}, "assert(${condition.replace(/"/g, "'")}) failed!");`;
    else return '';
  };
  globalThis.wdebuglog = function(condition) {
    return parseInt(globalThis.WEBGPU_DEBUG) ? `console.log(${condition});` : '';
  }
  globalThis.wdebugwarn = function(condition) {
    return parseInt(globalThis.WEBGPU_DEBUG) ? `console.warn(${condition});` : '';
  }
  globalThis.wdebugerror = function(condition) {
    return parseInt(globalThis.WEBGPU_DEBUG) ? `console.error(${condition});` : '';
  }
  globalThis.wdebugdir = function(condition, desc) {
    return parseInt(globalThis.WEBGPU_DEBUG) ? (typeof desc === 'undefined' ? `console.dir(${condition});` : `console.log(${desc}); console.dir(${condition});`) : '';
  }
  globalThis.werror = function(condition) {
    return parseInt(globalThis.WEBGPU_DEBUG) ? `console.error(${condition});` : '';
  }
  // Given a ptr, shift it to become an index to appropriate HEAPxxx type.
  globalThis.replacePtrToIdx = function(ptr, shift) {
    var shr = CAN_ADDRESS_2GB ? '>>>' : '>>';
    var s = '';
    if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG)) {
      if (MEMORY64) s += `assert(${ptr} % ${1 << shift}n == 0n, 'Unaligned pointer fault!');\n`;
      else if (CAN_ADDRESS_2GB) s += `assert((${ptr} >>> 0) % (${1 << shift}) == 0, 'Unaligned pointer fault!');\n`;
      else s += `assert(${ptr} % ${1 << shift} == 0, 'Unaligned pointer fault!');\n`;
    }
    shift = MEMORY64 ? `${shift}n` : `${shift}`;
    if (shift == 0 && MEMORY64) s += `${ptr} = Number(${ptr});`;
    else if (MEMORY64) s += `${ptr} = Number(${ptr} ${shr} ${shift});`;
    else if (shift > 0 || MAXIMUM_MEMORY > 2*1024*1024*1024) s += `${ptr} ${shr}= ${shift};`;
    return s;
  }
  // Returns given ptr converted to an index to appropriate HEAPxxx type.
  globalThis.shiftPtr = function(ptr, shift) {
    var shr = CAN_ADDRESS_2GB ? '>>>' : '>>';
    if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG)) return `wgpu_checked_shift(${ptr}, ${shift})`;
    if (shift == 0 && MEMORY64) return `Number(${ptr})`;
    else if (MEMORY64) return `Number(${ptr} ${shr} ${shift}n)`;
    else if (shift > 0 || MAXIMUM_MEMORY > 2*1024*1024*1024) return `(${ptr} ${shr} ${shift})`;
    else return `(${ptr})`;
  }
  globalThis.shiftIndex = function(index, shift) {
    if (MEMORY64)             return `(${index}) / ${1 << shift}`;
    else if (CAN_ADDRESS_2GB) return `${index} >>> ${shift}`;
    else                      return `${index} >> ${shift}`;
  }
  // Given ptr, read ptr from heap
  globalThis.readPtr = function(ptr, offset) {
    var shr = CAN_ADDRESS_2GB ? '>>>' : '>>';
    var ofs = offset ? ` + ${offset}${MEMORY64?"n":""}` : '';
    if (MEMORY64) return `HEAPU64[${ptr} ${ofs} ${shr} 3n]`;
    else          return `HEAPU32[${ptr} ${ofs} ${shr} 2]`;
  }
  // Given i32 index to ptr location, read it back as an i32 index
  globalThis.readIdx32 = function(heap32Idx) {
    var shr = CAN_ADDRESS_2GB ? '>>>' : '>>';
    if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG)) {
      if (MEMORY64) return `wgpu_checked_shift(HEAPU64[(${heap32Idx}) / 2], 2)`;
      else          return `wgpu_checked_shift(HEAPU32[${heap32Idx}], 2)`;
    }
    if (MEMORY64) return `Number(HEAPU64[(${heap32Idx}) / 2] ${shr} 2n)`;
    else          return `(HEAPU32[${heap32Idx}] ${shr} 2)`;
  }
  // Given i32 index to ptr location, read ptr
  globalThis.readPtrFromIdx32 = function(heap32Idx, offset) {
    var shr = CAN_ADDRESS_2GB ? '>>>' : '>>';
    var ofs = offset ? ` + ${offset}` : '';
    if (MEMORY64) return `HEAPU64[(${heap32Idx} ${ofs}) / 2]`;
    else          return `HEAPU32[${heap32Idx} ${ofs}]`;
  }
  // Given a value that is a BigInt in MEMORY64 builds, and a Number() in 32-bit
  // builds, this function casts it back to Number in a way that does not waste
  // code size in 32-bit builds (when the case is not needed)
  globalThis.toNumber = function(ptr) {
    if (MEMORY64) return `Number(${ptr})`;
    else          return ptr;
  }
  // Same as toNumber, but includes the assignment, i.e. `ptr = Number(ptr);`
  globalThis.convertToNumber = function(ptr) {
    if (MEMORY64) return `${ptr} = Number(${ptr})`;
    else          return '';
  }
  globalThis.toWasm64 = function(x) {
    if (!MEMORY64) return x;
    // Micro-size-optimization: if x is an integer literal, then we can append
    // the suffix 'n' instead of casting to BigInt(), to get smaller code size.
    var n = Number(x);
    if (Number.isInteger(n) && isFinite(n)) return `${x}n`;
    return `BigInt(${x})`;
  }
  globalThis.wasm4GbShift = function(ptr) {
    if (CAN_ADDRESS_2GB) return `${ptr} >>> 0`;
    else return ptr;
  }
  null;
}}}

let api = {
  $wgpu__deps: ['$utf8'
#if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG))
  , '$wgpu_checked_shift'
#endif
  ],

#if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG))
  $wgpuOffscreenCanvases__deps: ['$wgpu_checked_shift'],
#endif

  // Stores a ID->WebGPU object mapping registry of global top-level WebGPU objects.
  $wgpu: {},

  // Stores ID->OffscreenCanvas objects that are owned by the current thread.
  $wgpuOffscreenCanvases: {},

  // Free ID counter generation number
  // 0: reserved for invalid object (i.e. undefined) for e.g. wgpu_encoder_set_bind_group() purposes,
  // 1: reserved for a special GPUTexture that GPUCanvasContext.getCurrentTexture() returns.
  // [2, 2147483647]: valid WebGPU IDs
  $wgpuIdCounter: 2,

  // Stores the given WebGPU object under a new free WebGPU object ID.
  // Returns the new ID. Can be called with a null/undefined, in which
  // case no object/ID is persisted.
  $wgpuStore__deps: ['$wgpu', '$wgpuIdCounter'],
  $wgpuStore: function(object) {
    if (object) {
      // WebGPU renderer usage can burn through a lot of object IDs each rendered frame
      // (a number of GPUCommandEncoder, GPUTexture, GPUTextureView, GPURenderPassEncoder,
      // GPUCommandBuffer objects are created each application frame)
      // If we assume an upper bound of 1000 object IDs created per rendered frame, and a
      // new mobile device with 120hz display, a signed int32 state space is exhausted in
      // 2147483646 / 1000 / 120 / 60 / 60 = 4.97 hours, which is realistic for a page to
      // stay open for that long. Therefore handle wraparound of the ID counter generation,
      // and find free gaps in the object IDs for new objects.
      while(wgpu[wgpuIdCounter]) wgpuIdCounter = wgpuIdCounter < 2147483647 ? wgpuIdCounter + 1 : 2;

      wgpu[wgpuIdCounter] = object;

      // Each persisted objects gets a custom 'wid' field (wasm ID) which stores the ID that
      // this object is known by on Wasm side.
      object.wid = wgpuIdCounter;

      {{{ wdebugdir('object', '`Stored WebGPU object of type \'${object.constructor.name}\' with ID ${wgpuIdCounter}:`') }}};

      return wgpuIdCounter++;
    }
    // Implicit return undefined to marshal ID 0 over to Wasm.
  },

  $wgpuLinkParentAndChild: function(parent, childId, child) {
    child.parentObject = parent; // Link child->parent

    // WebGPU objects form an object hierarchy, and deleting an object (adapter, device, texture, etc.) will
    // destroy all child objects in the hierarchy)
    // N.b. Chrome has been observed to exhibit really poor performance (~100-1000x slower) if a {} is used
    // as an associative container here, so it is critical to use a Map(), which is observed to make
    // this function wgpuLinkParentAndChild() vanish from Chrome's performance profile traces altogether.
    (parent.derivedObjects ??= new Map()).set(childId, child); // Link parent->child
  },

  // Marks the given 'object' to be a child/derived object of 'parent',
  // and stores a reference to the object in the WebGPU table,
  // returning the ID.
  $wgpuStoreAndSetParent__deps: ['$wgpuStore', '$wgpuLinkParentAndChild'],
  $wgpuStoreAndSetParent: function(object, parent) {
    if (object) {
      var objectId = wgpuStore(object);
      wgpuLinkParentAndChild(parent, objectId, object);
      return objectId;
    }
  },

  $wgpuReadArrayOfItemsMaybeNull: function(itemDict, ptr, numItems) {
    {{{ wassert('numItems >= 0'); }}}
    {{{ wassert('ptr != 0 || numItems == 0'); }}} // Must be non-null pointer
    {{{ replacePtrToIdx('ptr', 2); }}}

    return Array.from({length: numItems}, () => itemDict[HEAPU32[ptr++]]);
  },

#if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG))
  // Reads a contiguous list of WebGPU things from the WebAssembly heap.
  // itemDict: an array or a dictionary that maps the WebAssembly identifiers to JavaScript side objects.
  //           This is either `wgpu` for WebGPU object mappings, or one of the decoded enum arrays.
  // ptr: A byte pointer to Wasm heap.
  $wgpuReadArrayOfItems__deps: ['$wgpuReadArrayOfItemsMaybeNull'],
  $wgpuReadArrayOfItems: function(itemDict, ptr, numItems) {
    var idx = {{{ shiftPtr('ptr', 2) }}};
    for(var i = 0; i < numItems; ++i) {
      {{{ wassert('HEAPU32[idx+i]'); }}} // Must reference a nonzero item
      {{{ wassert('itemDict[HEAPU32[idx+i]]'); }}} // Must reference a valid item in the array
    }
    return wgpuReadArrayOfItemsMaybeNull(itemDict, ptr, numItems);
  },
#else
  $wgpuReadArrayOfItems: '$wgpuReadArrayOfItemsMaybeNull',
#endif

  $wgpuReadI53FromU64HeapIdx: function(heap32Idx) {
    {{{ wassert('heap32Idx != 0'); }}}
#if WASM_BIGINT
    {{{ wassert('heap32Idx % 2 == 0'); }}}
    return Number(HEAPU64[heap32Idx >>> 1]);
#else
    return HEAPU32[heap32Idx] + HEAPU32[heap32Idx+1] * 4294967296;
#endif
  },

  $wgpuWriteI53ToU64HeapIdx: function(heap32Idx, number) {
    {{{ wassert('heap32Idx != 0'); }}}
#if WASM_BIGINT
    {{{ wassert('heap32Idx % 2 == 0'); }}}
    HEAPU64[heap32Idx >>> 1] = BigInt(number);
#else
    HEAPU32[heap32Idx] = number;
    HEAPU32[heap32Idx+1] = number / 4294967296;
#endif
  },

  wgpu_get_num_live_objects__deps: ['$wgpu'],
  wgpu_get_num_live_objects: function() {
    return Object.keys(wgpu).length;
  },

  // Calls .destroy() on the given WebGPU object, and releases the reference to it.
  wgpu_object_destroy__deps: ['$wgpu'],
  wgpu_object_destroy: function(object) {
    let o = wgpu[object];
    {{{ wassert(`o || !wgpu.hasOwnProperty(object), 'wgpu dictionary should never be storing key-values with null/undefined value in it'`); }}}
    if (o) {
      // Make sure if there might exist any other references to this JS object, that they will no longer see the .wid
      // field, since this object no longer exists in the wgpu table.
      o.wid = 0;
      // WebGPU objects of type GPUDevice, GPUBuffer, GPUTexture and GPUQuerySet have an explicit .destroy() function. Call that if applicable.
      o['destroy']?.();
      // If the given object has derived objects (GPUTexture -> GPUTextureViews), delete those in a hierarchy as well.
      o.derivedObjects?.forEach((_,k) => _wgpu_object_destroy(k));
      // If this object has a parent, unlink this object from its parent.
      o.parentObject?.derivedObjects.delete(object);
      // Finally erase reference to this object.
      delete wgpu[object];
    }
    {{{ wassert(`!wgpu.hasOwnProperty(object), 'object should have gotten deleted'`); }}}
  },

  wgpu_destroy_all_objects__deps: ['$wgpu'],
  wgpu_destroy_all_objects: function() {
    Object.values(wgpu).forEach(o => {
      o.wid = 0;
      o['destroy']?.();
    });
    wgpu = {};
  },

  wgpu_is_valid_object: function(o) { return !!wgpu[o]; }, // Tests if this ID references anything (not just a GPUObjectBase)
  wgpu_is_adapter: function(o) { return wgpu[o] instanceof GPUAdapter; },
  wgpu_is_device: function(o) { return wgpu[o] instanceof GPUDevice; },
  wgpu_is_buffer: function(o) { return wgpu[o] instanceof GPUBuffer; },
  wgpu_is_texture: function(o) { return wgpu[o] instanceof GPUTexture; },
  wgpu_is_texture_view: function(o) { return wgpu[o] instanceof GPUTextureView; },
  wgpu_is_external_texture: function(o) { return wgpu[o] instanceof GPUExternalTexture; },
  wgpu_is_sampler: function(o) { return wgpu[o] instanceof GPUSampler; },
  wgpu_is_bind_group_layout: function(o) { return wgpu[o] instanceof GPUBindGroupLayout; },
  wgpu_is_bind_group: function(o) { return wgpu[o] instanceof GPUBindGroup; },
  wgpu_is_pipeline_layout: function(o) { return wgpu[o] instanceof GPUPipelineLayout; },
  wgpu_is_shader_module: function(o) { return wgpu[o] instanceof GPUShaderModule; },
  wgpu_is_compute_pipeline: function(o) { return wgpu[o] instanceof GPUComputePipeline; },
  wgpu_is_render_pipeline: function(o) { return wgpu[o] instanceof GPURenderPipeline; },
  wgpu_is_command_buffer: function(o) { return wgpu[o] instanceof GPUCommandBuffer; },
  wgpu_is_command_encoder: function(o) { return wgpu[o] instanceof GPUCommandEncoder; },
  wgpu_is_binding_commands_mixin: function(o) { return wgpu[o] instanceof GPUComputePassEncoder || wgpu[o] instanceof GPURenderPassEncoder || wgpu[o] instanceof GPURenderBundleEncoder; },
  wgpu_is_render_commands_mixin: function(o) { return wgpu[o] instanceof GPURenderPassEncoder || wgpu[o] instanceof GPURenderBundleEncoder; },
  wgpu_is_render_pass_encoder: function(o) { return wgpu[o] instanceof GPURenderPassEncoder; },
  wgpu_is_render_bundle: function(o) { return wgpu[o] instanceof GPURenderBundle; },
  wgpu_is_render_bundle_encoder: function(o) { return wgpu[o] instanceof GPURenderBundleEncoder; },
  wgpu_is_compute_pass_encoder: function(o) { return wgpu[o] instanceof GPUComputePassEncoder; },
  wgpu_is_queue: function(o) { return wgpu[o] instanceof GPUQueue; },
  wgpu_is_query_set: function(o) { return wgpu[o] instanceof GPUQuerySet; },
  wgpu_is_canvas_context: function(o) { return wgpu[o] instanceof GPUCanvasContext; },
  wgpu_is_device_lost_info: function(o) { return wgpu[o] instanceof GPUDeviceLostInfo; },
  wgpu_is_error: function(o) { return wgpu[o] instanceof GPUError; },

  wgpu_object_set_label__deps: ['$utf8'],
  wgpu_object_set_label: function(o, label) {
    {{{ wassert('wgpu[o]'); }}}
    wgpu[o]['label'] = utf8(label);
  },

  wgpu_object_get_label__deps: ['$stringToUTF8'],
  wgpu_object_get_label: function(o, dstLabel, dstLabelSize) {
    {{{ wassert('wgpu[o]'); }}}
    stringToUTF8(wgpu[o]['label'], {{{ toNumber('dstLabel') }}}, dstLabelSize);
  },

  $wgpu_checked_shift: function(ptr, shift) {
#if MEMORY64
    assert(ptr % BigInt(1 << shift) == 0n, 'Unaligned pointer fault!');
    return Number(ptr / BigInt(1 << shift));
#elif CAN_ADDRESS_2GB
    assert((ptr >>> 0) % (1 << shift) == 0, 'Unaligned pointer fault!');
    return ptr >>> shift;
#else
    assert(ptr % (1 << shift) == 0);
    return ptr >> shift;
#endif
  },

  // UTF8ToString(ptr) does not work with Wasm64 pointers (cannot take in a BigInt), so use a custom
  // Wasm string -> JS string marshalling wrapper that is Wasm64 aware.
  $utf8: function(ptr) {
    return UTF8ToString({{{ toNumber('ptr') }}});
  },

  wgpu_canvas_get_webgpu_context__deps: ['$wgpuStore'
#if PROXY_TO_PTHREAD
    , '$GL'
#endif
  ],
  wgpu_canvas_get_webgpu_context: function(canvasSelector) {
    {{{ wdebuglog('`wgpu_canvas_get_webgpu_context(canvasSelector="${utf8(canvasSelector)}")`'); }}}
    {{{ wassert('canvasSelector'); }}}

#if PROXY_TO_PTHREAD
    // Search transferred OffscreenCanvases
    {{{ wdebugdir('GL.offscreenCanvases', '`Calling thread owns ${Object.keys(GL.offscreenCanvases).length} OffscreenCanvases in its object table:`') }}};
    let canvas = GL.offscreenCanvases[utf8(canvasSelector)].offscreenCanvas;
#else
    // Search Canvas elements in DOM.
    let canvas = document.querySelector(utf8(canvasSelector));
#endif
    {{{ wdebugdir('canvas', '`querySelector returned:`') }}};

    let ctx = canvas.getContext('webgpu');
    {{{ wdebugdir('ctx', '`canvas.getContext("webgpu") returned:`') }}};

    if (ctx.wid) return ctx.wid; // Return existing ID if user has called wgpu_canvas_get_webgpu_context() multiple times on the same canvas.

    return wgpuStore(ctx);
  },

  wgpu_offscreen_canvas_get_webgpu_context__deps: ['$wgpuOffscreenCanvases'],
  wgpu_offscreen_canvas_get_webgpu_context: function(offscreenCanvasId) {
    {{{ wdebuglog('`wgpu_offscreen_canvas_get_webgpu_context(offscreenCanvasId=${offscreenCanvasId})`'); }}}
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}

    let ctx = wgpuOffscreenCanvases[offscreenCanvasId].getContext('webgpu');
    {{{ wdebugdir('ctx', '`offscreenCanvas.getContext("webgpu") returned:`') }}};

    if (ctx.wid) return ctx.wid; // Return existing ID if user has called wgpu_offscreen_canvas_get_webgpu_context() multiple times on the same canvas.

    return wgpuStore(ctx);
  },

#if ASYNCIFY
  wgpu_request_animation_frame_loop__deps: ['_wgpuNumAsyncifiedOperationsPending'],
#endif
  wgpu_request_animation_frame_loop: (cb, userData) => {
#if ASYNCIFY == 2
    cb = WebAssembly.promising(getWasmTableEntry(cb));
#else
    cb = getWasmTableEntry(cb);
#endif
    function tick(timeStamp) {
      if (
#if ASYNCIFY
        __wgpuNumAsyncifiedOperationsPending ||
#endif
        cb(timeStamp, userData)) requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
  },

  $GPUTextureAndVertexFormats: [, 'r8unorm', 'r8snorm', 'r8uint', 'r8sint', 'r16unorm', 'r16snorm', 'r16uint', 'r16sint', 'r16float', 'rg8unorm', 'rg8snorm', 'rg8uint', 'rg8sint', 'r32uint', 'r32sint', 'r32float', 'rg16unorm', 'rg16snorm', 'rg16uint', 'rg16sint', 'rg16float', 'rgba8unorm', 'rgba8unorm-srgb', 'rgba8snorm', 'rgba8uint', 'rgba8sint', 'bgra8unorm', 'bgra8unorm-srgb', 'rgb9e5ufloat', 'rgb10a2uint', 'rgb10a2unorm', 'rg11b10ufloat', 'rg32uint', 'rg32sint', 'rg32float', 'rgba16unorm', 'rgba16snorm', 'rgba16uint', 'rgba16sint', 'rgba16float', 'rgba32uint', 'rgba32sint', 'rgba32float', 'stencil8', 'depth16unorm', 'depth24plus', 'depth24plus-stencil8', 'depth32float', 'depth32float-stencil8', 'bc1-rgba-unorm', 'bc1-rgba-unorm-srgb', 'bc2-rgba-unorm', 'bc2-rgba-unorm-srgb', 'bc3-rgba-unorm', 'bc3-rgba-unorm-srgb', 'bc4-r-unorm', 'bc4-r-snorm', 'bc5-rg-unorm', 'bc5-rg-snorm', 'bc6h-rgb-ufloat', 'bc6h-rgb-float', 'bc7-rgba-unorm', 'bc7-rgba-unorm-srgb', 'etc2-rgb8unorm', 'etc2-rgb8unorm-srgb', 'etc2-rgb8a1unorm', 'etc2-rgb8a1unorm-srgb', 'etc2-rgba8unorm', 'etc2-rgba8unorm-srgb', 'eac-r11unorm', 'eac-r11snorm', 'eac-rg11unorm', 'eac-rg11snorm', 'astc-4x4-unorm', 'astc-4x4-unorm-srgb', 'astc-5x4-unorm', 'astc-5x4-unorm-srgb', 'astc-5x5-unorm', 'astc-5x5-unorm-srgb', 'astc-6x5-unorm', 'astc-6x5-unorm-srgb', 'astc-6x6-unorm', 'astc-6x6-unorm-srgb', 'astc-8x5-unorm', 'astc-8x5-unorm-srgb', 'astc-8x6-unorm', 'astc-8x6-unorm-srgb', 'astc-8x8-unorm', 'astc-8x8-unorm-srgb', 'astc-10x5-unorm', 'astc-10x5-unorm-srgb', 'astc-10x6-unorm', 'astc-10x6-unorm-srgb', 'astc-10x8-unorm', 'astc-10x8-unorm-srgb', 'astc-10x10-unorm', 'astc-10x10-unorm-srgb', 'astc-12x10-unorm', 'astc-12x10-unorm-srgb', 'astc-12x12-unorm', 'astc-12x12-unorm-srgb', 'uint8', 'uint8x2', 'uint8x4', 'sint8', 'sint8x2', 'sint8x4', 'unorm8', 'unorm8x2', 'unorm8x4', 'snorm8', 'snorm8x2', 'snorm8x4', 'uint16', 'uint16x2', 'uint16x4', 'sint16', 'sint16x2', 'sint16x4', 'unorm16', 'unorm16x2', 'unorm16x4', 'snorm16', 'snorm16x2', 'snorm16x4', 'float16', 'float16x2', 'float16x4', 'float32', 'float32x2', 'float32x3', 'float32x4', 'uint32', 'uint32x2', 'uint32x3', 'uint32x4', 'sint32', 'sint32x2', 'sint32x3', 'sint32x4', 'unorm10-10-10-2', 'unorm8x4-bgra' ],
  wgpu32BitLimitNames: ['maxTextureDimension1D', 'maxTextureDimension2D', 'maxTextureDimension3D', 'maxTextureArrayLayers', 'maxBindGroups', 'maxBindGroupsPlusVertexBuffers', 'maxBindingsPerBindGroup', 'maxDynamicUniformBuffersPerPipelineLayout', 'maxDynamicStorageBuffersPerPipelineLayout', 'maxSampledTexturesPerShaderStage', 'maxSamplersPerShaderStage', 'maxStorageBuffersPerShaderStage', 'maxStorageBuffersInVertexStage', 'maxStorageBuffersInFragmentStage', 'maxStorageTexturesPerShaderStage', 'maxStorageTexturesInVertexStage', 'maxStorageTexturesInFragmentStage', 'maxUniformBuffersPerShaderStage', 'minUniformBufferOffsetAlignment', 'minStorageBufferOffsetAlignment', 'maxVertexBuffers', 'maxVertexAttributes', 'maxVertexBufferArrayStride', 'maxInterStageShaderVariables', 'maxColorAttachments', 'maxColorAttachmentBytesPerSample', 'maxComputeWorkgroupStorageSize', 'maxComputeInvocationsPerWorkgroup', 'maxComputeWorkgroupSizeX', 'maxComputeWorkgroupSizeY', 'maxComputeWorkgroupSizeZ', 'maxComputeWorkgroupsPerDimension' ],
  wgpu64BitLimitNames: ['maxUniformBufferBindingSize', 'maxStorageBufferBindingSize', 'maxBufferSize' ],
  wgpuFeatures: ['core-features-and-limits', 'depth-clip-control', 'depth32float-stencil8', 'texture-compression-bc', 'texture-compression-bc-sliced-3d', 'texture-compression-etc2', 'texture-compression-astc', 'texture-compression-astc-sliced-3d', 'timestamp-query', 'indirect-first-instance', 'shader-f16', 'rg11b10ufloat-renderable', 'bgra8unorm-storage', 'float32-filterable', 'float32-blendable', 'clip-distances', 'dual-source-blending', 'subgroups', 'texture-formats-tier1', 'texture-formats-tier2', 'primitive-index', 'texture-component-swizzle' ],
  $GPUBlendFactors: [, 'zero', 'one', 'src', 'one-minus-src', 'src-alpha', 'one-minus-src-alpha', 'dst', 'one-minus-dst', 'dst-alpha', 'one-minus-dst-alpha', 'src-alpha-saturated', 'constant', 'one-minus-constant', 'src1', 'one-minus-src1', 'src1-alpha', 'one-minus-src1-alpha' ],
  $GPUStencilOperations: [, 'keep', 'zero', 'replace', 'invert', 'increment-clamp', 'decrement-clamp', 'increment-wrap', 'decrement-wrap' ],
  $GPUCompareFunctions: [, 'never', 'less', 'equal', 'less-equal', 'greater', 'not-equal', 'greater-equal', 'always' ],
  $GPUBlendOperations: [, 'add', 'subtract', 'reverse-subtract', 'min', 'max' ],
  $GPUIndexFormats: [, 'uint16', 'uint32' ],
  $GPUTextureDimensions: [, '1d', '2d', '3d'],
  $GPUTextureViewDimensions: [, '1d', '2d', '2d-array', 'cube', 'cube-array', '3d' ],
  $GPUStorageTextureSampleTypes: [, 'write-only', 'read-only', 'read-write' ],
  $GPUAddressModes: [, 'clamp-to-edge', 'repeat', 'mirror-repeat' ],
  $GPUTextureAspects: [, 'all', 'stencil-only', 'depth-only' ],
  $GPUPrimitiveTopologys: [, 'point-list', 'line-list', 'line-strip', 'triangle-list', 'triangle-strip' ],
  $GPUBufferBindingTypes: [, 'uniform', 'storage', 'read-only-storage' ],
  $GPUSamplerBindingTypes: [, 'filtering', 'non-filtering', 'comparison' ],
  $GPUTextureSampleTypes: [, 'float', 'unfilterable-float', 'depth', 'sint', 'uint' ],
  $GPUQueryTypes: [, 'occlusion', 'timestamp'],
  $HTMLPredefinedColorSpaces: [, 'srgb', 'srgb-linear', 'display-p3', 'display-p3-linear'],
  $GPUFilterModes: [, 'nearest', 'linear' ],
  // $GPUMipmapFilterModes has identical values to $GPUFilterModes - reuse the same array for both mag/min and mipmap filter lookups.
  // $GPUMipmapFilterModes: [, 'nearest', 'linear' ],
  $GPULoadOps: [, 'load', 'clear'],
  $GPUStoreOps: [, 'store', 'discard'],
  $GPUCanvasToneMappingModes: [, 'standard', 'extended'],
  $GPUCanvasAlphaModes: [, 'opaque', 'premultiplied'],
  $GPUAutoLayoutMode: '="auto"',

  wgpu_canvas_context_configure__deps: ['$GPUTextureAndVertexFormats', '$HTMLPredefinedColorSpaces', '$wgpuReadArrayOfItems', '$GPUCanvasToneMappingModes', '$GPUCanvasAlphaModes'],
  wgpu_canvas_context_configure: function(canvasContext, config) {
    {{{ wdebuglog('`wgpu_canvas_context_configure(canvasContext=${canvasContext}, config=${config})`'); }}}
    {{{ wassert('canvasContext != 0'); }}}
    {{{ wassert('wgpu[canvasContext]'); }}}
    {{{ wassert('wgpu[canvasContext] instanceof GPUCanvasContext'); }}}
    {{{ wassert('config != 0'); }}} // Must be non-null

    {{{ replacePtrToIdx('config', 2); }}}

    let desc = {
      'device': wgpu[HEAPU32[config]],
      'format': GPUTextureAndVertexFormats[HEAPU32[config+1]],
      'usage': HEAPU32[config+2],
      'viewFormats': wgpuReadArrayOfItems(GPUTextureAndVertexFormats, {{{ readPtrFromIdx32('config', 4) }}}, HEAPU32[config+3]),
      'colorSpace': HTMLPredefinedColorSpaces[HEAPU32[config+6]],
      'toneMapping': {
        'mode': GPUCanvasToneMappingModes[HEAPU32[config+7]]
      },
      'alphaMode': GPUCanvasAlphaModes[HEAPU32[config+8]]
    };

    {{{ wdebugdir('desc', '`canvasContext.configure() with descriptor:`') }}};
    wgpu[canvasContext]['configure'](desc);
  },

  wgpu_canvas_context_unconfigure: function(canvasContext) {
    {{{ wdebuglog('`wgpu_canvas_context_unconfigure(canvasContext=${canvasContext})`'); }}}
    {{{ wassert('canvasContext != 0'); }}}
    {{{ wassert('wgpu[canvasContext]'); }}}
    {{{ wassert('wgpu[canvasContext] instanceof GPUCanvasContext'); }}}

    wgpu[canvasContext]['unconfigure']();
  },

  wgpu_canvas_context_get_configuration__deps: ['malloc', '$GPUTextureAndVertexFormats', '$HTMLPredefinedColorSpaces', '$GPUCanvasToneMappingModes', '$GPUCanvasAlphaModes'],
  wgpu_canvas_context_get_configuration: function(canvasContext) {
    {{{ wdebuglog('`wgpu_canvas_context_get_configuration(canvasContext=${canvasContext})`'); }}}
    {{{ wassert('canvasContext != 0'); }}}
    {{{ wassert('wgpu[canvasContext]'); }}}
    {{{ wassert('wgpu[canvasContext] instanceof GPUCanvasContext'); }}}

    var cfg = wgpu[canvasContext]['getConfiguration']();
    {{{ wdebugdir('cfg', '`canvasContext.getConfiguration() returned:`') }}};
    if (!cfg) return {{{ toWasm64('0') }}};

    var numViewFormats = cfg['viewFormats'].length;
    var config = _malloc(40+4*numViewFormats); // sizeof(WGpuCanvasConfiguration) + 4*numViewFormats
    var c = {{{ shiftIndex('config', 2) }}};

    HEAPU32[c]   = cfg['device'].wid;
    HEAPU32[c+1] = GPUTextureAndVertexFormats.indexOf(cfg['format']);
    HEAPU32[c+2] = cfg['usage'];
    HEAPU32[c+3] = numViewFormats;
#if MEMORY64
    HEAPU64[c+4 >>> 1] = BigInt(config + 40); // viewFormats pointer
#else
    HEAPU32[c+4] = config + 40;
#endif
    HEAPU32[c+6] = HTMLPredefinedColorSpaces.indexOf(cfg['colorSpace']);
    // TODO: Firefox does not currently implement toneMapping field. Remove '?.' after all browsers support it.
#if MIN_FIREFOX_VERSION != TARGET_NOT_SUPPORTED
    HEAPU32[c+7] = GPUCanvasToneMappingModes.indexOf(cfg['toneMapping']?.['mode']);
#else
    HEAPU32[c+7] = GPUCanvasToneMappingModes.indexOf(cfg['toneMapping']['mode']);
#endif
    HEAPU32[c+8] = GPUCanvasAlphaModes.indexOf(cfg['alphaMode']);

    // Populate the actual formats into the viewFormats pointer.
    for(var i = 0; i < numViewFormats; ++i) {
      HEAPU32[c+10+i] = GPUTextureAndVertexFormats.indexOf(cfg['viewFormats'][i]);
    }

    return {{{ toWasm64('config') }}}; // Return the malloc()ed pointer to caller. It must remember to free it!
  },

  wgpu_canvas_context_get_current_texture__deps: ['wgpu_object_destroy', '$wgpuLinkParentAndChild'],
  wgpu_canvas_context_get_current_texture: function(canvasContext) {
    {{{ wdebuglog('`wgpu_canvas_context_get_current_texture(canvasContext=${canvasContext})`'); }}}
    {{{ wassert('canvasContext != 0'); }}}
    {{{ wassert('wgpu[canvasContext]'); }}}
    {{{ wassert('wgpu[canvasContext] instanceof GPUCanvasContext'); }}}

    canvasContext = wgpu[canvasContext];
    // The canvas context texture is a special texture that automatically invalidates itself after the current rAF()
    // callback if over. Therefore when a new swap chain texture is produced, we need to delete the old one to avoid
    // accumulating references to stale textures from each frame.

    // Acquire the new canvas context texture..
    var canvasTexture = canvasContext['getCurrentTexture']();
    {{{ wassert('canvasTexture'); }}}
    if (canvasTexture != wgpu[1]) {
      // ... and destroy previous special canvas context texture, if it was an old one.
      _wgpu_object_destroy(1);
      wgpu[1] = canvasTexture;
      canvasTexture.wid = 1;
      wgpuLinkParentAndChild(canvasContext, 1, canvasTexture);
    }
    // The canvas context texture is hardcoded the special ID 1. Return that ID to caller.
    return 1;
  },

  wgpuReportErrorCodeAndMessage__deps: ['$lengthBytesUTF8', '$stringToUTF8'
#if parseInt(EMSCRIPTEN_VERSION.split('.')[0]) > 3 || (parseInt(EMSCRIPTEN_VERSION.split('.')[0]) == 3 && parseInt(EMSCRIPTEN_VERSION.split('.')[1]) > 1) || (parseInt(EMSCRIPTEN_VERSION.split('.')[0]) == 3 && parseInt(EMSCRIPTEN_VERSION.split('.')[1]) == 1 && parseInt(EMSCRIPTEN_VERSION.split('.')[2]) >= 57)
  , '$stackSave', '$stackAlloc', '$stackRestore'
#endif
  ],
  wgpuReportErrorCodeAndMessage: function(device, callback, errorCode, stringMessage, userData) {
    if (stringMessage) {
      // n.b. these variables deliberately rely on 'var' scope.
      var stackTop = stackSave(),
        len = lengthBytesUTF8(stringMessage)+1,
        errorMessage = stackAlloc(len);
      stringToUTF8(stringMessage, errorMessage, len);
    }
#if MEMORY64
    {{{ makeDynCall('viipp', 'callback') }}}(device, errorCode, errorMessage||0n, userData);
#else
    {{{ makeDynCall('viipp', 'callback') }}}(device, errorCode, errorMessage, userData);
#endif
    if (stackTop) stackRestore(stackTop);
  },

  wgpu_device_set_lost_callback__deps: ['wgpuReportErrorCodeAndMessage'],
  wgpu_device_set_lost_callback: function(device, callback, userData) {
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    wgpu[device]['lost'].then(deviceLostInfo => {
      {{{ wdebuglog("`WebGPU device lost. Reason: ${deviceLostInfo['reason']}`"); }}}
      _wgpuReportErrorCodeAndMessage(device, callback,
        +(deviceLostInfo['reason'][0] == 'd')/*WGPU_DEVICE_LOST_REASON_DESTROYED=1, UNKNOWN=0*/,
        deviceLostInfo['message'], userData);
    });
  },

  wgpu_device_push_error_scope: function(device, filter) {
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    wgpu[device]['pushErrorScope']([, 'out-of-memory', 'validation', 'internal'][filter]);
  },

  wgpuErrorObjectToErrorType: function(error) {
    return error
        ? (error instanceof GPUInternalError    ? 3/*WGPU_ERROR_TYPE_INTERNAL*/
        : (error instanceof GPUValidationError  ? 2/*WGPU_ERROR_TYPE_VALIDATION*/
        : (error instanceof GPUOutOfMemoryError ? 1/*WGPU_ERROR_TYPE_OUT_OF_MEMORY*/
        : 4/*WGPU_ERROR_TYPE_UNKNOWN*/)))
        : 0/*WGPU_ERROR_TYPE_NO_ERROR*/;
  },

  wgpuDispatchWebGpuErrorEvent__deps: ['wgpuReportErrorCodeAndMessage', 'wgpuErrorObjectToErrorType'],
  wgpuDispatchWebGpuErrorEvent: function(device, callback, error, userData) {
    // Awkward WebGPU spec: errors do not contain a data-driven error code that
    // could be used to identify the error type in a general forward compatible
    // fashion, but must do an 'instanceof' check to look at the types of the
    // errors. If new error types are introduced in the future, their types won't
    // be recognized! (and code size creeps by having to do an 'instanceof' on every
    // error type)
    _wgpuReportErrorCodeAndMessage(device,
      callback,
      _wgpuErrorObjectToErrorType(error),
      error?.['message'],
      userData);
  },

  // wgpuMuteJsExceptions(fn) returns a new function that can be called to invoke
  // the passed function in a way that all exceptions coming out from that function
  // are guarded inside a try-catch.
  wgpuMuteJsExceptions: function(fn) {
    return (p) => { // only support one argument to function fn (we could do ...params, but we only ever need one arg so that's fine)
      try {
        return fn(p);
      } catch(e) {
        {{{ werror('`Exception thrown when handling a WebGPU callback: ${e}`'); }}}
      }
    }
  },

  wgpu_device_pop_error_scope_async__deps: ['wgpuDispatchWebGpuErrorEvent', 'wgpuMuteJsExceptions'],
  wgpu_device_pop_error_scope_async: function(device, callback, userData) {
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('callback'); }}}

    let d = error => _wgpuDispatchWebGpuErrorEvent(device, callback, error, userData);
    wgpu[device]['popErrorScope']().then(_wgpuMuteJsExceptions(d)).catch(d);
  },

  wgpu_device_pop_error_scope_sync__deps: ['_wgpuNumAsyncifiedOperationsPending', '$wgpu_async', 'wgpuMuteJsExceptions', '$stringToUTF8', 'wgpuErrorObjectToErrorType'],
//  wgpu_device_pop_error_scope_sync__sig: 'iipi', // Iirc this would be needed for -sASYNCIFY=1 build mode, but this breaks -sMEMORY64=1 due to the detrimental automatic wrappers
  wgpu_device_pop_error_scope_sync__async: true,
  wgpu_device_pop_error_scope_sync: function(device, msg, msgLen) {
    return wgpu_async(() => {
      {{{ wassert('device != 0'); }}}
      {{{ wassert('wgpu[device]'); }}}
      {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
      {{{ wassert('msgLen >= 0'); }}}
      {{{ wassert('msg || msgLen == 0'); }}}

      let dispatchErrorCallback = error => {
        --__wgpuNumAsyncifiedOperationsPending;
        var err = error?.['message'] || '';
#if (ASSERTIONS || parseInt(globalThis.WEBGPU_DEBUG))
        console.dir(err);
#endif
        stringToUTF8(err, {{{ toNumber('msg') }}}, msgLen);
        return _wgpuErrorObjectToErrorType(error);
      };

      ++__wgpuNumAsyncifiedOperationsPending;
      return wgpu[device]['popErrorScope']()
        .then(_wgpuMuteJsExceptions(dispatchErrorCallback))
        .catch(dispatchErrorCallback);
    });
  },

  wgpu_device_set_uncapturederror_callback__deps: ['wgpuDispatchWebGpuErrorEvent'],
  wgpu_device_set_uncapturederror_callback: function(device, callback, userData) {
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    wgpu[device]['onuncapturederror'] = callback ? function(uncapturedError) {
      {{{ wdebugdir('uncapturedError'); }}}
      _wgpuDispatchWebGpuErrorEvent(device, callback, uncapturedError['error'], userData);
    } : null;
  },

  navigator_gpu_available: function() {
    return !!navigator['gpu'];
  },

  navigator_delete_webgpu_api_access: function() {
    // N.b. removing access to WebGPU is done via the prototype chain of Navigator,
    // and not the instance object navigator.
    delete Navigator.prototype.gpu;
  },

  $wgpuReadAdapterOptions: function(options) {
    return options ? {
      'featureLevel': [, 'compatibility'][HEAPU32[options]], // 'core' is omitted intentionally, since the spec says it is the default value.
      'powerPreference': [, 'low-power', 'high-performance'][HEAPU32[options+1]],
      'forceFallbackAdapter': !!HEAPU32[options+2],
      'xrCompatible': !!HEAPU32[options+3]
    } : {};
  },

  navigator_gpu_request_adapter_async__deps: ['$wgpuStore', 'wgpuMuteJsExceptions', '$wgpuReadAdapterOptions'],
  navigator_gpu_request_adapter_async__docs: '/** @suppress{checkTypes} */', // This function intentionally calls cb() without args.
  navigator_gpu_request_adapter_async: function(options, adapterCallback, userData) {
    {{{ wdebuglog('`navigator_gpu_request_adapter_async(options: ${options}, adapterCallback: ${adapterCallback}, userData: ${userData})`'); }}}
    {{{ wassert('adapterCallback, "must pass a callback function to navigator_gpu_request_adapter_async!"'); }}}
    {{{ wassert('navigator["gpu"], "Your browser does not support WebGPU!"'); }}}
    {{{ wassert('options != 0'); }}}

    {{{ replacePtrToIdx('options', 2); }}}

    let gpu = navigator['gpu'], opts = wgpuReadAdapterOptions(options);

    if (gpu) {
      {{{ wdebuglog('`navigator.gpu.requestAdapter(options=${JSON.stringify(opts)})`'); }}}
      let cb = adapter => {
        {{{ wdebugdir('adapter', '`navigator.gpu.requestAdapter resolved with following adapter:`'); }}}
        {{{ makeDynCall('vip', 'adapterCallback') }}}(wgpuStore(adapter), userData);
      };
      gpu['requestAdapter'](opts)
        .then(_wgpuMuteJsExceptions(cb))
        .catch(
#if ASSERTIONS || globalThis.WEBGPU_DEBUG
        (e)=>{console.error(`navigator.gpu.requestAdapter() Promise rejected: ${e}`); cb(/*intentionally omit arg to pass undefined*/)}
#else
        ()=>{cb(/*intentionally omit arg to pass undefined*/)}
#endif
      );
      return 1/*WGPU_TRUE*/;
    }
    {{{ werror('`WebGPU is not supported by the current browser!`'); }}}
    // Implicit return WGPU_FALSE, WebGPU is not supported.
  },

#if ASYNCIFY
  _wgpuNumAsyncifiedOperationsPending: 0,

  $wgpuStoreAsyncifiedOp__deps: ['_wgpuNumAsyncifiedOperationsPending', '$wgpuStoreAndSetParent', '$wgpuStore'],
  $wgpuStoreAsyncifiedOp: function(object, parent) {
    {{{ wassert('__wgpuNumAsyncifiedOperationsPending > 0'); }}}
    --__wgpuNumAsyncifiedOperationsPending;
    return parent ? wgpuStoreAndSetParent(object, parent) : wgpuStore(object);
  },

  wgpu_sync_operations_pending: function() {
    return __wgpuNumAsyncifiedOperationsPending;
  },

  $wgpu_async: async function(func) {
    return await func();
  },

  navigator_gpu_request_adapter_sync__deps: ['$wgpuStoreAsyncifiedOp', '_wgpuNumAsyncifiedOperationsPending', '$wgpu_async', '$wgpuReadAdapterOptions'],
//  navigator_gpu_request_adapter_sync__sig: 'ip', // Iirc this would be needed for -sASYNCIFY=1 build mode, but this breaks -sMEMORY64=1 due to the detrimental automatic wrappers
  navigator_gpu_request_adapter_sync__async: true,
  navigator_gpu_request_adapter_sync: function(options) {
    return wgpu_async(() => {
      {{{ wdebuglog('`navigator_gpu_request_adapter_sync(options: ${options})`'); }}}
      {{{ wassert('navigator["gpu"], "Your browser does not support WebGPU!"'); }}}
      {{{ wassert('options != 0'); }}}

      {{{ replacePtrToIdx('options', 2); }}}

      let gpu = navigator['gpu'], opts = wgpuReadAdapterOptions(options);

      if (gpu) {
        {{{ wdebuglog('`navigator.gpu.requestAdapter(options=${JSON.stringify(opts)})`'); }}}
        ++__wgpuNumAsyncifiedOperationsPending;
        return gpu['requestAdapter'](opts).then(wgpuStoreAsyncifiedOp);
      }
      {{{ werror('`WebGPU is not supported by the current browser!`'); }}}
      // Implicit return WGPU_FALSE, WebGPU is not supported.
    });
  },
#else
  wgpu_sync_operations_pending: function() {
    return 0;
  },
#endif

  // A "_simple" variant of navigator_gpu_request_adapter_async() that does
  // not take in any descriptor params, for building tiny code with default
  // args and creating readable test cases etc.
  navigator_gpu_request_adapter_async_simple__deps: ['$wgpuStore'],
  navigator_gpu_request_adapter_async_simple: function(adapterCallback) {
    {{{ wdebuglog('`navigator_gpu_request_adapter_async_simple(adapterCallback=${adapterCallback})`'); }}}
    {{{ wassert('navigator["gpu"], "Your browser does not support WebGPU!"'); }}}
    navigator['gpu']['requestAdapter']().then(adapter => {
      // N.b. this function deliberately invokes a callback with signature 'vii',
      // the second integer parameter is intentionally passed as undefined and coerced to zero to save code bytes.
#if MEMORY64
      getWasmTableEntry(adapterCallback)(wgpuStore(adapter), 0n);
#else
      getWasmTableEntry(adapterCallback)(wgpuStore(adapter));
#endif
    });
  },

#if ASYNCIFY
  navigator_gpu_request_adapter_sync_simple__deps: ['$wgpuStoreAsyncifiedOp', '_wgpuNumAsyncifiedOperationsPending', '$wgpu_async'],
  navigator_gpu_request_adapter_sync_simple__sig: 'i',
  navigator_gpu_request_adapter_sync_simple__async: true,
  navigator_gpu_request_adapter_sync_simple: function() {
    return wgpu_async(() => {
      {{{ wdebuglog('`navigator_gpu_request_adapter_sync_simple()`'); }}}
      {{{ wassert('navigator["gpu"], "Your browser does not support WebGPU!"'); }}}
      ++__wgpuNumAsyncifiedOperationsPending;
      return navigator['gpu']['requestAdapter']().then(wgpuStoreAsyncifiedOp);
    });
  },
#endif

  navigator_gpu_get_preferred_canvas_format__deps: ['$GPUTextureAndVertexFormats'],
  navigator_gpu_get_preferred_canvas_format: function() {
    {{{ wdebuglog('`navigator_gpu_get_preferred_canvas_format()`'); }}}
    {{{ wassert('navigator["gpu"], "Your browser does not support WebGPU!"'); }}}

    {{{ wassert('GPUTextureAndVertexFormats.includes(navigator["gpu"]["getPreferredCanvasFormat"]())'); }}}
    return GPUTextureAndVertexFormats.indexOf(navigator['gpu']['getPreferredCanvasFormat']());
  },

  navigator_gpu_get_wgsl_language_features__deps: ['$stringToUTF8OnStack', '$stackAlloc'],
  navigator_gpu_get_wgsl_language_features: function() {
    var f = navigator['gpu']['wgslLanguageFeatures'];
    var i = {{{ toWasm64('0') }}};
    var wgpuSupportedWgslLanguageFeatures = stackAlloc((f.size+1) * 8); // 8 == sizeof(char*) in Wasm64 mode
    for(var feat of f.keys()) {
#if MEMORY64
      HEAPU64[BigInt(wgpuSupportedWgslLanguageFeatures) + i >> 3n] = BigInt(stringToUTF8OnStack(feat));
      i += 8n;
#else
      HEAPU32[wgpuSupportedWgslLanguageFeatures + i >> 2] = stringToUTF8OnStack(feat);
      i += 4;
#endif
    }
      // Null-terminate the list of wgsl language feature strings.
#if MEMORY64
    HEAPU64[BigInt(wgpuSupportedWgslLanguageFeatures) + i >> 3n] = 0n;
#else
    HEAPU32[wgpuSupportedWgslLanguageFeatures + i >> 2] = 0;
#endif
    return {{{ toWasm64('wgpuSupportedWgslLanguageFeatures') }}};
  },

  navigator_gpu_is_wgsl_language_feature_supported__deps: ['$utf8'],
  navigator_gpu_is_wgsl_language_feature_supported: function(feature) {
    return navigator['gpu']['wgslLanguageFeatures']['has'](utf8(feature));
  },

  wgpu_adapter_or_device_get_features__deps: ['wgpuFeatures'],
  wgpu_adapter_or_device_get_features: function(adapterOrDevice) {
    {{{ wdebuglog('`wgpu_adapter_or_device_get_features(adapterOrDevice: ${adapterOrDevice})`'); }}}
    {{{ wassert('adapterOrDevice != 0'); }}}
    {{{ wassert('wgpu[adapterOrDevice]'); }}}
    {{{ wassert('wgpu[adapterOrDevice] instanceof GPUAdapter || wgpu[adapterOrDevice] instanceof GPUDevice'); }}}
    let featuresBitMask = 0;

    {{{ wdebuglog('`The following adapter features are supported:`'); }}}

    _wgpuFeatures.forEach((feature, i) => {
      if (wgpu[adapterOrDevice]['features'].has(feature)) {
        {{{ wdebuglog('` - "${feature}", feature bit 0x${(1<<i).toString(16)}`'); }}}
        featuresBitMask |= 1 << i;
      }
    });
    return featuresBitMask;
  },

  wgpu_adapter_or_device_supports_feature__deps: ['wgpuFeatures'],
  wgpu_adapter_or_device_supports_feature: function(adapterOrDevice, feature) {
    {{{ wdebuglog('`wgpu_adapter_or_device_supports_feature(adapterOrDevice: ${adapterOrDevice}, feature: ${feature} == ${_wgpuFeatures[31 - Math.clz32(feature)]})`'); }}}
    {{{ wassert('adapterOrDevice != 0'); }}}
    {{{ wassert('(feature & (feature-1)) == 0'); }}} // Only call on a single feature at a time, not a bit combination of multiple features!
    {{{ wassert('wgpu[adapterOrDevice]'); }}}
    {{{ wassert('wgpu[adapterOrDevice] instanceof GPUAdapter || wgpu[adapterOrDevice] instanceof GPUDevice'); }}}
    return wgpu[adapterOrDevice]['features'].has(_wgpuFeatures[31 - Math.clz32(feature)])
  },

  wgpu_adapter_or_device_get_limits__deps: ['wgpu32BitLimitNames', 'wgpu64BitLimitNames', '$wgpuWriteI53ToU64HeapIdx'],
  wgpu_adapter_or_device_get_limits: function(adapterOrDevice, limits) {
    {{{ wdebuglog('`wgpu_adapter_or_device_get_limits(adapterOrDevice: ${adapterOrDevice}, limits: ${limits})`'); }}}
    {{{ wassert('limits != 0, "passed a null limits struct pointer"'); }}}
    {{{ wassert('adapterOrDevice != 0'); }}}
    {{{ wassert('wgpu[adapterOrDevice]'); }}}
    {{{ wassert('wgpu[adapterOrDevice] instanceof GPUAdapter || wgpu[adapterOrDevice] instanceof GPUDevice'); }}}

    let l = wgpu[adapterOrDevice]['limits'];

    {{{ replacePtrToIdx('limits', 2); }}}
    for(let limitName of _wgpu64BitLimitNames) {
      {{{ wassert('l[limitName] !== undefined, `Browser WebGPU implementation incorrect: it should advertise limit ${limitName}`'); }}}
      wgpuWriteI53ToU64HeapIdx(limits, l[limitName]);
      limits += 2;
    }

    for(let limitName of _wgpu32BitLimitNames) {
      HEAPU32[limits++] = l[limitName];
    }
  },

  $wgpuReadSupportedLimits__deps: ['wgpu32BitLimitNames', 'wgpu64BitLimitNames', '$wgpuReadI53FromU64HeapIdx'],
  $wgpuReadSupportedLimits: function(heap32Idx) {
    let requiredLimits = {}, v;

    // Assertion in lib_webgpu.h below struct WGpuSupportedLimits must match:
    {{{ wassert('_wgpu64BitLimitNames.length == 3, `Internal error: Number of uint64_t limit fields is not correct.`'); }}}
    {{{ wassert('_wgpu32BitLimitNames.length == 32, `Internal error: Number of uint32_t limit fields is not correct.`'); }}}

    // Marshal all the complex 64-bit quantities first ..
    for(let limitName of _wgpu64BitLimitNames) {
      if ((v = wgpuReadI53FromU64HeapIdx(heap32Idx))) requiredLimits[limitName] = v;
      heap32Idx += 2;
    }

    // .. followed by the 32-bit quantities.
    for(let limitName of _wgpu32BitLimitNames) {
      if ((v = HEAPU32[heap32Idx++])) requiredLimits[limitName] = v;
    }
    return requiredLimits;
  },

  $wgpuReadQueueDescriptor: function(heap32Idx) {
#if MEMORY64
    let v = HEAPU64[heap32Idx >>> 1]; return v ? { 'label': utf8(v) } : void 0;
#else
    let v = HEAPU32[heap32Idx]; return v ? { 'label': utf8(v) } : void 0;
#endif
  },

  $wgpuReadFeaturesBitfield__deps: ['wgpuFeatures'],
  $wgpuReadFeaturesBitfield: function(heap32Idx) {
    let requiredFeatures = [], v = HEAPU32[heap32Idx];

    {{{ wassert('_wgpuFeatures.length == 22'); }}} // Do not emit _wgpuFeatures.length in code below, to micro-optimize code size. Assert here this invariant holds.
    {{{ wassert('_wgpuFeatures.length <= 30'); }}} // We can only do up to 30 distinct feature bits here with the current code.

    _wgpuFeatures.forEach((f, i) => { if (v & (1 << i)) requiredFeatures.push(f); });
    return requiredFeatures;
  },

  $wgpuReadDeviceDescriptor__deps: ['$wgpuReadSupportedLimits', '$wgpuReadQueueDescriptor', '$wgpuReadFeaturesBitfield'],
  $wgpuReadDeviceDescriptor: function(descriptor) {
    {{{ wassert('descriptor != 0'); }}}
    {{{ replacePtrToIdx('descriptor', 2); }}}

    return {
      'requiredLimits': wgpuReadSupportedLimits(descriptor),
      'defaultQueue': wgpuReadQueueDescriptor(descriptor+38/*sizeof(WGpuSupportedLimits)*/), // TODO: Check code size here, redundant for release builds?
      'requiredFeatures': wgpuReadFeaturesBitfield(descriptor+40/*sizeof(WGpuSupportedLimits)+sizeof(WGpuQueueDescriptor)*/)
    };
  },

  wgpu_adapter_request_device_async__deps: ['$wgpuStoreAndSetParent', 'wgpuMuteJsExceptions', '$wgpuReadDeviceDescriptor'],
  wgpu_adapter_request_device_async__docs: '/** @suppress{checkTypes} */', // This function intentionally calls cb() without args.
  wgpu_adapter_request_device_async: function(adapter, descriptor, deviceCallback, userData) {
    {{{ wdebuglog('`wgpu_adapter_request_device_async(adapter: ${adapter}, deviceCallback: ${deviceCallback}, userData: ${userData})`'); }}}
    {{{ wassert('adapter != 0'); }}}
    {{{ wassert('wgpu[adapter]'); }}}
    {{{ wassert('wgpu[adapter] instanceof GPUAdapter'); }}}

    adapter = wgpu[adapter];
    let cb = device => {
      // If device is non-null, initialization succeeded.
      {{{ wdebugdir('device', '`adapter.requestDevice resolved with following device:`'); }}}
      // Register an ID for the queue of this newly created device (using a ?. if device initialization succeeded)
      wgpuStoreAndSetParent(device?.['queue'], device);
      {{{ makeDynCall('vip', 'deviceCallback') }}}(wgpuStoreAndSetParent(device, adapter), userData);
    };

    let desc = wgpuReadDeviceDescriptor(descriptor);

    {{{ wdebugdir('desc', '`GPUAdapter.requestDevice() with descriptor:`') }}};
    adapter['requestDevice'](desc)
      .then(_wgpuMuteJsExceptions(cb))
      .catch(
#if ASSERTIONS || globalThis.WEBGPU_DEBUG
      (e)=>{console.error(`GPUAdapter.requestDevice() Promise rejected: ${e}`); cb(/*intentionally omit arg to pass undefined*/)}
#else
      ()=>{cb(/*intentionally omit arg to pass undefined*/)}
#endif
    );
  },

#if ASYNCIFY
  wgpu_adapter_request_device_sync__deps: ['$wgpuStoreAndSetParent', '$wgpuStoreAsyncifiedOp', '$wgpuReadDeviceDescriptor', '_wgpuNumAsyncifiedOperationsPending', '$wgpu_async'],
  //wgpu_adapter_request_device_sync__sig: 'iip', // Iirc this would be needed for -sASYNCIFY=1 build mode, but this breaks -sMEMORY64=1 due to the detrimental automatic wrappers
  wgpu_adapter_request_device_sync__async: true,
  wgpu_adapter_request_device_sync: function(adapter, descriptor) {
    return wgpu_async(() => {
      {{{ wdebuglog('`wgpu_adapter_request_device_sync(adapter: ${adapter})`'); }}}
      {{{ wassert('adapter != 0'); }}}
      {{{ wassert('wgpu[adapter]'); }}}
      {{{ wassert('wgpu[adapter] instanceof GPUAdapter'); }}}

      adapter = wgpu[adapter];
      let cb = device => {
        // If device is non-null, initialization succeeded.
        {{{ wdebugdir('device', '`adapter.requestDevice resolved with following device:`'); }}}
      // Register an ID for the queue of this newly created device (using a ?. if device initialization succeeded)
        wgpuStoreAndSetParent(device?.['queue'], device);
        return wgpuStoreAsyncifiedOp(device, adapter);
      };

      ++__wgpuNumAsyncifiedOperationsPending;

      let desc = wgpuReadDeviceDescriptor(descriptor);

      {{{ wdebugdir('desc', '`GPUAdapter.requestDevice() with descriptor:`') }}};
      return adapter['requestDevice'](desc).then(cb);
    });
  },
#endif

  // A "_simple" variant of wgpu_adapter_request_device_async() that does
  // not take in any descriptor params, for building tiny code with default
  // args and creating readable test cases etc.
  wgpu_adapter_request_device_async_simple__deps: ['$wgpuStoreAndSetParent'],
  wgpu_adapter_request_device_async_simple: function(adapter, deviceCallback) {
    {{{ wdebuglog('`wgpu_adapter_request_device_async_simple(adapter: ${adapter}, deviceCallback=${deviceCallback})`'); }}}
    {{{ wassert('adapter != 0'); }}}
    {{{ wassert('wgpu[adapter]'); }}}
    {{{ wassert('wgpu[adapter] instanceof GPUAdapter'); }}}
    adapter = wgpu[adapter];
    adapter['requestDevice']().then(device => {
      // Register an ID for the queue of this newly created device (using a ?. if device initialization succeeded)
      wgpuStoreAndSetParent(device?.['queue'], device);
#if MEMORY64
      getWasmTableEntry(deviceCallback)(wgpuStoreAndSetParent(device, adapter), 0n);
#else
      // The second userData pointer is intentionally omitted here.
      {{{ makeDynCall('vip', 'deviceCallback') }}}(wgpuStoreAndSetParent(device, adapter));
#endif
    });
  },

#if ASYNCIFY
  wgpu_adapter_request_device_sync_simple__deps: ['$wgpuStoreAndSetParent', '$wgpuStoreAsyncifiedOp', '_wgpuNumAsyncifiedOperationsPending', '$wgpu_async'],
  wgpu_adapter_request_device_sync_simple__sig: 'ii',
  wgpu_adapter_request_device_sync_simple__async: true,
  wgpu_adapter_request_device_sync_simple: function(adapter) {
    return wgpu_async(() => {
      {{{ wdebuglog('`wgpu_adapter_request_device_sync_simple(adapter: ${adapter})`'); }}}
      {{{ wassert('adapter != 0'); }}}
      {{{ wassert('wgpu[adapter]'); }}}
      {{{ wassert('wgpu[adapter] instanceof GPUAdapter'); }}}
      ++__wgpuNumAsyncifiedOperationsPending;
      adapter = wgpu[adapter];
      return adapter['requestDevice']().then(device => {
        // Register an ID for the queue of this newly created device (using a ?. if device initialization succeeded)
        wgpuStoreAndSetParent(device?.['queue'], device);
        return wgpuStoreAsyncifiedOp(device, adapter);
      });
    });
  },
#endif

  wgpu_adapter_or_device_get_info__deps: ['$stringToUTF8'],
  wgpu_adapter_or_device_get_info: function(adapterOrDevice, infoPtr) {
    {{{ wdebuglog('`wgpu_adapter_or_device_get_info(adapterOrDevice: ${adapterOrDevice}, info: ${infoPtr})`'); }}}
    {{{ wassert('adapterOrDevice != 0'); }}}
    {{{ wassert('wgpu[adapterOrDevice]'); }}}
    {{{ wassert('wgpu[adapterOrDevice] instanceof GPUAdapter || wgpu[adapterOrDevice] instanceof GPUDevice'); }}}
    {{{ wassert('infoPtr != 0'); }}}
    var infoIdx = {{{ shiftPtr('infoPtr', 2) }}},
      infoByteIdx = {{{ shiftPtr('infoPtr', 0) }}},
      ao = wgpu[adapterOrDevice],
      adapterInfo = ao['adapterInfo'] || ao['info'];
    {{{ wdebugdir('adapterInfo', '`GPUAdapter.info is a member with following parameters:`'); }}}

    ['vendor','architecture','device','description'].forEach((f,i) => stringToUTF8(adapterInfo[f], infoByteIdx+i*512, 512));
    HEAPU32[infoIdx + 512] = adapterInfo['subgroupMinSize'];
    HEAPU32[infoIdx + 513] = adapterInfo['subgroupMaxSize'];
    HEAPU32[infoIdx + 514] = adapterInfo['isFallbackAdapter'];
  },

  wgpu_device_get_queue: function(device) {
    {{{ wdebuglog('`wgpu_device_get_queue(device=${device})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('wgpu[device].wid == device', "GPUDevice has lost its wid member field!"); }}}
    {{{ wassert('wgpu[device]["queue"].wid', "GPUDevice.queue must have been assigned an ID in function wgpu_adapter_request_device!"); }}}
    return wgpu[device]['queue'].wid;
  },

  $wgpuReadShaderModuleCompilationHints__deps: ['$GPUAutoLayoutMode'],
  $wgpuReadShaderModuleCompilationHints: function(index) {
    let numHints = HEAP32[index+2],
      hints = [],
      hintsIndex = {{{ readIdx32('index') }}},
      layout;
    {{{ wassert('numHints >= 0'); }}}
    while(numHints--) {
      layout = HEAPU32[hintsIndex+2];
      // layout == 0 (WGPU_AUTO_LAYOUT_MODE_NO_HINT) means no compilation hints are passed,
      // layout == 1 (WGPU_AUTO_LAYOUT_MODE_AUTO) means { layout: 'auto' } hint will be passed.
      // layout > 1: A handle to a given GPUPipelineLayout object is specified as a hint for creating the shader.
      // See https://github.com/gpuweb/gpuweb/pull/2876#issuecomment-1218341636
      {{{ wassert('layout <= 1 || wgpu[layout]'); }}}
      {{{ wassert('layout <= 1 || wgpu[layout] instanceof GPUPipelineLayout'); }}}
      hints.push({
        'entryPoint': utf8({{{ readPtrFromIdx32('hintsIndex') }}}),
        'layout': layout > 1 ? wgpu[layout] : (layout ? GPUAutoLayoutMode : void 0)
      });
      hintsIndex += 4;
    }
    return hints;
  },

  $wgpuReadShaderModuleDescriptor__deps: ['$wgpuReadShaderModuleCompilationHints'],
  $wgpuReadShaderModuleDescriptor: function(descriptor) {
    {{{ wassert('descriptor != 0'); }}}
    {{{ replacePtrToIdx('descriptor', 2); }}}
    return {
#if MEMORY64
      'code': utf8(HEAPU64[descriptor >>> 1]),
#else
      'code': utf8(HEAPU32[descriptor]),
#endif
      'compilationHints': wgpuReadShaderModuleCompilationHints(descriptor+2)
    }
  },

  wgpu_device_create_shader_module__deps: ['$wgpuStoreAndSetParent', '$wgpuReadShaderModuleDescriptor'],
  wgpu_device_create_shader_module: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_shader_module(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}

    device = wgpu[device];
    let desc = wgpuReadShaderModuleDescriptor(descriptor);
    {{{ wdebugdir('desc', '`device.createShaderModule() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createShaderModule'](desc), device);
  },

  wgpu_shader_module_get_compilation_info_async__deps: ['$lengthBytesUTF8', '$stringToUTF8', 'malloc'],
  wgpu_shader_module_get_compilation_info_async: function(shaderModule, callback, userData) {
    {{{ wdebuglog('`wgpu_shader_module_get_compilation_info_async(shaderModule=${shaderModule}, callback=${callback}, userData=${userData})`'); }}}
    {{{ wassert('shaderModule != 0'); }}}
    {{{ wassert('wgpu[shaderModule]'); }}}
    {{{ wassert('wgpu[shaderModule] instanceof GPUShaderModule'); }}}
    {{{ wassert('callback != 0'); }}}
    wgpu[shaderModule]['getCompilationInfo']().then(info => {
      {{{ wdebugdir('info', '`shaderModule.getCompilationInfo() completed with info:`'); }}}
      // To optimize marshalling, call into malloc() just once, and marshal the compilationInfo
      // object into one memory block, with the following layout:

      // A) 1 * struct WGpuCompilationInfo, followed by
      // B) info.messages.length * struct WGpuCompilationMessage, followed by
      // C) info.messages.length * null-terminated C strings for the messages.

      let msgs = info['messages'],
        len = msgs['length'],
        structLen = len * 32/*sizeof(WGpuCompilationMessage) */
                    + 8/*sizeof(WGpuCompilationInfo)*/,
        totalLen = structLen, msg, infoPtr, msgPtr, i;

      for(msg of msgs) totalLen += lengthBytesUTF8(msg['message']) + 1;

      infoPtr = _malloc(totalLen);
      msgPtr = infoPtr + structLen;
      i = {{{ shiftIndex('infoPtr', 2) }}};

      // Write A) struct WGpuCompilationInfo.
      HEAPU32[i] = len;
      i += 2; // sizeof(WGpuCompilationInfo)

      for(msg of msgs) {
        // Write B) struct WGpuCompilationMessage.
#if MEMORY64
        HEAPU64[i >>> 1] = BigInt(msgPtr);
#else
        HEAPU32[i] = msgPtr;
#endif
        {{{ wassert('["error","warning","info"].includes(msg["type"])'); }}}
        HEAPU32[i+2] = 'ewi'.indexOf(msg['type'][0]); // 'e'rror=0, 'w'arning=1, 'i'nfo=2
        HEAPU32[i+3] = msg['lineNum'];
        HEAPU32[i+4] = msg['linePos'];
        HEAPU32[i+5] = msg['offset'];
        HEAPU32[i+6] = msg['length'];

        // Write C) null-terminated C string for the message.
        msgPtr += stringToUTF8(msg['message'], msgPtr, 2**32) + 1;

        i += 8; // sizeof(WGpuCompilationMessage)
      }
      {{{ makeDynCall('vipp', 'callback') }}}(shaderModule, {{{ toWasm64('infoPtr') }}}, userData);
    });
  },

  wgpu_device_create_buffer__deps: ['$wgpuReadI53FromU64HeapIdx', '$wgpuStoreAndSetParent'],
  wgpu_device_create_buffer: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_buffer(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor != 0'); }}}
    device = wgpu[device];
    {{{ replacePtrToIdx('descriptor', 2); }}}

    let desc = {
      'size': wgpuReadI53FromU64HeapIdx(descriptor),
      'usage': HEAPU32[descriptor+2],
      'mappedAtCreation': !!HEAPU32[descriptor+3]
    };
    {{{ wdebugdir('desc', '`GPUDevice.createBuffer() with descriptor:`') }}};
    let buffer = device['createBuffer'](desc);

    // Add tracking space for mapped ranges
    buffer.mappedRanges = {};
    // Mark this object to be of type GPUBuffer for wgpu_device_create_bind_group().
    buffer.isBuffer = 1;
    return wgpuStoreAndSetParent(buffer, device);
  },

  wgpu_buffer_get_mapped_range: function(gpuBuffer, offset, size) {
    {{{ wdebuglog('`wgpu_buffer_get_mapped_range(gpuBuffer=${gpuBuffer}, offset=${offset}, size=${size})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(offset)'); }}}
    {{{ wassert('offset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= -1'); }}}

    {{{ wdebuglog("`gpuBuffer.getMappedRange(offset=${offset}, size=${size}):`"); }}}
    gpuBuffer = wgpu[gpuBuffer];
    try {
      gpuBuffer.mappedRanges[offset] = gpuBuffer['getMappedRange'](offset, size < 0 ? void 0 : size);
    } catch(e) {
      // E.g. if the GPU ran out of memory when creating a new buffer, this can fail.
      {{{ wdebugdir('e', '`gpuBuffer.getMappedRange() failed:`'); }}}
      return -1;
    }
    {{{ wdebugdir('gpuBuffer.mappedRanges[offset]', '`gpuBuffer.getMappedRange() returned:`'); }}}
    return offset;
  },

  wgpu_buffer_read_mapped_range: function(gpuBuffer, startOffset, subOffset, dst, size) {
    {{{ wdebuglog('`wgpu_buffer_read_mapped_range(gpuBuffer=${gpuBuffer}, startOffset=${startOffset}, subOffset=${subOffset}, dst=${dst}, size=${size})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('wgpu[gpuBuffer].mappedRanges[startOffset]', "wgpu_buffer_read_mapped_range: No such mapped range with specified startOffset!"); }}}
    {{{ wassert('Number.isSafeInteger(startOffset)'); }}}
    {{{ wassert('startOffset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(subOffset)'); }}}
    {{{ wassert('subOffset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= 0'); }}}
    {{{ wassert('dst || size == 0'); }}}

    // N.b. this generates garbage because JavaScript does not allow ArrayBufferView.set(ArrayBuffer, offset, size, dst)
    // but must create a dummy view.
    HEAPU8.set(new Uint8Array(wgpu[gpuBuffer].mappedRanges[startOffset], subOffset, size), {{{ shiftPtr('dst', 0) }}} );
  },

  wgpu_buffer_write_mapped_range: function(gpuBuffer, startOffset, subOffset, src, size) {
    {{{ wdebuglog('`wgpu_buffer_write_mapped_range(gpuBuffer=${gpuBuffer}, startOffset=${startOffset}, subOffset=${subOffset}, src=${src}, size=${size})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('wgpu[gpuBuffer].mappedRanges[startOffset]', "wgpu_buffer_write_mapped_range: No such mapped range with specified startOffset!"); }}}
    {{{ wassert('Number.isSafeInteger(startOffset)'); }}}
    {{{ wassert('startOffset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(subOffset)'); }}}
    {{{ wassert('subOffset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= 0'); }}}
    {{{ wassert('src || size == 0'); }}}

    // Here 'buffer' refers to the global Wasm memory buffer.
    // N.b. generates garbage.
    new Uint8Array(wgpu[gpuBuffer].mappedRanges[startOffset]).set(new Uint8Array(HEAPU8.buffer, {{{ shiftPtr('src', 0) }}}, size), subOffset);
  },

  wgpu_buffer_unmap: function(gpuBuffer) {
    {{{ wdebuglog('`wgpu_buffer_unmap(gpuBuffer=${gpuBuffer})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    gpuBuffer = wgpu[gpuBuffer];
    gpuBuffer['unmap']();

    // Let GC reclaim all previous getMappedRange()s for this buffer.
    gpuBuffer.mappedRanges = {};
  },

  wgpu_buffer_size: function(gpuBuffer) {
    {{{ wdebuglog('`wgpu_buffer_size(gpuBuffer=${gpuBuffer})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    return wgpu[gpuBuffer]['size'];
  },

  wgpu_buffer_usage: function(gpuBuffer) {
    {{{ wdebuglog('`wgpu_buffer_usage(gpuBuffer=${gpuBuffer})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    return wgpu[gpuBuffer]['usage'];
  },

  wgpu_buffer_map_state: function(gpuBuffer) {
    {{{ wdebuglog('`wgpu_buffer_map_state(gpuBuffer=${gpuBuffer})`'); }}}
    {{{ wassert('gpuBuffer != 0'); }}}
    {{{ wassert('wgpu[gpuBuffer]'); }}}
    {{{ wassert('wgpu[gpuBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('["unmapped","pending","mapped"].includes(wgpu[gpuBuffer]["mapState"])'); }}}
    return ' upm'.indexOf(wgpu[gpuBuffer]['mapState'][0]); // 'u'nmapped=1, 'p'ending=2, 'm'apped=3
  },

  $wgpuReadGpuStencilFaceState__deps: ['$GPUCompareFunctions', '$GPUStencilOperations'],
  $wgpuReadGpuStencilFaceState: function(idx) {
    {{{ wassert('idx != 0'); }}}
    return {
      'compare': GPUCompareFunctions[HEAPU32[idx]],
      'failOp': GPUStencilOperations[HEAPU32[idx+1]],
      'depthFailOp': GPUStencilOperations[HEAPU32[idx+2]],
      'passOp': GPUStencilOperations[HEAPU32[idx+3]]
    };
  },

  $wgpuReadGpuBlendComponent__deps: ['$GPUBlendOperations', '$GPUBlendFactors'],
  $wgpuReadGpuBlendComponent: function(idx) {
    {{{ wassert('idx != 0'); }}}
    {{{ wassert('GPUBlendOperations[HEAPU32[idx]]'); }}}
    {{{ wassert('GPUBlendFactors[HEAPU32[idx+1]]'); }}}
    {{{ wassert('GPUBlendFactors[HEAPU32[idx+2]]'); }}}
    return {
      'operation': GPUBlendOperations[HEAPU32[idx]],
      'srcFactor': GPUBlendFactors[HEAPU32[idx+1]],
      'dstFactor': GPUBlendFactors[HEAPU32[idx+2]]
    };
  },

  $wgpuReadRenderPipelineDescriptor__deps: ['$wgpuReadGpuStencilFaceState', '$wgpuReadGpuBlendComponent', '$wgpuReadI53FromU64HeapIdx', '$wgpuReadConstants', '$GPUIndexFormats', '$GPUTextureAndVertexFormats', '$GPUCompareFunctions', '$GPUPrimitiveTopologys', '$GPUAutoLayoutMode'],
  $wgpuReadRenderPipelineDescriptor: function(descriptor) {
    {{{ wassert('descriptor != 0'); }}}
    {{{ replacePtrToIdx('descriptor', 2); }}}

    let vertexBuffers = [],
        targets = [],
        vertexIdx = descriptor,
        numVertexBuffers = HEAP32[vertexIdx+7], // +7 == WGpuVertexState.numBuffers
        vertexBuffersIdx = {{{ readIdx32('vertexIdx+2') }}}, // +2 == WGpuVertexState.buffers
        primitiveIdx = vertexIdx + 10, // sizeof(WGpuVertexState)
        depthStencilIdx = primitiveIdx + 5, // sizeof(WGpuPrimitiveState)
        multisampleIdx = depthStencilIdx + 17, // sizeof(WGpuDepthStencilState)
        fragmentIdx = multisampleIdx + 4, // sizeof(WGpuMultisampleState) + 1 for unused padding
        numTargets = HEAP32[fragmentIdx+7], // +7 == WGpuFragmentState.numTargets
        targetsIdx = {{{ readIdx32('fragmentIdx+2') }}}, // +2 == WGpuFragmentState.targets
        depthStencilFormat = HEAPU32[depthStencilIdx],
        multisampleCount = HEAPU32[multisampleIdx],
        fragmentModule = HEAPU32[fragmentIdx+6],
        pipelineLayoutId = HEAPU32[fragmentIdx+10], // sizeof(WGpuPipelineLayout)
        desc;

    {{{ wassert('pipelineLayoutId <= 1/*"auto"*/ || wgpu[pipelineLayoutId]'); }}}
    {{{ wassert('pipelineLayoutId <= 1/*"auto"*/ || wgpu[pipelineLayoutId] instanceof GPUPipelineLayout'); }}}

    // Read GPUVertexState
    {{{ wassert('numVertexBuffers >= 0'); }}}
    while(numVertexBuffers--) {
      let attributes = [],
          numAttributes = HEAP32[vertexBuffersIdx+2],
          attributesIdx = {{{ readIdx32('vertexBuffersIdx') }}};
      {{{ wassert('numAttributes >= 0'); }}}
      while(numAttributes--) {
        attributes.push({
          'offset': wgpuReadI53FromU64HeapIdx(attributesIdx),
          'shaderLocation': HEAPU32[attributesIdx+2],
          'format': GPUTextureAndVertexFormats[HEAPU32[attributesIdx+3]]
        });
        attributesIdx += 4;
      }
      vertexBuffers.push({
        'arrayStride': wgpuReadI53FromU64HeapIdx(vertexBuffersIdx+4),
        'stepMode': [, 'vertex', 'instance'][HEAPU32[vertexBuffersIdx+3]],
        'attributes': attributes
      });
      vertexBuffersIdx += 6; // sizeof(WGpuVertexBufferLayout)
    }

    {{{ wassert('numTargets >= 0'); }}}
    while(numTargets--) {
      // If target format is 0 (WGPU_TEXTURE_FORMAT_INVALID), then this target
      // is sparse and specified as 'null'.
      let fmt = HEAPU32[targetsIdx];
      targets.push(fmt ? {
        'format': GPUTextureAndVertexFormats[fmt],
        'blend': HEAPU32[targetsIdx+1] ? {
          'color': wgpuReadGpuBlendComponent(targetsIdx+1),
          'alpha': wgpuReadGpuBlendComponent(targetsIdx+4)
        } : void 0,
        'writeMask': HEAPU32[targetsIdx+7]
      } : null);
      targetsIdx += 8;
    }

    desc = {
      'vertex': {
        'module': wgpu[HEAPU32[vertexIdx+6]],
        // If null pointer was passed to use the default entry point name, then utf8() would return '', but spec requires undefined.
        'entryPoint': utf8({{{ readPtrFromIdx32('vertexIdx') }}}) || void 0,
        'buffers': vertexBuffers,
        'constants': wgpuReadConstants({{{ readPtrFromIdx32('vertexIdx+4') }}}, HEAP32[vertexIdx+8])
      },
      'fragment': fragmentModule ? {
        'module': wgpu[fragmentModule],
        // If null pointer was passed to use the default entry point name, then utf8() would return '', but spec requires undefined.
        'entryPoint': utf8({{{ readPtrFromIdx32('fragmentIdx') }}}) || void 0,
        'targets': targets,
        'constants': wgpuReadConstants({{{ readPtrFromIdx32('fragmentIdx+4') }}}, HEAP32[fragmentIdx+8])
      } : void 0,
      'primitive': {
        'topology': GPUPrimitiveTopologys[HEAPU32[primitiveIdx]],
        'stripIndexFormat': GPUIndexFormats[HEAPU32[primitiveIdx+1]],
        'frontFace': [, 'ccw', 'cw'][HEAPU32[primitiveIdx+2]],
        'cullMode': [, 'none', 'front', 'back'][HEAPU32[primitiveIdx+3]],
        'unclippedDepth': !!HEAPU32[primitiveIdx+4]
      },
      'depthStencil': depthStencilFormat ? {
        'format': GPUTextureAndVertexFormats[depthStencilFormat],
        'depthWriteEnabled': !!HEAPU32[depthStencilIdx+1],
        'depthCompare': GPUCompareFunctions[HEAPU32[depthStencilIdx+2]],
        'stencilReadMask': HEAPU32[depthStencilIdx+3],
        'stencilWriteMask': HEAPU32[depthStencilIdx+4],
        'depthBias': HEAP32[depthStencilIdx+5],
        'depthBiasSlopeScale': HEAPF32[depthStencilIdx+6],
        'depthBiasClamp': HEAPF32[depthStencilIdx+7],
        'stencilFront': wgpuReadGpuStencilFaceState(depthStencilIdx+8),
        'stencilBack': wgpuReadGpuStencilFaceState(depthStencilIdx+12),
        'clampDepth': !!HEAPU32[depthStencilIdx+16],
      } : void 0,
      'multisample': multisampleCount ? {
        'count': multisampleCount,
        'mask': HEAPU32[multisampleIdx+1],
        'alphaToCoverageEnabled': !!HEAPU32[multisampleIdx+2]
      } : void 0,
      'layout': pipelineLayoutId > 1 ? wgpu[pipelineLayoutId] : GPUAutoLayoutMode
    };

    return desc;
  },

  wgpu_device_create_render_pipeline__deps: ['$wgpuReadRenderPipelineDescriptor', '$wgpuStoreAndSetParent'],
  wgpu_device_create_render_pipeline: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_render_pipeline(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor'); }}}

    device = wgpu[device];
    let desc = wgpuReadRenderPipelineDescriptor(descriptor);
    {{{ wdebugdir('desc', '`GPUDevice.createRenderPipeline() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createRenderPipeline'](desc), device);
  },

  $wgpuPipelineCreationFailed__deps: ['malloc', 'free', '$stringToNewUTF8'],
  $wgpuPipelineCreationFailed: function(device, callback, error, userData) {
    {{{ wdebugdir('error', '`createComputePipelineAsync failed with error:`'); }}}
    let e = _malloc(24); // n.b. Emscripten _malloc() export can be called with either BigInt or Number, and always returns a Number.
#if MEMORY64
    let eIdx = {{{ shiftIndex('e', 3) }}};
    let n = HEAPU64[eIdx]     = BigInt(stringToNewUTF8(error['name'])),
        m = HEAPU64[eIdx + 1] = BigInt(stringToNewUTF8(error['message'])),
        r = HEAPU64[eIdx + 2] = BigInt(stringToNewUTF8(error['reason']));
#else
    let eIdx = {{{ shiftPtr('e', 2) }}};
    let n = HEAPU32[eIdx]     = stringToNewUTF8(error['name']),
        m = HEAPU32[eIdx + 2] = stringToNewUTF8(error['message']),
        r = HEAPU32[eIdx + 4] = stringToNewUTF8(error['reason']);
#endif
    {{{ makeDynCall('vipip', 'callback') }}}(device, e, /*pipeline handle=*/0, userData);
    _free(r); _free(m); _free(n);
    _free(e);
  },

  wgpu_device_create_render_pipeline_async__deps: ['$wgpuReadRenderPipelineDescriptor', '$wgpuStoreAndSetParent', '$wgpuPipelineCreationFailed', 'wgpuMuteJsExceptions'],
  wgpu_device_create_render_pipeline_async__docs: '/** @suppress{checkTypes} */', // This function intentionally calls cb() without args.
  wgpu_device_create_render_pipeline_async: function(device, descriptor, callback, userData) {
    {{{ wdebuglog('`wgpu_device_create_render_pipeline_async(device=${device}, descriptor=${descriptor}, callback=${callback}, userData=${userData})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor'); }}}
    {{{ wassert('callback'); }}}
    let deviceObject = wgpu[device];

    let cb = (pipeline) => {
      {{{ wdebugdir('pipeline', '`createRenderPipelineAsync completed with pipeline:`'); }}}
      {{{ makeDynCall('vipip', 'callback') }}}(device, {{{ toWasm64('0') }}}, wgpuStoreAndSetParent(pipeline, deviceObject), userData);
    };

    let desc = wgpuReadRenderPipelineDescriptor(descriptor);
    {{{ wdebugdir('desc', '`GPUDevice.createRenderPipelineAsync() with descriptor:`') }}};
    deviceObject['createRenderPipelineAsync'](desc)
      .then(_wgpuMuteJsExceptions(cb))
      .catch(
#if ASSERTIONS || globalThis.WEBGPU_DEBUG
      (e)=>{console.error(`GPUDevice.createRenderPipelineAsync() Promise rejected: ${e}`); wgpuPipelineCreationFailed(device, callback, e, userData); }
#else
      ()=>{cb(/*intentionally omit arg to pass undefined*/)}
#endif
    );
  },

  wgpu_device_create_command_encoder__deps: ['$wgpuStoreAndSetParent'],
  wgpu_device_create_command_encoder: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_command_encoder(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    device = wgpu[device];
    return wgpuStoreAndSetParent(device['createCommandEncoder'](), device);
  },

  // A "_simple" variant of wgpu_device_create_command_encoder() that does
  // not take in any descriptor params, for building tiny code with default
  // args and creating readable test cases etc.
  wgpu_device_create_command_encoder_simple__deps: ['$wgpuStoreAndSetParent'],
  wgpu_device_create_command_encoder_simple: function(device) {
    device = wgpu[device];
    return wgpuStoreAndSetParent(device['createCommandEncoder'](), device);
  },

  wgpu_device_create_render_bundle_encoder__deps: ['$GPUTextureAndVertexFormats', '$wgpuStoreAndSetParent'],
  wgpu_device_create_render_bundle_encoder: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_render_bundle_encoder(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor != 0'); }}} // Must be non-null
    device = wgpu[device];
    {{{ replacePtrToIdx('descriptor', 2); }}}

    let colorFormats = [],
      numColorFormats = HEAP32[descriptor+2],
      colorFormatsPtr = {{{ readPtrFromIdx32('descriptor') }}},
      colorFormatsIdx = {{{ shiftPtr('colorFormatsPtr', 2) }}};

    {{{ wassert('numColorFormats >= 0'); }}}
    while(numColorFormats--) {
      // Color formats are allowed to be zero here, in which case a null/sparse target will be used in that slot.
      colorFormats.push(GPUTextureAndVertexFormats[HEAPU32[colorFormatsIdx++]]);
    }

    let desc = {
      'colorFormats': colorFormats,
      'depthStencilFormat': GPUTextureAndVertexFormats[HEAPU32[descriptor+3]],
      'sampleCount': HEAPU32[descriptor+4],
      'depthReadOnly': !!HEAPU32[descriptor+5],
      'stencilReadOnly': !!HEAPU32[descriptor+6]
    };
    {{{ wdebugdir('desc', '`GPUDevice.createRenderBundleEncoder() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createRenderBundleEncoder'](desc), device);
  },

  wgpu_device_create_query_set__deps: ['$GPUQueryTypes', '$wgpuStoreAndSetParent'],
  wgpu_device_create_query_set: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_query_set(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor != 0'); }}} // Must be non-null
    device = wgpu[device];
    {{{ replacePtrToIdx('descriptor', 2); }}}

    let desc = {
      'type': GPUQueryTypes[HEAPU32[descriptor]],
      'count': HEAPU32[descriptor+1],
    };
    {{{ wdebugdir('desc', '`GPUDevice.createQuerySet() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createQuerySet'](desc), device);
  },

  wgpu_buffer_map_async: function(buffer, callback, userData, mode, offset, size) {
    {{{ wdebuglog('`wgpu_buffer_map_async(buffer=${buffer}, callback=${callback}, userData=${userData}, mode=${mode}, offset=${offset}, size=${size})`'); }}}
    {{{ wassert('buffer != 0'); }}}
    {{{ wassert('wgpu[buffer]'); }}}
    {{{ wassert('wgpu[buffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(offset)'); }}}
    {{{ wassert('offset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= -1'); }}}

    wgpu[buffer]['mapAsync'](mode, offset, size < 0 ? void 0 : size).then(() => {{{ makeDynCall('vipidd', 'callback') }}}(buffer, userData, mode, offset, size));
  },

#if ASYNCIFY
  wgpu_buffer_map_sync__deps: ['_wgpuNumAsyncifiedOperationsPending', '$wgpu_async'],
  wgpu_buffer_map_sync__sig: 'iiidd',
  wgpu_buffer_map_sync__async: true,
  wgpu_buffer_map_sync: function(buffer, mode, offset, size) {
    return wgpu_async(() => {
      {{{ wdebuglog('`wgpu_buffer_map_sync(buffer=${buffer}, mode=${mode}, offset=${offset}, size=${size})`'); }}}
      {{{ wassert('buffer != 0'); }}}
      {{{ wassert('wgpu[buffer]'); }}}
      {{{ wassert('wgpu[buffer] instanceof GPUBuffer'); }}}
      {{{ wassert('Number.isSafeInteger(offset)'); }}}
      {{{ wassert('offset >= 0'); }}}
      {{{ wassert('Number.isSafeInteger(size)'); }}}
      {{{ wassert('size >= -1'); }}}

      buffer = wgpu[buffer];
      ++__wgpuNumAsyncifiedOperationsPending;

      return buffer['mapAsync'](mode, offset, size < 0 ? void 0 : size)
        .then(() => --__wgpuNumAsyncifiedOperationsPending);
    });
  },
#endif

  wgpu_device_create_texture__deps: ['$wgpuStoreAndSetParent', '$GPUTextureViewDimensions', '$GPUTextureAndVertexFormats', '$wgpuReadArrayOfItems'],
  wgpu_device_create_texture: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_texture(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor != 0'); }}} // Must be non-null
    device = wgpu[device];

    {{{ replacePtrToIdx('descriptor', 2); }}}
    {{{ wassert('HEAPU32[descriptor+8] >= 1'); }}} // 'dimension' must be one of 1d, 2d or 3d.
    {{{ wassert('HEAPU32[descriptor+8] <= 3'); }}} // 'dimension' must be one of 1d, 2d or 3d.

    let desc = {
      'viewFormats': wgpuReadArrayOfItems(GPUTextureAndVertexFormats, {{{ readPtrFromIdx32('descriptor') }}}, HEAPU32[descriptor+2]),
      'size': [HEAP32[descriptor+3], HEAP32[descriptor+4], HEAP32[descriptor+5]],
      'mipLevelCount': HEAP32[descriptor+6],
      'sampleCount': HEAP32[descriptor+7],
      'dimension': HEAPU32[descriptor+8] + 'd',
      'format': GPUTextureAndVertexFormats[HEAPU32[descriptor+9]],
      'usage': HEAPU32[descriptor+10],
      'textureBindingViewDimension': GPUTextureViewDimensions[HEAPU32[descriptor+11]] // Only used in WebGPU compatibility mode, ignored by core devices.
    };
    {{{ wdebugdir('desc', '`GPUDevice.createTexture() with descriptor:`'); }}}
    let texture = device['createTexture'](desc);

    return wgpuStoreAndSetParent(texture, device);
  },

  wgpu_device_create_sampler__deps: ['$wgpuStoreAndSetParent', '$GPUAddressModes', '$GPUFilterModes', '$GPUCompareFunctions'],
  wgpu_device_create_sampler: function(device, descriptor) {
    {{{ wdebuglog('`wgpu_device_create_sampler(device=${device}, descriptor=${descriptor})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    device = wgpu[device];

    {{{ replacePtrToIdx('descriptor', 2); }}}
    let desc = descriptor ? {
      'addressModeU': GPUAddressModes[HEAPU32[descriptor]],
      'addressModeV': GPUAddressModes[HEAPU32[descriptor+1]],
      'addressModeW': GPUAddressModes[HEAPU32[descriptor+2]],
      'magFilter': GPUFilterModes[HEAPU32[descriptor+3]],
      'minFilter': GPUFilterModes[HEAPU32[descriptor+4]],
      // N.b. The field below technically uses '$GPUMipmapFilterModes', but use '$GPUFilterModes' instead to save code size, since these two have identical contents
      'mipmapFilter': GPUFilterModes[HEAPU32[descriptor+5]],
      'lodMinClamp': HEAPF32[descriptor+6],
      'lodMaxClamp': HEAPF32[descriptor+7],
      'compare': GPUCompareFunctions[HEAPU32[descriptor+8]],
      'maxAnisotropy': HEAPU32[descriptor+9]
    } : void 0;
    {{{ wdebugdir('desc', '`GPUDevice.createSampler() with descriptor:`'); }}}

    return wgpuStoreAndSetParent(device['createSampler'](desc), device);
  },

  // This is a JavaScript facing API function for calling GPUDevice.importExternalTexture().
  // Returns a integer handle to the imported texture that can be passed to Wasm code.
  // device: a integer handle to the WebGPU device, obtained from Wasm code.
  // descriptor: a JSON object of type GPUExternalTextureDescriptor
  wgpuDeviceImportExternalTexture__deps: ['$wgpuStoreAndSetParent'],
  wgpuDeviceImportExternalTexture: function(device, descriptor) {
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor'); }}}
    {{{ wassert('descriptor["source"]'); }}}
    device = wgpu[device];

    return wgpuStoreAndSetParent(device['importExternalTexture'](descriptor), device);
  },

  // This is a Wasm facing API function for calling GPUDevice.importExternalTexture().
  // Returns a integer handle to the imported texture that can be used in Wasm code.
  // device: a integer handle to the WebGPU device.
  // descriptor: a pointer to a GPUExternalTextureDescriptor struct in Wasm heap
  wgpu_device_import_external_texture: function(device, descriptor) {
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('descriptor'); }}}

    {{{ replacePtrToIdx('descriptor', 2); }}}

    {{{ wassert('wgpu[HEAPU32[descriptor]]'); }}}
    {{{ wassert('wgpu[HEAPU32[descriptor]] instanceof HTMLVideoElement'); }}}

    device = wgpu[device];

    return wgpuStoreAndSetParent(device['importExternalTexture']({
      'source': wgpu[HEAPU32[descriptor]]
      // TODO: If/when GPUExternalTextureDescriptor.colorSpace field gains other values than 'srgb', add reading those fields in here.
    }), device);
  },

  $wgpuReadBindGroupLayoutDescriptor__deps: ['$GPUBufferBindingTypes', '$wgpuReadI53FromU64HeapIdx', '$GPUSamplerBindingTypes', '$GPUTextureSampleTypes', '$GPUTextureViewDimensions', '$GPUTextureAndVertexFormats', '$GPUStorageTextureSampleTypes'],
  $wgpuReadBindGroupLayoutDescriptor: function(entries, numEntries) {
    {{{ wassert('numEntries >= 0'); }}}
    {{{ wassert('entries != 0 || numEntries == 0'); }}} // Must be non-null pointer

    {{{ replacePtrToIdx('entries', 2); }}}
    let e = [];
    while(numEntries--) {
      let entry = {
        'binding': HEAPU32[entries],
        'visibility': HEAPU32[entries+1],
      }, type = HEAPU32[entries+2];
      entries += 4;
      {{{ wassert('type >= 1 && type <= 5'); }}}
      if (type == 1/*WGPU_BIND_GROUP_LAYOUT_TYPE_BUFFER*/) {
        entry['buffer'] = {
          'type': GPUBufferBindingTypes[HEAPU32[entries]],
          'hasDynamicOffset': !!HEAPU32[entries+1],
          'minBindingSize': wgpuReadI53FromU64HeapIdx(entries+2)
        };
      } else if (type == 2/*WGPU_BIND_GROUP_LAYOUT_TYPE_SAMPLER*/) {
        entry['sampler'] = {
          'type': GPUSamplerBindingTypes[HEAPU32[entries]]
        };
      } else if (type == 3/*WGPU_BIND_GROUP_LAYOUT_TYPE_TEXTURE*/) {
        entry['texture'] = {
          'sampleType': GPUTextureSampleTypes[HEAPU32[entries]],
          'viewDimension': GPUTextureViewDimensions[HEAPU32[entries+1]],
          'multisampled': !!HEAPU32[entries+2]
        };
      } else if (type == 4/*WGPU_BIND_GROUP_LAYOUT_TYPE_STORAGE_TEXTURE*/) {
        entry['storageTexture'] = {
          'access': GPUStorageTextureSampleTypes[HEAPU32[entries]],
          'format': GPUTextureAndVertexFormats[HEAPU32[entries+1]],
          'viewDimension': GPUTextureViewDimensions[HEAPU32[entries+2]]
        };
      } else { // type == 5/*WGPU_BIND_GROUP_LAYOUT_TYPE_EXTERNAL_TEXTURE*/ {
        entry['externalTexture'] = {};
      }
      entries += 4;
      e.push(entry);
    }
    return {
      'entries': e
    }
  },

  wgpu_device_create_bind_group_layout__deps: ['$wgpuStoreAndSetParent', '$wgpuReadBindGroupLayoutDescriptor'],
  wgpu_device_create_bind_group_layout: function(device, entries, numEntries) {
    {{{ wdebuglog('`wgpu_device_create_bind_group_layout(device=${device}, entries=${entries}, numEntries=${numEntries})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    device = wgpu[device];

    let desc = wgpuReadBindGroupLayoutDescriptor(entries, numEntries);
    {{{ wdebugdir('desc', '`GPUDevice.createBindGroupLayout() with descriptor:`'); }}}
    return wgpuStoreAndSetParent(device['createBindGroupLayout'](desc), device);
  },

  wgpu_device_create_pipeline_layout__deps: ['$wgpuStoreAndSetParent', '$wgpuReadArrayOfItemsMaybeNull'],
  wgpu_device_create_pipeline_layout: function(device, layouts, numLayouts) {
    {{{ wdebuglog('`wgpu_device_create_pipeline_layout(device=${device}, layouts=${layouts}, numLayouts=${numLayouts})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    device = wgpu[device];

    let desc = {
      'bindGroupLayouts': wgpuReadArrayOfItemsMaybeNull(wgpu, layouts, numLayouts)
    };
    {{{ wdebugdir('desc', '`GPUDevice.createPipelineLayout() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createPipelineLayout'](desc), device);
  },

  wgpu_device_create_bind_group__deps: ['$wgpuStoreAndSetParent', '$wgpuReadI53FromU64HeapIdx'],
  wgpu_device_create_bind_group: function(device, layout, entries, numEntries) {
    {{{ wdebuglog('`wgpu_device_create_bind_group(device=${device}, layout=${layout}, entries=${entries}, numEntries=${numEntries})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('layout != 0'); }}} // Must be a valid BindGroupLayout
    {{{ wassert('layout > 1'); }}} // Cannot pass WGPU_AUTO_LAYOUT_MODE_NO_HINT or WGPU_AUTO_LAYOUT_MODE_AUTO to this function
    {{{ wassert('wgpu[layout]'); }}}
    {{{ wassert('wgpu[layout] instanceof GPUBindGroupLayout'); }}}
    {{{ wassert('numEntries >= 0'); }}}
    {{{ wassert('entries != 0 || numEntries == 0'); }}} // Must be non-null pointer
    device = wgpu[device];
    {{{ replacePtrToIdx('entries', 2); }}}
    let e = [];
    while(numEntries--) {
      let resource = wgpu[HEAPU32[entries + 1]];
      {{{ wassert('resource'); }}}
      e.push({
        'binding': HEAPU32[entries],
        'resource': resource.isBuffer ? {
          'buffer': resource,
          'offset': wgpuReadI53FromU64HeapIdx(entries + 2),
          'size': wgpuReadI53FromU64HeapIdx(entries + 4) || void 0 // Awkward polymorphism: convert size=0 to 'undefined' to mean to bind the whole buffer.
        } : resource,
      });
      entries += 6;
    }

    let desc = {
      'layout': wgpu[layout],
      'entries': e
    };
    {{{ wdebugdir('desc', '`GPUDevice.createBindGroup() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createBindGroup'](desc), device);
  },

  $wgpuReadConstants: function(constants, numConstants) {
    {{{ wassert('numConstants >= 0'); }}}
    {{{ wassert('constants != 0 || numConstants == 0'); }}}

    let c = {};
    while(numConstants--) {
      c[utf8({{{ readPtr('constants') }}})] = HEAPF64[{{{ shiftPtr('constants + ' + toWasm64('8'), 3) }}}];
      constants += {{{ toWasm64('16') }}};
    }
    return c;
  },

  $wgpuReadComputePipelineDescriptor__deps: ['$wgpuReadConstants', '$GPUAutoLayoutMode'],
  $wgpuReadComputePipelineDescriptor: function(computeModule, entryPoint, layout, constants, numConstants) {
    return {
      'layout': layout > 1 ? wgpu[layout] : GPUAutoLayoutMode,
      'compute': {
        'module': wgpu[computeModule],
        'entryPoint': utf8(entryPoint) || void 0, // If null pointer was passed to use the default entry point name, then utf8() would return '', but spec requires undefined.
        'constants': wgpuReadConstants(constants, numConstants)
      }
    };
  },

  wgpu_device_create_compute_pipeline__deps: ['$wgpuStoreAndSetParent', '$wgpuReadComputePipelineDescriptor'],
  wgpu_device_create_compute_pipeline: function(device, computeModule, entryPoint, layout, constants, numConstants) {
    {{{ wdebuglog('`wgpu_device_create_compute_pipeline(device=${device}, computeModule=${computeModule}, entryPoint=${entryPoint}, layout=${layout}, constants=${constants}, numConstants=${numConstants})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('computeModule != 0'); }}}
    {{{ wassert('wgpu[computeModule]'); }}}
    {{{ wassert('wgpu[computeModule] instanceof GPUShaderModule'); }}}
    {{{ wassert('layout <= 1/*"auto"*/ || wgpu[layout]'); }}}
    {{{ wassert('layout <= 1/*"auto"*/ || wgpu[layout] instanceof GPUPipelineLayout'); }}}
    {{{ wassert('numConstants >= 0'); }}}
    {{{ wassert('numConstants == 0 || constants'); }}}
    {{{ wassert('!entryPoint || utf8(entryPoint).length > 0'); }}} // If entry point string is provided, it must be a nonempty JS string
    device = wgpu[device];

    let desc = wgpuReadComputePipelineDescriptor(computeModule, entryPoint, layout, constants, numConstants);
    {{{ wdebugdir('desc', '`GPUDevice.createComputePipeline() with descriptor:`') }}};
    return wgpuStoreAndSetParent(device['createComputePipeline'](desc), device);
  },

  wgpu_device_create_compute_pipeline_async__deps: ['$wgpuStoreAndSetParent', '$wgpuReadComputePipelineDescriptor', '$wgpuPipelineCreationFailed', 'wgpuMuteJsExceptions'],
  wgpu_device_create_compute_pipeline_async__docs: '/** @suppress{checkTypes} */', // This function intentionally calls cb() without args.
  wgpu_device_create_compute_pipeline_async: function(device, computeModule, entryPoint, layout, constants, numConstants, callback, userData) {
    {{{ wdebuglog('`wgpu_device_create_compute_pipeline_async(device=${device}, computeModule=${computeModule}, entryPoint=${entryPoint}, layout=${layout}, constants=${constants}, numConstants=${numConstants}, callback=${callback}, userData=${userData})`'); }}}
    {{{ wassert('device != 0'); }}}
    {{{ wassert('wgpu[device]'); }}}
    {{{ wassert('wgpu[device] instanceof GPUDevice'); }}}
    {{{ wassert('computeModule != 0'); }}}
    {{{ wassert('wgpu[computeModule]'); }}}
    {{{ wassert('wgpu[computeModule] instanceof GPUShaderModule'); }}}
    {{{ wassert('layout <= 1/*"auto"*/ || wgpu[layout]'); }}}
    {{{ wassert('layout <= 1/*"auto"*/ || wgpu[layout] instanceof GPUPipelineLayout'); }}}
    {{{ wassert('numConstants >= 0'); }}}
    {{{ wassert('numConstants == 0 || constants'); }}}
    {{{ wassert('!entryPoint || utf8(entryPoint).length > 0'); }}} // If entry point string is provided, it must be a nonempty JS string
    {{{ wassert('callback'); }}}
    let deviceObject = wgpu[device];

    let cb = (pipeline) => {
      {{{ wdebugdir('pipeline', '`createComputePipelineAsync succeeded with pipeline:`'); }}}
      {{{ makeDynCall('vipip', 'callback') }}}(device, /*error*/ {{{ toWasm64('0') }}}, wgpuStoreAndSetParent(pipeline, deviceObject), userData);
    };

    let desc = wgpuReadComputePipelineDescriptor(computeModule, entryPoint, layout, constants, numConstants);
    {{{ wdebugdir('desc', '`GPUDevice.createComputePipelineAsync() with descriptor:`') }}};

    deviceObject['createComputePipelineAsync'](desc)
      .then(_wgpuMuteJsExceptions(cb))
      .catch(
#if ASSERTIONS || globalThis.WEBGPU_DEBUG
      (e)=>{console.error(`GPUDevice.createComputePipelineAsync() Promise rejected: ${e}`); wgpuPipelineCreationFailed(device, callback, e, userData); }
#else
      ()=>{cb(/*intentionally omit arg to pass undefined*/)}
#endif
    );
  },

  wgpu_texture_create_view__deps: ['$wgpuStoreAndSetParent', '$GPUTextureAndVertexFormats', '$GPUTextureViewDimensions', '$GPUTextureAspects'],
  wgpu_texture_create_view: function(texture, descriptor) {
    {{{ wdebuglog('`wgpu_texture_create_view(texture=${texture}, descriptor=${descriptor})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    texture = wgpu[texture];

    var descriptorIdx = {{{ shiftPtr('descriptor', 2) }}},
      descriptorByteIdx = {{{ shiftPtr('descriptor', 0) }}};

    let desc = descriptorIdx ? {
      'format': GPUTextureAndVertexFormats[HEAPU32[descriptorIdx]],
      'dimension': GPUTextureViewDimensions[HEAPU32[descriptorIdx+1]],
      'usage': HEAPU32[descriptorIdx+2],
      'aspect': GPUTextureAspects[HEAPU32[descriptorIdx+3]],
      'baseMipLevel': HEAP32[descriptorIdx+4],
      'mipLevelCount': HEAP32[descriptorIdx+5],
      'baseArrayLayer': HEAP32[descriptorIdx+6],
      'arrayLayerCount': HEAP32[descriptorIdx+7],
      'swizzle': UTF8ToString(descriptorByteIdx + 32) || 'rgba'
    } : void 0;
    {{{ wdebugdir('desc', '`GPUTexture.createView() with descriptor:`') }}};
    return wgpuStoreAndSetParent(texture['createView'](desc), texture);
  },

  // A "_simple" variant of wgpu_texture_create_view() that does
  // not take in any descriptor params, for building tiny code with default
  // args and creating readable test cases etc.
  wgpu_texture_create_view_simple__deps: ['$wgpuStoreAndSetParent'],
  wgpu_texture_create_view_simple: function(texture) {
    {{{ wdebuglog('`wgpu_texture_create_view_simple(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    texture = wgpu[texture];
    return wgpuStoreAndSetParent(texture['createView'](), texture);
  },

  wgpu_texture_width: function(texture) {
    {{{ wdebuglog('`wgpu_texture_width(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return wgpu[texture]['width'];
  },

  wgpu_texture_height: function(texture) {
    {{{ wdebuglog('`wgpu_texture_height(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return wgpu[texture]['height'];
  },

  wgpu_texture_depth_or_array_layers: function(texture) {
    {{{ wdebuglog('`wgpu_texture_depth_or_array_layers(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return wgpu[texture]['depthOrArrayLayers'];
  },

  wgpu_texture_mip_level_count: function(texture) {
    {{{ wdebuglog('`wgpu_texture_mip_level_count(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return wgpu[texture]['mipLevelCount'];
  },

  wgpu_texture_sample_count: function(texture) {
    {{{ wdebuglog('`wgpu_texture_sample_count(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return wgpu[texture]['sampleCount'];
  },

#if ASSERTIONS || globalThis.WEBGPU_DEBUG
  wgpu_texture_dimension__deps: ['$GPUTextureDimensions'],
#endif
  wgpu_texture_dimension: function(texture) {
    {{{ wdebuglog('`wgpu_texture_dimension(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    {{{ wassert('GPUTextureDimensions.indexOf(wgpu[texture]["dimension"]) != -1'); }}}
    {{{ wassert('GPUTextureDimensions.indexOf(wgpu[texture]["dimension"]) == +wgpu[texture]["dimension"][0]'); }}}
    // N.b. instead of indexing to the string array, e.g.
    // return GPUTextureDimensions.indexOf(wgpu[texture]['dimension']);
    // Look up the enum value arithmetically.
    return +wgpu[texture]['dimension'][0];
  },

  wgpu_texture_format__deps: ['$GPUTextureAndVertexFormats'],
  wgpu_texture_format: function(texture) {
    {{{ wdebuglog('`wgpu_texture_format(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    {{{ wassert('GPUTextureAndVertexFormats.indexOf(wgpu[texture]["format"]) != -1'); }}}
    return GPUTextureAndVertexFormats.indexOf(wgpu[texture]['format']);
  },

  wgpu_texture_usage: function(texture) {
    {{{ wdebuglog('`wgpu_texture_usage(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return wgpu[texture]['usage'];
  },

  wgpu_texture_binding_view_dimension: function(texture) {
    {{{ wdebuglog('`wgpu_texture_binding_view_dimension(texture=${texture})`'); }}}
    {{{ wassert('texture != 0'); }}}
    {{{ wassert('wgpu[texture]'); }}}
    {{{ wassert('wgpu[texture] instanceof GPUTexture'); }}}
    return GPUTextureViewDimensions.indexOf(wgpu[texture]['textureBindingViewDimension']);
  },

  wgpu_pipeline_get_bind_group_layout: function(pipelineBase, index) {
    {{{ wdebuglog('`wgpu_pipeline_get_bind_group_layout(pipelineBase=${pipelineBase}, index=${index})`'); }}}
    {{{ wassert('pipelineBase != 0'); }}}
    {{{ wassert('wgpu[pipelineBase]'); }}}
    {{{ wassert('wgpu[pipelineBase] instanceof GPURenderPipeline || wgpu[pipelineBase] instanceof GPUComputePipeline'); }}}
    pipelineBase = wgpu[pipelineBase];
    return wgpuStoreAndSetParent(pipelineBase['getBindGroupLayout'](index), pipelineBase);
  },

  $wgpuReadTimestampWrites: function(timestampWritesIndex) {
    let querySet = HEAPU32[timestampWritesIndex];
    if (querySet) {
      let timestampWrites = { 'querySet': wgpu[querySet] }, i;
      if ((i = HEAP32[timestampWritesIndex+1]) >= 0) timestampWrites['beginningOfPassWriteIndex'] = i;
      if ((i = HEAP32[timestampWritesIndex+2]) >= 0) timestampWrites['endOfPassWriteIndex'] = i;
      return timestampWrites;
    }
  },

  $wgpuReadRenderPassDepthStencilAttachment: function(heap32Idx) {
    let v = HEAPU32[heap32Idx]; return v ? {
        'view': wgpu[v],
        'depthLoadOp': GPULoadOps[HEAPU32[heap32Idx+1]],
        'depthClearValue': HEAPF32[heap32Idx+2],
        'depthStoreOp': GPUStoreOps[HEAPU32[heap32Idx+3]],
        'depthReadOnly': !!HEAPU32[heap32Idx+4],
        'stencilLoadOp': GPULoadOps[HEAPU32[heap32Idx+5]],
        'stencilClearValue': HEAPU32[heap32Idx+6],
        'stencilStoreOp': GPUStoreOps[HEAPU32[heap32Idx+7]],
        'stencilReadOnly': !!HEAPU32[heap32Idx+8],
      } : void 0;
  },

  wgpu_command_encoder_begin_render_pass__deps: ['$GPULoadOps', '$GPUStoreOps', '$wgpuReadTimestampWrites', '$wgpuReadRenderPassDepthStencilAttachment', '$wgpuStore'],
  wgpu_command_encoder_begin_render_pass: function(commandEncoder, descriptor) {
    {{{ wdebuglog('`wgpu_command_encoder_begin_render_pass(commandEncoder=${commandEncoder}, descriptor=${descriptor})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('descriptor != 0'); }}}

    {{{ replacePtrToIdx('descriptor', 2); }}}

    let colorAttachments = [],
      numColorAttachments = HEAP32[descriptor+4],
      colorAttachmentsIdx = {{{ readIdx32('descriptor+2') }}},
      colorAttachmentsIdxDbl = {{{ shiftIndex('colorAttachmentsIdx + 6', 1) }}}, // Alias the view for HEAPF64.
      maxDrawCount = HEAPF64[{{{ shiftIndex('descriptor', 1) }}}],
      depthStencilAttachment = HEAPU32[descriptor+5];

    {{{ wassert('Number.isSafeInteger(maxDrawCount)'); }}} // 'maxDrawCount' is a double_int53_t
    {{{ wassert('maxDrawCount >= 0'); }}}

    {{{ wassert('colorAttachmentsIdx % 2 == 0'); }}} // Must be aligned at double boundary
    {{{ wassert('depthStencilAttachment == 0 || wgpu[depthStencilAttachment] instanceof GPUTexture || wgpu[depthStencilAttachment] instanceof GPUTextureView'); }}} // Must point to a valid WebGPU texture or texture view object if nonzero

    {{{ wassert('numColorAttachments >= 0'); }}}
    while(numColorAttachments--) {
      // If view is 0, then this attachment is to be sparse.
      let v = HEAPU32[colorAttachmentsIdx], ds = HEAP32[colorAttachmentsIdx+1];
      colorAttachments.push(v ? {
        'view': wgpu[v],
        'depthSlice': ds < 0 ? void 0 : ds, // Awkward polymorphism: spec does not allow 'depthSlice' to be given a value (even 0) if attachment is not a 3D texture.
        'resolveTarget': wgpu[HEAPU32[colorAttachmentsIdx+2]],
        'storeOp': GPUStoreOps[HEAPU32[colorAttachmentsIdx+3]],
        'loadOp': GPULoadOps[HEAPU32[colorAttachmentsIdx+4]],
        'clearValue': [HEAPF64[colorAttachmentsIdxDbl  ], HEAPF64[colorAttachmentsIdxDbl+1],
                       HEAPF64[colorAttachmentsIdxDbl+2], HEAPF64[colorAttachmentsIdxDbl+3]]
      } : null);

      colorAttachmentsIdx += 14; // sizeof(WGpuRenderPassColorAttachment)
      colorAttachmentsIdxDbl += 7; // sizeof(WGpuRenderPassColorAttachment)/2
    }

    let desc = {
      'colorAttachments': colorAttachments,
      // Awkward polymorphism: cannot specify 'view': undefined if no depth-stencil attachment
      // is to be present, but must pass undefined as the whole attachment object.
      'depthStencilAttachment': wgpuReadRenderPassDepthStencilAttachment(descriptor+5),
      'occlusionQuerySet': wgpu[HEAPU32[descriptor+14]], // 5 + 9==sizeof(WGpuRenderPassDepthStencilAttachment)
      // If maxDrawCount is set to zero, pass in undefined to use the default value
      // (likely 50 million, but omit it in case the spec might change in the future)
      'maxDrawCount': maxDrawCount || void 0,
      'timestampWrites': wgpuReadTimestampWrites(descriptor+15) // 5 + 9==sizeof(WGpuRenderPassDepthStencilAttachment) + 1==sizeof(WGpuQuerySet)
    };
    {{{ wdebugdir('desc', '`GPUCommandEncoder.beginRenderPass() with descriptor:`') }}};
    return wgpuStore(wgpu[commandEncoder]['beginRenderPass'](desc));
  },

  wgpu_command_encoder_begin_compute_pass__deps: ['$wgpuReadTimestampWrites', '$wgpuStore'],
  wgpu_command_encoder_begin_compute_pass: function(commandEncoder, descriptor) {
    {{{ wdebuglog('`wgpu_command_encoder_begin_compute_pass(commandEncoder=${commandEncoder}, descriptor=${descriptor})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    // descriptor may be a null pointer

    commandEncoder = wgpu[commandEncoder];
    {{{ replacePtrToIdx('descriptor', 2); }}}

    let desc = descriptor ? {
      'timestampWrites': wgpuReadTimestampWrites(descriptor)
    } : void 0;

    {{{ wdebugdir('desc', '`GPUCommandEncoder.beginComputePass() with descriptor:`'); }}}
    return wgpuStore(commandEncoder['beginComputePass'](desc));
  },

  wgpu_command_encoder_copy_buffer_to_buffer: function(commandEncoder, source, sourceOffset, destination, destinationOffset, size) {
    {{{ wdebuglog('`wgpu_command_encoder_copy_buffer_to_buffer(commandEncoder=${commandEncoder}, source=${source}, sourceOffset=${sourceOffset}, destination=${destination}, destinationOffset=${destinationOffset}, size=${size})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('wgpu[source] instanceof GPUBuffer'); }}}
    {{{ wassert('wgpu[destination] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(sourceOffset)'); }}}
    {{{ wassert('sourceOffset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(destinationOffset)'); }}}
    {{{ wassert('destinationOffset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size) || size == Infinity'); }}}
    {{{ wassert('size >= 0'); }}}
    wgpu[commandEncoder]['copyBufferToBuffer'](wgpu[source], sourceOffset, wgpu[destination], destinationOffset, size < 1/0 ? size : void 0);
  },

  $wgpuReadGpuTexelCopyBufferInfo__deps: ['$wgpuReadI53FromU64HeapIdx'],
  $wgpuReadGpuTexelCopyBufferInfo: function(ptr) {
    {{{ wassert('ptr != 0'); }}}
    {{{ replacePtrToIdx('ptr', 2); }}}
    return {
      'offset': wgpuReadI53FromU64HeapIdx(ptr),
      'bytesPerRow': HEAP32[ptr+2],
      'rowsPerImage': HEAP32[ptr+3],
      'buffer': wgpu[HEAPU32[ptr+4]]
    };
  },

  $wgpuReadGpuTexelCopyTextureInfo__deps: ['$GPUTextureAspects'],
  $wgpuReadGpuTexelCopyTextureInfo: function(ptr) {
    {{{ wassert('ptr'); }}}
    {{{ replacePtrToIdx('ptr', 2); }}}
    return {
      'texture': wgpu[HEAPU32[ptr]],
      'mipLevel': HEAP32[ptr+1],
      'origin': [HEAP32[ptr+2], HEAP32[ptr+3], HEAP32[ptr+4]],
      'aspect': GPUTextureAspects[HEAPU32[ptr+5]]
    };
  },

  wgpu_command_encoder_copy_buffer_to_texture__deps: ['$wgpuReadGpuTexelCopyBufferInfo', '$wgpuReadGpuTexelCopyTextureInfo'],
  wgpu_command_encoder_copy_buffer_to_texture: function(commandEncoder, source, destination, copyWidth, copyHeight, copyDepthOrArrayLayers) {
    {{{ wdebuglog('`wgpu_command_encoder_copy_buffer_to_texture(commandEncoder=${commandEncoder}, source=${source}, destination=${destination}, copyWidth=${copyWidth}, copyHeight=${copyHeight}, copyDepthOrArrayLayers=${copyDepthOrArrayLayers})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('source'); }}}
    {{{ wassert('destination'); }}}
    wgpu[commandEncoder]['copyBufferToTexture'](wgpuReadGpuTexelCopyBufferInfo(source), wgpuReadGpuTexelCopyTextureInfo(destination), [copyWidth, copyHeight, copyDepthOrArrayLayers]);
  },

  wgpu_command_encoder_copy_texture_to_buffer__deps: ['$wgpuReadGpuTexelCopyTextureInfo', '$wgpuReadGpuTexelCopyBufferInfo'],
  wgpu_command_encoder_copy_texture_to_buffer: function(commandEncoder, source, destination, copyWidth, copyHeight, copyDepthOrArrayLayers) {
    {{{ wdebuglog('`wgpu_command_encoder_copy_texture_to_buffer(commandEncoder=${commandEncoder}, source=${source}, destination=${destination}, copyWidth=${copyWidth}, copyHeight=${copyHeight}, copyDepthOrArrayLayers=${copyDepthOrArrayLayers})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('source'); }}}
    {{{ wassert('destination'); }}}
    wgpu[commandEncoder]['copyTextureToBuffer'](wgpuReadGpuTexelCopyTextureInfo(source), wgpuReadGpuTexelCopyBufferInfo(destination), [copyWidth, copyHeight, copyDepthOrArrayLayers]);
  },

  wgpu_command_encoder_copy_texture_to_texture__deps: ['$wgpuReadGpuTexelCopyTextureInfo'],
  wgpu_command_encoder_copy_texture_to_texture: function(commandEncoder, source, destination, copyWidth, copyHeight, copyDepthOrArrayLayers) {
    {{{ wdebuglog('`wgpu_command_encoder_copy_texture_to_texture(commandEncoder=${commandEncoder}, source=${source}, destination=${destination}, copyWidth=${copyWidth}, copyHeight=${copyHeight}, copyDepthOrArrayLayers=${copyDepthOrArrayLayers})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('source'); }}}
    {{{ wassert('destination'); }}}
    wgpu[commandEncoder]['copyTextureToTexture'](wgpuReadGpuTexelCopyTextureInfo(source), wgpuReadGpuTexelCopyTextureInfo(destination), [copyWidth, copyHeight, copyDepthOrArrayLayers]);
  },

  wgpu_command_encoder_clear_buffer: function(commandEncoder, buffer, offset, size) {
    {{{ wdebuglog('`wgpu_command_encoder_clear_buffer(commandEncoder=${commandEncoder}, buffer=${buffer}, offset=${offset}, size=${size})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('wgpu[buffer]'); }}}
    {{{ wassert('wgpu[buffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(offset)'); }}}
    {{{ wassert('offset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= -1'); }}} // -1 == MAX_SIZE, or >= 0 for the specified size.
    wgpu[commandEncoder]['clearBuffer'](wgpu[buffer], offset, size < 0 ? void 0 : size);
  },

  wgpu_encoder_push_debug_group: function(encoder, groupLabel) {
    {{{ wdebuglog('`wgpu_command_encoder_push_debug_group(encoder=${encoder}, groupLabel=${groupLabel})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUCommandEncoder || wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('groupLabel != 0'); }}}
    wgpu[encoder]['pushDebugGroup'](utf8(groupLabel));
  },

  wgpu_encoder_pop_debug_group: function(encoder) {
    {{{ wdebuglog('`wgpu_command_encoder_pop_debug_group(encoder=${encoder})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUCommandEncoder || wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}
    wgpu[encoder]['popDebugGroup']();
  },

  wgpu_encoder_insert_debug_marker: function(encoder, markerLabel) {
    {{{ wdebuglog('`wgpu_command_encoder_insert_debug_marker(encoder=${encoder}, markerLabel=${markerLabel})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUCommandEncoder || wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('markerLabel != 0'); }}}
    wgpu[encoder]['insertDebugMarker'](utf8(markerLabel));
  },

  wgpu_command_encoder_resolve_query_set: function(commandEncoder, querySet, firstQuery, queryCount, destination, destinationOffset) {
    {{{ wdebuglog('`wgpu_command_encoder_resolve_query_set(commandEncoder=${commandEncoder}, querySet=${querySet}, firstQuery=${firstQuery}, queryCount=${queryCount}, destination=${destination}, destinationOffset=${destinationOffset})`'); }}}
    {{{ wassert('commandEncoder != 0'); }}}
    {{{ wassert('wgpu[commandEncoder]'); }}}
    {{{ wassert('wgpu[commandEncoder] instanceof GPUCommandEncoder'); }}}
    {{{ wassert('wgpu[querySet] instanceof GPUQuerySet'); }}}
    {{{ wassert('wgpu[destination] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(destinationOffset)'); }}}
    {{{ wassert('destinationOffset >= 0'); }}}
    wgpu[commandEncoder]['resolveQuerySet'](wgpu[querySet], firstQuery, queryCount, wgpu[destination], destinationOffset);
  },

  wgpu_encoder_set_pipeline: function(encoder, pipeline) {
    {{{ wdebuglog('`wgpu_encoder_set_pipeline(encoder=${encoder}, pipeline=${pipeline})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('wgpu[pipeline] instanceof GPURenderPipeline || wgpu[pipeline] instanceof GPUComputePipeline'); }}}
    {{{ wassert('(wgpu[encoder] instanceof GPUComputePassEncoder) == (wgpu[pipeline] instanceof GPUComputePipeline)'); }}}
    wgpu[encoder]['setPipeline'](wgpu[pipeline]);
  },

  wgpu_render_commands_mixin_set_index_buffer__deps: ['$GPUIndexFormats'],
  wgpu_render_commands_mixin_set_index_buffer: function(passEncoder, buffer, indexFormat, offset, size) {
    {{{ wdebuglog('`wgpu_render_commands_mixin_set_index_buffer(passEncoder=${passEncoder}, buffer=${buffer}, indexFormat=${indexFormat}, offset=${offset}, size=${size})`'); }}}
    {{{ wassert('passEncoder != 0'); }}}
    {{{ wassert('wgpu[passEncoder]'); }}}
    {{{ wassert('wgpu[passEncoder] instanceof GPURenderPassEncoder || wgpu[passEncoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('wgpu[buffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(offset)'); }}}
    {{{ wassert('offset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= -1'); }}}

    wgpu[passEncoder]['setIndexBuffer'](wgpu[buffer], GPUIndexFormats[indexFormat], offset, size < 0 ? void 0 : size);
  },

  wgpu_render_commands_mixin_set_vertex_buffer: function(passEncoder, slot, buffer, offset, size) {
    {{{ wdebuglog('`wgpu_render_commands_mixin_set_vertex_buffer(passEncoder=${passEncoder}, slot=${slot}, buffer=${buffer}, offset=${offset}, size=${size})`'); }}}
    {{{ wassert('passEncoder != 0'); }}}
    {{{ wassert('wgpu[passEncoder]'); }}}
    {{{ wassert('wgpu[passEncoder] instanceof GPURenderPassEncoder || wgpu[passEncoder] instanceof GPURenderBundleEncoder'); }}}
    // N.b. buffer may be null here, in which case the existing buffer is intended to be unbound.
    {{{ wassert('buffer == 0 || wgpu[buffer]'); }}}
    {{{ wassert('buffer == 0 || wgpu[buffer] instanceof GPUBuffer'); }}}
    {{{ wassert('buffer != 0 || offset == 0'); }}}
    {{{ wassert('buffer != 0 || size <= 0'); }}}
    {{{ wassert('Number.isSafeInteger(offset)'); }}}
    {{{ wassert('offset >= 0'); }}}
    {{{ wassert('Number.isSafeInteger(size)'); }}}
    {{{ wassert('size >= -1'); }}}

    wgpu[passEncoder]['setVertexBuffer'](slot, wgpu[buffer], offset, size < 0 ? void 0 : size);
  },

  wgpu_render_commands_mixin_draw: function(passEncoder, vertexCount, instanceCount, firstVertex, firstInstance) {
    {{{ wdebuglog('`wgpu_render_commands_mixin_draw(passEncoder=${passEncoder}, vertexCount=${vertexCount}, instanceCount=${instanceCount}, firstVertex=${firstVertex}, firstInstance=${firstInstance})`'); }}}
    {{{ wassert('passEncoder != 0'); }}}
    {{{ wassert('wgpu[passEncoder]'); }}}
    {{{ wassert('wgpu[passEncoder] instanceof GPURenderPassEncoder || wgpu[passEncoder] instanceof GPURenderBundleEncoder'); }}}

    wgpu[passEncoder]['draw'](vertexCount, instanceCount, firstVertex, firstInstance);
  },

  wgpu_render_commands_mixin_draw_indexed: function(passEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance) {
    {{{ wdebuglog('`wgpu_render_commands_mixin_draw_indexed(passEncoder=${passEncoder}, indexCount=${indexCount}, instanceCount=${instanceCount}, firstIndex=${firstIndex}, baseVertex=${baseVertex}, firstInstance=${firstInstance})`'); }}}
    {{{ wassert('passEncoder != 0'); }}}
    {{{ wassert('wgpu[passEncoder]'); }}}
    {{{ wassert('wgpu[passEncoder] instanceof GPURenderPassEncoder || wgpu[passEncoder] instanceof GPURenderBundleEncoder'); }}}

    wgpu[passEncoder]['drawIndexed'](indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
  },

  wgpu_render_commands_mixin_draw_indirect: function(passEncoder, indirectBuffer, indirectOffset) {
    {{{ wdebuglog('`wgpu_render_commands_mixin_draw_indirect(passEncoder=${passEncoder}, indirectBuffer=${indirectBuffer}, indirectOffset=${indirectOffset})`'); }}}
    {{{ wassert('passEncoder != 0'); }}}
    {{{ wassert('wgpu[passEncoder]'); }}}
    {{{ wassert('wgpu[passEncoder] instanceof GPURenderPassEncoder || wgpu[passEncoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('wgpu[indirectBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(indirectOffset)'); }}}
    {{{ wassert('indirectOffset >= 0'); }}}

    wgpu[passEncoder]['drawIndirect'](wgpu[indirectBuffer], indirectOffset);
  },

  wgpu_render_commands_mixin_draw_indexed_indirect: function(passEncoder, indirectBuffer, indirectOffset) {
    {{{ wdebuglog('`wgpu_render_commands_mixin_draw_indexed_indirect(passEncoder=${passEncoder}, indirectBuffer=${indirectBuffer}, indirectOffset=${indirectOffset})`'); }}}
    {{{ wassert('passEncoder != 0'); }}}
    {{{ wassert('wgpu[passEncoder]'); }}}
    {{{ wassert('wgpu[passEncoder] instanceof GPURenderPassEncoder || wgpu[passEncoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('wgpu[indirectBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(indirectOffset)'); }}}
    {{{ wassert('indirectOffset >= 0'); }}}

    wgpu[passEncoder]['drawIndexedIndirect'](wgpu[indirectBuffer], indirectOffset);
  },

  wgpu_encoder_end__deps: ['wgpu_object_destroy'],
  wgpu_encoder_end: function(encoder) {
    {{{ wdebuglog('`wgpu_encoder_end(encoder=${encoder})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder'); }}}

    wgpu[encoder]['end']();

    /* https://gpuweb.github.io/gpuweb/#render-pass-encoder-finalization:
      "The render pass encoder can be ended by calling end() once the user has
      finished recording commands for the pass. Once end() has been called the
      render pass encoder can no longer be used."

      Hence to help make C/C++ side code read easier, immediately release all references
      to the pass encoder after end() occurs, so that C/C++ side code does not need
      to release references manually. */
    _wgpu_object_destroy(encoder);
  },

  // WebGPU specification has two functions GPUCommandBuffer.finish() and GPURenderBundleEncoder.finish(),
  // which currently have identical semantics and structure. Therefore instead of emitting two functions for
  // each, generate only one to save code size.
  wgpu_encoder_finish__deps: ['wgpu_object_destroy', '$wgpuStore'],
  wgpu_encoder_finish: function(encoder) {
    {{{ wdebuglog('`wgpu_encoder_finish(encoder=${encoder})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUCommandEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}

    {{{ wdebuglog('`GPU${wgpu[encoder] instanceof GPUCommandEncoder ? "Command" : "RenderBundle"}Encoder.finish()`'); }}}
    let cmdBuffer = wgpu[encoder]['finish']();

    /* https://gpuweb.github.io/gpuweb/#command-encoder-finalization:
      "A GPUCommandBuffer containing the commands recorded by the GPUCommandEncoder can be
      created by calling finish(). Once finish() has been called the command encoder can no
      longer be used."

      Hence to help make C/C++ side code read easier, immediately release all references to
      the command encoder after finish() occurs, so that C/C++ side code does not need to
      release references manually. */
    _wgpu_object_destroy(encoder);

    return wgpuStore(cmdBuffer);
  },

  wgpu_encoder_set_bind_group: function(encoder, index, /*nullable*/ bindGroup, dynamicOffsets, numDynamicOffsets) {
    {{{ wdebuglog('`wgpu_encoder_set_bind_group(encoder=${encoder}, index=${index}, bindGroup=${bindGroup}, dynamicOffsets=${dynamicOffsets}, numDynamicOffsets=${numDynamicOffsets})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}
    // N.b. bindGroup may be null here, in which case the existing bind group is intended to be unbound.
    {{{ wassert('bindGroup == 0 || wgpu[bindGroup]'); }}}
    {{{ wassert('bindGroup == 0 || wgpu[bindGroup] instanceof GPUBindGroup'); }}}
    {{{ wassert('dynamicOffsets != 0 || numDynamicOffsets == 0'); }}}
#if MIN_FIREFOX_VERSION != TARGET_NOT_SUPPORTED && (MEMORY64 || CAN_ADDRESS_2GB)
    // No Wasm4GB/Wasm64 support in Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=2022805
    // Make a deep copy of the buffer that is small enough for Firefox to handle.
    var firefoxWorkaroundBuffer = new Uint32Array(new Uint32Array(HEAPU32.buffer, {{{ shiftPtr('dynamicOffsets', 0) }}}, numDynamicOffsets));
    wgpu[encoder]['setBindGroup'](index, wgpu[bindGroup], firefoxWorkaroundBuffer);
#else
    wgpu[encoder]['setBindGroup'](index, wgpu[bindGroup], HEAPU32, {{{ shiftPtr('dynamicOffsets', 2) }}}, numDynamicOffsets);
#endif
  },

  wgpu_compute_pass_encoder_dispatch_workgroups: function(encoder, workgroupCountX, workgroupCountY, workgroupCountZ) {
    {{{ wdebuglog('`wgpu_compute_pass_encoder_dispatch_workgroups(encoder=${encoder}, workgroupCountX=${workgroupCountX}, workgroupCountY=${workgroupCountY}, workgroupCountZ=${workgroupCountZ})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUComputePassEncoder'); }}}
    wgpu[encoder]['dispatchWorkgroups'](workgroupCountX, workgroupCountY, workgroupCountZ);
  },

  wgpu_compute_pass_encoder_dispatch_workgroups_indirect: function(encoder, indirectBuffer, indirectOffset) {
    {{{ wdebuglog('`wgpu_compute_pass_encoder_dispatch_workgroups_indirect(encoder=${encoder}, indirectBuffer=${indirectBuffer}, indirectOffset=${indirectOffset})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUComputePassEncoder'); }}}
    {{{ wassert('indirectBuffer != 0'); }}}
    {{{ wassert('wgpu[indirectBuffer]'); }}}
    {{{ wassert('wgpu[indirectBuffer] instanceof GPUBuffer'); }}}
    {{{ wassert('Number.isSafeInteger(indirectOffset)'); }}}
    {{{ wassert('indirectOffset >= 0'); }}}
    wgpu[encoder]['dispatchWorkgroupsIndirect'](wgpu[indirectBuffer], indirectOffset);
  },

  wgpu_render_pass_encoder_set_viewport: function(encoder, x, y, width, height, minDepth, maxDepth) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_set_viewport(encoder=${encoder}, x=${x}, y=${y}, width=${width}, height=${height}, minDepth=${minDepth}, maxDepth=${maxDepth})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['setViewport'](x, y, width, height, minDepth, maxDepth);
  },

  wgpu_render_pass_encoder_set_scissor_rect: function(encoder, x, y, width, height) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_set_scissor_rect(encoder=${encoder}, x=${x}, y=${y}, width=${width}, height=${height})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['setScissorRect'](x, y, width, height);
  },

  wgpu_render_pass_encoder_set_blend_constant: function(encoder, r, g, b, a) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_set_blend_constant(encoder=${encoder}, r=${r}, g=${g}, b=${b}, a=${a})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['setBlendConstant']([r, g, b, a]);
  },

  wgpu_render_pass_encoder_set_stencil_reference: function(encoder, stencilValue) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_set_stencil_reference(stencilValue=${stencilValue})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['setStencilReference'](stencilValue);
  },

  wgpu_render_pass_encoder_begin_occlusion_query: function(encoder, queryIndex) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_begin_occlusion_query(queryIndex=${queryIndex})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['beginOcclusionQuery'](queryIndex);
  },

  wgpu_render_pass_encoder_end_occlusion_query: function(encoder) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_end_occlusion_query()`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['endOcclusionQuery']();
  },

  wgpu_render_pass_encoder_execute_bundles__deps: ['$wgpuReadArrayOfItems'],
  wgpu_render_pass_encoder_execute_bundles: function(encoder, bundles, numBundles) {
    {{{ wdebuglog('`wgpu_render_pass_encoder_execute_bundles(encoder=${encoder}, bundles=${bundles}, numBundles=${numBundles})`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPURenderPassEncoder'); }}}
    wgpu[encoder]['executeBundles'](wgpuReadArrayOfItems(wgpu, bundles, numBundles));
  },

  wgpu_queue_submit_one_and_destroy__deps: ['wgpu_object_destroy'],
  wgpu_queue_submit_one_and_destroy: function(queue, commandBuffer) {
    {{{ wdebuglog('`wgpu_queue_submit_one_and_destroy(queue=${queue}, commandBuffer=${commandBuffer})`'); }}}
    {{{ wassert('queue != 0'); }}}
    {{{ wassert('wgpu[queue]'); }}}
    {{{ wassert('wgpu[queue] instanceof GPUQueue'); }}}
    {{{ wassert('commandBuffer != 0'); }}}
    {{{ wassert('wgpu[commandBuffer]'); }}}
    {{{ wassert('wgpu[commandBuffer] instanceof GPUCommandBuffer'); }}}
    wgpu[queue]['submit']([wgpu[commandBuffer]]);
    _wgpu_object_destroy(commandBuffer);
  },

  wgpu_queue_submit_multiple_and_destroy__deps: ['wgpu_object_destroy', '$wgpuReadArrayOfItems'],
  wgpu_queue_submit_multiple_and_destroy: function(queue, commandBuffers, numCommandBuffers) {
    {{{ wdebuglog('`wgpu_queue_submit_multiple_and_destroy(queue=${queue}, commandBuffers=${commandBuffers}, numCommandBuffers=${numCommandBuffers})`'); }}}
    {{{ wassert('queue != 0'); }}}
    {{{ wassert('wgpu[queue]'); }}}
    {{{ wassert('wgpu[queue] instanceof GPUQueue'); }}}
    wgpu[queue]['submit'](wgpuReadArrayOfItems(wgpu, commandBuffers, numCommandBuffers));

    {{{ replacePtrToIdx('commandBuffers', 2); }}}
    while(numCommandBuffers--) _wgpu_object_destroy(HEAPU32[commandBuffers++]);
  },

  wgpu_queue_set_on_submitted_work_done_callback: function(queue, callback, userData) {
    {{{ wassert('queue != 0'); }}}
    {{{ wassert('wgpu[queue]'); }}}
    {{{ wassert('wgpu[queue] instanceof GPUQueue'); }}}
    {{{ wassert('callback'); }}}
    wgpu[queue]['onSubmittedWorkDone']().then(() => {{{ makeDynCall('vip', 'callback') }}}(queue, userData));
  },

  wgpu_queue_write_buffer: function(queue, buffer, bufferOffset, data, size) {
    {{{ wdebuglog('`wgpu_queue_write_buffer(queue=${queue}, buffer=${buffer}, bufferOffset=${bufferOffset}, data=${Number(data)>>>0}, size=${size})`'); }}}
    {{{ wassert('queue != 0'); }}}
    {{{ wassert('wgpu[queue]'); }}}
    {{{ wassert('wgpu[queue] instanceof GPUQueue'); }}}
    {{{ wassert('buffer != 0'); }}}
    {{{ wassert('wgpu[buffer]'); }}}
    {{{ wassert('wgpu[buffer] instanceof GPUBuffer'); }}}
#if MIN_FIREFOX_VERSION != TARGET_NOT_SUPPORTED && (MEMORY64 || CAN_ADDRESS_2GB)
    // No Wasm4GB/Wasm64 support in Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=2022805
    var firefoxWorkaroundBuffer = new Uint8Array(new Uint8Array(HEAPU8.buffer, {{{ shiftPtr('data', 0) }}}, size));
    wgpu[queue]['writeBuffer'](wgpu[buffer], bufferOffset, firefoxWorkaroundBuffer);
#else
    wgpu[queue]['writeBuffer'](wgpu[buffer], bufferOffset, HEAPU8, {{{ shiftPtr('data', 0) }}}, size);
#endif
  },

  wgpu_queue_write_texture__deps: ['$wgpuReadGpuTexelCopyTextureInfo'],
  wgpu_queue_write_texture: function(queue, destination, data, bytesPerBlockRow, blockRowsPerImage, writeWidth, writeHeight, writeDepthOrArrayLayers) {
    {{{ wdebuglog('`wgpu_queue_write_texture(queue=${queue}, destination=${destination}, data=${Number(data)>>>0}, bytesPerBlockRow=${bytesPerBlockRow}, blockRowsPerImage=${blockRowsPerImage}, writeWidth=${writeWidth}, writeHeight=${writeHeight}, writeDepthOrArrayLayers=${writeDepthOrArrayLayers})`'); }}}
    {{{ wassert('queue != 0'); }}}
    {{{ wassert('wgpu[queue]'); }}}
    {{{ wassert('wgpu[queue] instanceof GPUQueue'); }}}
    {{{ wassert('destination'); }}}
#if MIN_FIREFOX_VERSION != TARGET_NOT_SUPPORTED && (MEMORY64 || CAN_ADDRESS_2GB)
    // No Wasm4GB/Wasm64 support in Firefox: https://bugzilla.mozilla.org/show_bug.cgi?id=2022805
    var firefoxWorkaroundBuffer = new Uint8Array(new Uint8Array(HEAPU8.buffer, {{{ shiftPtr('data', 0) }}}, bytesPerBlockRow*blockRowsPerImage*writeDepthOrArrayLayers));
    wgpu[queue]['writeTexture'](wgpuReadGpuTexelCopyTextureInfo(destination), firefoxWorkaroundBuffer,
      { 'offset': 0,
        'bytesPerRow': bytesPerBlockRow,
        'rowsPerImage': blockRowsPerImage
      }, [writeWidth, writeHeight, writeDepthOrArrayLayers]);
#else
    wgpu[queue]['writeTexture'](wgpuReadGpuTexelCopyTextureInfo(destination), HEAPU8,
      { 'offset': {{{ shiftPtr('data', 0) }}},
        'bytesPerRow': bytesPerBlockRow,
        'rowsPerImage': blockRowsPerImage
      }, [writeWidth, writeHeight, writeDepthOrArrayLayers]);
#endif
  },

  wgpu_queue_copy_external_image_to_texture__deps: ['$wgpuReadGpuTexelCopyTextureInfo', '$HTMLPredefinedColorSpaces'],
  wgpu_queue_copy_external_image_to_texture: function(queue, source, destination, copyWidth, copyHeight, copyDepthOrArrayLayers) {
    {{{ wdebuglog('`wgpu_queue_copy_external_image_to_texture(queue=${queue}, source=${source}, destination=${destination}, copyWidth=${copyWidth}, copyHeight=${copyHeight}, copyDepthOrArrayLayers=${copyDepthOrArrayLayers})`'); }}}
    {{{ wassert('queue != 0'); }}}
    {{{ wassert('wgpu[queue]'); }}}
    {{{ wassert('wgpu[queue] instanceof GPUQueue'); }}}
    {{{ wassert('source'); }}}
    {{{ wassert('destination'); }}}

    {{{ replacePtrToIdx('source', 2); }}}
    let dest = wgpuReadGpuTexelCopyTextureInfo(destination);
    {{{ replacePtrToIdx('destination', 2); }}}
    dest['colorSpace'] = HTMLPredefinedColorSpaces[HEAP32[destination+6]];
    dest['premultipliedAlpha'] = !!HEAP32[destination+7];

#if ASSERTIONS
    let src = {
      'source': wgpu[HEAPU32[source]],
      'origin': [HEAP32[source+1], HEAP32[source+2]],
      'flipY': !!HEAPU32[source+3]
      };
    {{{ wdebugdir('src', '`GPUQueue.copyExternalImageToTexture() with src:`'); }}}
    {{{ wdebugdir('dest', '`dst:`'); }}}
    {{{ wdebuglog('`copy dimensions: ${[copyWidth, copyHeight, copyDepthOrArrayLayers]}`'); }}}
    wgpu[queue]['copyExternalImageToTexture'](src, dest, [copyWidth, copyHeight, copyDepthOrArrayLayers]);
#else
    wgpu[queue]['copyExternalImageToTexture']({
      'source': wgpu[HEAPU32[source]],
      'origin': [HEAP32[source+1], HEAP32[source+2]],
      'flipY': !!HEAPU32[source+3]
      }, dest, [copyWidth, copyHeight, copyDepthOrArrayLayers]);
#endif
  },

#if ASSERTIONS || globalThis.WEBGPU_DEBUG
  wgpu_query_set_type__deps: ['$GPUQueryTypes'],
#endif
  wgpu_query_set_type: function(querySet) {
    {{{ wdebuglog('`wgpu_query_set_type(querySet=${querySet})`'); }}}
    {{{ wassert('querySet != 0'); }}}
    {{{ wassert('wgpu[querySet]'); }}}
    {{{ wassert('wgpu[querySet] instanceof GPUQuerySet'); }}}
    {{{ wassert('GPUQueryTypes.includes(wgpu[querySet]["type"])'); }}}
    return ' ot'.indexOf(wgpu[querySet]['type'][0]); // 'o'cclusion=1, 't'imestamp=2
  },

  wgpu_query_set_count: function(querySet) {
    {{{ wdebuglog('`wgpu_query_set_count(querySet=${querySet})`'); }}}
    {{{ wassert('querySet != 0'); }}}
    {{{ wassert('wgpu[querySet]'); }}}
    {{{ wassert('wgpu[querySet] instanceof GPUQuerySet'); }}}
    return wgpu[querySet]['count'];
  },

  wgpu_load_image_bitmap_from_url_async__deps: ['wgpuMuteJsExceptions', '$utf8', '$wgpuStore'],
  wgpu_load_image_bitmap_from_url_async: function(url, flipY, callback, userData) {
    {{{ wdebuglog('`wgpu_load_image_bitmap_from_url_async(url=\"${utf8(url)}\" (${url}), callback=${callback}, userData=${userData})`'); }}}
    let img = new Image();
    img.src = utf8(url);

    let dispatchCallback = imageBitmapOrError => {
      {{{ wdebugdir('imageBitmapOrError', '`createImageBitmap(img) loaded:`'); }}}
      {{{ wdebugdir('img', '`from:`'); }}}
      {{{ makeDynCall('viiip', 'callback') }}}(imageBitmapOrError.width && wgpuStore(imageBitmapOrError), imageBitmapOrError.width, imageBitmapOrError.height, userData);
    };

    img.decode().then(() => createImageBitmap(img, flipY ? { 'imageOrientation': 'flipY' } : {}))
              .then(_wgpuMuteJsExceptions(dispatchCallback)).catch(dispatchCallback);
  },

#if ASYNCIFY
  wgpu_present_all_rendering_and_wait_for_next_animation_frame__deps: ['$wgpu_async'],
  wgpu_present_all_rendering_and_wait_for_next_animation_frame__sig: 'v',
  wgpu_present_all_rendering_and_wait_for_next_animation_frame__async: true,
  wgpu_present_all_rendering_and_wait_for_next_animation_frame: function() {
    return new Promise(requestAnimationFrame);
  },
#endif

  offscreen_canvas_create__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_create: function(offscreenCanvasId, width, height) {
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('!wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('width > 0'); }}}
    {{{ wassert('height > 0'); }}}

    wgpuOffscreenCanvases[offscreenCanvasId] = new OffscreenCanvas(width, height);
  },

  canvas_transfer_control_to_offscreen__deps: ['$wgpuOffscreenCanvases', '$utf8'],
  canvas_transfer_control_to_offscreen: function(canvasSelector, offscreenCanvasId) {
    {{{ wdebuglog('`canvas_transfer_control_to_offscreen(canvasSelector="${utf8(canvasSelector)}", offscreenCanvasId=${offscreenCanvasId})`'); }}}
    {{{ wassert('canvasSelector'); }}}
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('!wgpuOffscreenCanvases[offscreenCanvasId]'); }}}

    let canvas = document.querySelector(utf8(canvasSelector));
    {{{ wdebugdir('canvas', '`querySelector returned:`') }}};

    wgpuOffscreenCanvases[offscreenCanvasId] = canvas.transferControlToOffscreen();
    canvas['isOffscreen'] = 1;
  },

  // Shared message listener for receiving OffscreenCanvas transfers from the main thread.
  // Using a single __postset here ensures the listener is registered only once even if
  // both offscreen_canvas_post_to_worker and offscreen_canvas_post_to_pthread are linked.
  $wgpuOffscreenCanvasListener__deps: ['$wgpuOffscreenCanvases'],
  $wgpuOffscreenCanvasListener__postset: 'addEventListener("message", (e) => { var d = e["data"]; if (d["wgpuCanvas"]) wgpuOffscreenCanvases[d["id"]] = d["wgpuCanvas"]; });',
  $wgpuOffscreenCanvasListener: 0,

  offscreen_canvas_post_to_worker__deps: ['$wgpuOffscreenCanvases', '$_wasmWorkers', '$wgpuOffscreenCanvasListener'],
  offscreen_canvas_post_to_worker: function(offscreenCanvasId, worker) {
    {{{ wdebuglog('`offscreen_canvas_post_to_worker(offscreenCanvasId=${offscreenCanvasId}, worker=${worker})`'); }}}
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}

    let c = wgpuOffscreenCanvases[offscreenCanvasId];
    _wasmWorkers[worker].postMessage({'wgpuCanvas': c, 'id': offscreenCanvasId}, [c]);
    delete wgpuOffscreenCanvases[offscreenCanvasId];
  },

  offscreen_canvas_post_to_pthread__deps: ['$wgpuOffscreenCanvases', '$PThread', '$wgpuOffscreenCanvasListener'],
  offscreen_canvas_post_to_pthread: function(offscreenCanvasId, pthread) {
    {{{ wdebuglog('`offscreen_canvas_post_to_pthread(offscreenCanvasId=${offscreenCanvasId}, pthread=${pthread})`'); }}}
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}

    let c = wgpuOffscreenCanvases[offscreenCanvasId];
    PThread.pthreads[{{{ wasm4GbShift('pthread') }}}].postMessage({'wgpuCanvas': c, 'id': offscreenCanvasId}, [c]);
    delete wgpuOffscreenCanvases[offscreenCanvasId];
  },

  offscreen_canvas_is_valid__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_is_valid: function(offscreenCanvasId) {
    {{{ wassert('!wgpuOffscreenCanvases[offscreenCanvasId] || wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}
    return !!wgpuOffscreenCanvases[offscreenCanvasId];
  },

  offscreen_canvas_destroy__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_destroy: function(offscreenCanvasId) {
    {{{ wdebuglog('`offscreen_canvas_destroy(offscreenCanvasId=${offscreenCanvasId})${wgpuOffscreenCanvases[offscreenCanvasId] ? ", deleted OffscreenCanvas" : ", no canvas with such ID existed"}`'); }}}
    delete wgpuOffscreenCanvases[offscreenCanvasId];
  },

  offscreen_canvas_width__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_width: function(offscreenCanvasId) {
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}

    return wgpuOffscreenCanvases[offscreenCanvasId]['width'];
  },

  offscreen_canvas_height__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_height: function(offscreenCanvasId) {
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}

    return wgpuOffscreenCanvases[offscreenCanvasId]['height'];
  },

  offscreen_canvas_size__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_size: function(offscreenCanvasId, outWidth, outHeight) {
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}
    {{{ wassert('outWidth'); }}}
    {{{ wassert('outHeight'); }}}

    var c = wgpuOffscreenCanvases[offscreenCanvasId];
    HEAPU32[{{{ shiftPtr('outWidth', 2); }}}] = c['width'];
    HEAPU32[{{{ shiftPtr('outHeight', 2); }}}] = c['height'];
  },

  offscreen_canvas_set_size__deps: ['$wgpuOffscreenCanvases'],
  offscreen_canvas_set_size: function(offscreenCanvasId, width, height) {
    {{{ wassert('offscreenCanvasId'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId]'); }}}
    {{{ wassert('wgpuOffscreenCanvases[offscreenCanvasId] instanceof OffscreenCanvas'); }}}

    var c = wgpuOffscreenCanvases[offscreenCanvasId];
    c['width'] = width;
    c['height'] = height;
  },

  // EXPERIMENTAL: Not part of the ratified spec yet.
  // https://github.com/gpuweb/gpuweb/blob/main/proposals/immediate-data.md
  wgpu_encoder_set_immediate_data: function(encoder, offset, ptr, size) {
    {{{ wdebuglog('`wgpu_encoder_set_immediate_data(encoder=${encoder}, offset=${offset}, ptr=${ptr}, size=${size}`'); }}}
    {{{ wassert('encoder != 0'); }}}
    {{{ wassert('wgpu[encoder]'); }}}
    {{{ wassert('wgpu[encoder] instanceof GPUComputePassEncoder || wgpu[encoder] instanceof GPURenderPassEncoder || wgpu[encoder] instanceof GPURenderBundleEncoder'); }}}
    {{{ wassert('offset >= 0'); }}}
    {{{ wassert('offset <= 64'); }}}
    {{{ wassert('ptr > 0'); }}}
    {{{ wassert('size >= 0'); }}}
    {{{ wassert('size <= 64'); }}}
    wgpu[encoder]['setImmediateData'](offset, HEAPU8, {{{ shiftPtr('ptr', 0) }}}, size);
  }
};

// If building with -jsDWEBGPU_PROFILE=1, then wrap all WebGPU API calls into
// performance.now() timers to get a log of any slow running functions.
#if globalThis.WEBGPU_PROFILE
function argList(len) {
  var args = [];
  for(let i = 0; i < len; ++i) args.push(`a${i}`);
    return args;
}

for(const [name, func] of Object.entries(api)) {
  if (name.startsWith('wgpu_') && func instanceof Function) {
    const benchmarked_name = `benchmarked_${name}`;
    api[benchmarked_name] = func;
    (api[`${name}__deps`] ??= []).push(benchmarked_name);

    const args = argList(func.length);
    api[name] = new Function(...args, `const t0 = performance.now();
const ret = _${benchmarked_name}(${args.join(',')});
const t1 = performance.now();
if (t1-t0 > 1) console.warn(\`[wgpu perf] Call to ${name} took \${t1-t0} msecs.\`);
return ret;`);
  }
}
#endif

mergeInto(LibraryManager.library, api);
