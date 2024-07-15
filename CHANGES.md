Changelog
===

dev
---

### Add

- Add `Layer` object, `lovr.headset.newLayer`, and `lovr.headset.get/setLayers`.
- Add `File` object and `lovr.filesystem.newFile`.
- Add support for BMFont in `Font` and `Rasterizer`.
- Add `--watch` CLI flag, `lovr.filechanged` event, and `lovr.filesystem.watch/unwatch`.
- Add `Model:resetBlendShapes`.
- Add `lovr.system.wasMousePressed` and `lovr.system.wasMouseReleased`.
- Add `lovr.system.get/setClipboardText`.
- Add `KeyCode`s for numpad keys.
- Add support for `uniform` variables in shader code.
- Add support for cubemap array textures.
- Add support for transfer operations on texture views.
- Add support for nesting texture views (creating a view of a view).
- Add `sn10x3` `DataType`.
- Add support for `d24` texture format.
- Add `Quat:get/setEuler`.
- Add `lovr.system.openConsole`.
- Add `lovr.filesystem.getBundlePath` (for internal Lua code).
- Add `lovr.filesystem.setSource` (for internal Lua code).
- Add `t.thread.workers` to configure number of worker threads.
- Add support for declaring objects as to-be-closed variables in Lua 5.4.
- Add variant of `lovr.physics.newWorld` that takes a table of settings.
- Add `World:get/setCallbacks` and `Contact` object.
- Add `World:getColliderCount`.
- Add `World:getJointCount` and `World:getJoints`.
- Add `World:shapecast`.
- Add `World:collideShape`.
- Add `Collider:get/setGravityScale`.
- Add `Collider:is/setContinuous`.
- Add `Collider:get/setDegreesOfFreedom`.
- Add `Collider:applyLinearImpulse` and `Collider:applyAngularImpulse`.
- Add `Collider:moveKinematic`.
- Add `Collider:getRawPosition` and `Collider:getRawOrientation`.
- Add `Collider:getShape`.
- Add `Collider:is/setSensor` (replaces `Shape:is/setSensor`).
- Add `Collider:get/setInertia`.
- Add `Collider:get/setCenterOfMass`.
- Add `Collider:resetMassData`.
- Add `Collider:get/setAutomaticMass`.
- Add `Collider:is/setEnabled`.
- Add `ConvexShape`.
- Add `WeldJoint`.
- Add `ConeJoint`.
- Add `Joint:getForce` and `Joint:getTorque`.
- Add `Joint:get/setPriority`.
- Add `Joint:isDestroyed`.
- Add `DistanceJoint:get/setLimits`.
- Add `:get/setSpring` to `DistanceJoint`, `HingeJoint`, and `SliderJoint`.
- Add `HingeJoint:get/setFriction` and `SliderJoint:get/setFriction`.
- Add `SliderJoint:getAnchors`.
- Add `Shape:get/setDensity`.
- Add `Shape:getMass/Volume/Inertia/CenterOfMass`.
- Add `Shape:get/setOffset`.
- Add motor support to `HingeJoint` and `SliderJoint`.
- Add support for creating a `MeshShape` from a `ModelData`.
- Add `Texture:getLabel` and `Shader:getLabel`.
- Add `Shader:hasVariable`.
- Add `lovr.headset.getHandles`.
- Add `raw` flag to `lovr.graphics.newShader`.
- Add `border` `WrapMode`.

### Change

- Change `lovr.physics` to use a fixed timestep by default, with automatic Collider interpolation.
- Change nogame screen to be bundled as a fused zip archive.
- Change `Mesh:setMaterial` to also take a `Texture`.
- Change shader syntax to no longer require set/binding numbers for buffer/texture variables.
- Change `Texture:getFormat` to also return whether the texture is linear or sRGB.
- Change `Readback:getImage` and `Texture:getPixels` to return sRGB images when the source Texture is sRGB.
- Change `lovr.graphics.newTexture` to use a layer count of 6 when type is `cube` and only width/height is given.
- Change `lovr.graphics.newTexture` to default mipmap count to 1 when an Image is given with a non-blittable format.
- Change `lovr.graphics.newTexture` to error if mipmaps are requested and an Image is given with a non-blittable format.
- Change `TextureFeature` to merge `sample` with `filter`, `render` with `blend`, and `blitsrc`/`blitdst` into `blit`.
- Change headset simulator movement to slow down when holding the control key.
- Change headset simulator to use `t.headset.supersample`.
- Change `lovr.graphics.compileShader` to take/return multiple stages.
- Change maximum number of physics tags from 16 to 31.
- Change `TerrainShape` to require square dimensions.
- Change `Buffer:setData` to use more consistent rules to read data from tables.
- Change `World:queryBox/querySphere` to perform coarse AABB collision detection (use `World:collideShape` for an exact test).
- Change `World:queryBox/querySphere` to take an optional set of tags to include/exclude.
- Change `World:queryBox/querySphere` to return the first collider detected, when the callback is nil.
- Change `World:queryBox/querySphere` to return nil when a callback is given.
- Change `World:raycast` to take a set of tags to allow/ignore.
- Change `World:raycast` callback to be optional (if nil, the closest hit will be returned).
- Change physics queries to report colliders in addition to shapes.

### Fix

- Fix `t.headset.submitDepth` to actually submit depth.
- Fix depth write when depth testing is disabled.
- Fix "morgue overflow" error when creating or destroying large amounts of textures at once.
- Fix `Texture:getType` when used with texture views.
- Fix possible negative `dt` in lovr.update when restarting with the simulator.
- Fix issue where `Collider`/`Shape`/`Joint` userdata wouldn't get garbage collected.
- Fix OBJ triangulation for faces with more than 4 vertices.
- Fix possible crash when using vectors in multiple threads.
- Fix possible crash with `Blob:getName`.
- Fix issue when sampling from depth-stencil textures.

### Deprecate

- Deprecate `Texture:getSampleCount`.
- Deprecate `World:get/setTightness` (use `stabilization` option when creating World).
- Deprecate `World:get/setLinearDamping` (use `Collider:get/setLinearDamping`).
- Deprecate `World:get/setAngularDamping` (use `Collider:get/setAngularDamping`).
- Deprecate `World:is/setSleepingAllowed` (use `sleep` option when creating World).
- Deprecate `World:get/setStepCount` (use `positionSteps`/`velocitySteps` option when creating World).
- Deprecate `Collider:is/setGravityIgnored` (use `Collider:get/setGravityScale`).

### Remove

- Remove `lovr.headset.getOriginType` (use `lovr.headset.isSeated`).
- Remove `lovr.headset.getDisplayFrequency` (renamed to `lovr.headset.getRefreshRate`).
- Remove `lovr.headset.getDisplayFrequencies` (renamed to `lovr.headset.getRefreshRates`).
- Remove `lovr.headset.setDisplayFrequency` (renamed to `lovr.headset.setRefreshRate`).
- Remove `lovr.graphics.getBuffer` (use `lovr.graphics.newBuffer`).
- Remove variant of `lovr.graphics.newBuffer` that takes length first (format is first now).
- Remove `location` key for Buffer fields (use `name`).
- Remove `Buffer:isTemporary`.
- Remove `Buffer:getPointer` (renamed to `Buffer:mapData`).
- Remove `lovr.graphics.getPass` (use `lovr.graphics.newPass`).
- Remove `Pass:getType` (passes support both render and compute now).
- Remove `Pass:getTarget` (renamed to `Pass:getCanvas`).
- Remove `Pass:getSampleCount` (`Pass:getCanvas` returns sample count).
- Remove `samples` option from `lovr.graphics.newTexture` (sample count is always 1).
- Remove variant of `Pass:send` that takes a binding number instead of a variable name.
- Remove `Texture:newView` (renamed to `lovr.graphics.newTextureView`).
- Remove `Texture:isView` and `Texture:getParent`.
- Remove `Shape:setPosition`, `Shape:setOrientation`, and `Shape:setPose` (use `Shape:setOffset`).
- Remove `Shape:is/setSensor` (use `Collider:is/setSensor`).
- Remove `World:overlaps/computeOverlaps/collide/getContacts` (use `World:setCallbacks`).
- Remove `BallJoint:setAnchor`.
- Remove `DistanceJoint:setAnchors`.
- Remove `HingeJoint:setAnchor`.
- Remove `BallJoint:get/setResponseTime` and `BallJoint:get/setTightness`.
- Remove `DistanceJoint:get/setResponseTime` and `DistanceJoint:get/setTightness` (use `:setSpring`).
- Remove `DistanceJoint:get/setDistance` (renamed to `:get/setLimits`).
- Remove `HingeJoint:get/setUpper/LowerLimit` (use `:get/setLimits`).
- Remove `SliderJoint:get/setUpper/LowerLimit` (use `:get/setLimits`).
- Remove `Collider:getLocalCenter` (renamed to `Collider:getCenterOfMass`).
- Remove `Shape:getMassData` (split into separate getters).
- REmove `shaderConstantSize` limit from `lovr.graphics.getLimits`.

v0.17.1 - 2024-03-12
---

### Change

- Change file permissions for files and directories created by `lovr.filesystem` (so ADB can access them on Android 12+).
- Change `Pass:sphere` to error when segment count is too small.
- Change `Buffer:setData` to error when copying within a single Buffer and the source and destination ranges overlap.

### Fix

- Fix LÖVR failing to start on Oculus Quest v62+.
- Fix `Pass:skybox` to render cubemaps with the correct X/Z orientation.
- Fix depth write when depth testing is disabled.
- Fix `lovr.graphics.newModel` to support empty GLB models.
- Fix `lovr.graphics.newModel` to support models without any vertices.
- Fix bitangent sign in `TangentMatrix` builtin.
- Fix shader #includes to error when including a file that doesn't exist.
- Fix `lovr.quit` to not get called twice when canceling quit.
- Fix GPU error when texture had only `transfer` usage.
- Fix rare crash when calling vector methods.
- Fix `Model:getMesh`.

v0.17.0 - 2023-10-14
---

### Add

#### General

- Add `enet` plugin.
- Add `http` plugin.
- Add `utf8` module.
- Add `:type` method to all objects and vectors.
- Add `--graphics-debug` command line option, which sets `t.graphics.debug` to true.

#### Audio

- Add `lovr.audio.getDevice`.

#### Data

- Add `Image:mapPixel`.
- Add a variant of `Blob:getString` that takes a byte range.
- Add `Blob:getI8/getU8/getI16/getU16/getI32/getU32/getF32/getF64`.

#### Filesystem

- Add argument to `lovr.filesystem.load` to restrict chunks to text/binary.

#### Graphics

- Add `Pass:roundrect`.
- Add variant of `Pass:cone` that takes two `vec3` endpoints.
- Add variant of `Pass:draw` that takes a `Texture`.
- Add `Pass:setViewCull` to enable frustum culling.
- Add `Model:getBlendShapeWeight`, and `Model:setBlendShapeWeight`.
- Add `Model(Data):getBlendShapeCount`, `Model(Data):getBlendShapeName`.
- Add support for animated blend shape weights in Model animations.
- Add support for arrays, nested structs, and field names to Buffer formats.
- Add `Shader:getBufferFormat`.
- Add back `Mesh` object (sorry!).
- Add `Model:clone`.
- Add `materials` flag to `lovr.graphics.newModel`.
- Add `lovr.graphics.isInitialized` (mostly internal).
- Add support for using depth textures in multisampled render passes.
- Add `depthResolve` feature to `lovr.graphics.getFeatures`.
- Add a variant of `Texture:newView` that creates a 2D slice of a layer/mipmap of a texture.
- Add a variant of `Pass:send` that takes tables for uniform buffers.
- Add `Buffer:getData` and `Buffer:newReadback`.
- Add `Texture:getPixels/setPixels/clear/newReadback/generateMipmaps`.
- Add support for stepping through shaders in e.g. RenderDoc when `t.graphics.debug` is set.
- Add `lovr.graphics.is/setTimingEnabled` to record GPU durations for each Pass object.
- Add `lovr.graphics.newPass`.
- Add `Pass:setCanvas` and `Pass:setClear`.
- Add `Pass:getStats`.
- Add `Pass:reset`.
- Add `Pass:getScissor` and `Pass:getViewport`.
- Add `Pass:barrier`.
- Add `Pass:beginTally/finishTally` and `Pass:get/setTallyBuffer`.
- Add support for `#include` in shader code.

#### Headset

- Add `lovr.headset.getPassthrough/setPassthrough/getPassthroughModes`.
- Add support for more headsets in Android APKs  (Quest, Pico, Vive Focus all work).
- Add support for controller input on Pico Neo 3 and Pico Neo 4 devices.
- Add support for controller input on Magic Leap 2.
- Add `hand/*/pinch`, `hand/*/poke`, and `hand/*/grip` Devices.
- Add support for using `lovr.headset` when `lovr.graphics` is disabled, on supported runtimes.
- Add support for `elbow/left` and `elbow/right` poses on Ultraleap hand tracking.
- Add `lovr.headset.getDirection`.
- Add `lovr.headset.isVisible` and `lovr.visible` callback.
- Add `lovr.recenter` callback.
- Add `floor` Device.
- Add `t.headset.seated` and `lovr.headset.isSeated`.
- Add `lovr.headset.stopVibration`.
- Add `radius` fields to joint tables returned by `lovr.headset.getSkeleton`.
- Add support for "sprinting" in the headset simulator using the shift key.

#### Math

- Add capitalized globals for creating permanent vectors (`Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat4`).
- Add `Vec3:transform`, `Vec4:transform`, and `Vec3:rotate`.
- Add vector constants (`vec3.up`, `vec2.one`, `quat.identity`, etc.).
- Add `Mat4:getTranslation/getRotation/getScale/getPose`.
- Add variant of `Vec3:set` that takes a `Quat`.
- Add `Mat4:reflect`.

#### Physics

- Add `TerrainShape`.
- Add `World:queryBox` and `World:querySphere`.
- Add `World:getTags`.
- Add `Shape:get/setPose`.
- Add missing `lovr.physics.newMeshShape` function.
- Add `Collider:isDestroyed`.

#### System

- Add `lovr.system.wasKeyPressed/wasKeyReleased`.
- Add `lovr.system.getMouseX`, `lovr.system.getMouseY`, and `lovr.system.getMousePosition`.
- Add `lovr.system.isMouseDown`.
- Add `lovr.mousepressed`, `lovr.mousereleased`, `lovr.mousemoved`, and `lovr.wheelmoved` callbacks.
- Add `lovr.system.has/setKeyRepeat`.

#### Thread

- Add support for vectors and lightuserdata with `Channel:push` and `Channel:pop`.

### Change

- Change `lovr.graphics.submit` to no longer invalidate Pass objects.
- Change vector constructors to be callable metatables (allows adding custom methods).
- Change max size of vector pool to hold 16 million numbers instead of 64 thousand.
- Change `Pass:setBlendMode`/`Pass:setColorWrite` to take an optional attachment index.
- Change OpenXR driver to throttle update loop when headset session is idle.
- Change `lovr.graphics.newModel` to work when the asset references paths starting with `./`.
- Change `lovr.graphics.newModel` to error when the asset references paths starting with `/`.
- Change `lovr.graphics.newBuffer` to take format first instead of length/data.
- Change `Pass:setStencilWrite` to only set the "stencil pass" action when given a single action, instead of all 3.
- Change `lovr.graphics` to show a message box on Windows when Vulkan isn't supported.
- Change plugin loader to call `JNI_OnLoad` on Android so plugins can use JNI.
- Change `t.headset.overlay` to also support numeric values to control overlay sort order.
- Change `World:isCollisionEnabledBetween` to take `nil`s, which act as wildcard tags.
- Change `Mat4:__tostring` to print matrix components.
- Change `World:raycast` to prevent subsequent checks when the callback returns `false`.
- Change `lovr.system.isKeyDown` to take multiple keys.
- Change `lovr.headset.isDown/isTouched` to return nil instead of nothing.
- Change `lovr.headset.getTime` to always start at 0 to avoid precision issues.
- Change Pass viewport and scissor to apply to all draws in the Pass, instead of per-draw.
- Change nogame/error screen to use a transparent background on AR/passthrough headsets.
- Change VR simulator to use projected mouse position for hand pose (scroll controls distance).
- Change `lovr.headset.getDriver` to also return the OpenXR runtime name.
- Change `pitchable` flag in `lovr.audio.newSource` to default to true.
- Change `Buffer:setData`, `Buffer:clear`, and `Buffer:getPointer` to work on permanent Buffers.
- Change `lovr.timer.sleep` to have higher precision on Windows.
- Change `lovr.headset.animate` to no longer require a `Device` argument (current variant is deprecated).

### Fix

- Fix `lovr.physics.newBoxShape` always creating a cube.
- Fix several issues related to VRAM leaks when creating Textures.
- Fix issue on Linux where the Vulkan library wouldn't get loaded properly.
- Fix macOS to not require the Vulkan SDK to be installed.
- Fix recentering on Quest.
- Fix headset mirror window to properly render in mono when a VR headset is connected.
- Fix window size not updating on resize or on highdpi displays.
- Fix `MeshShape` not working properly with some OBJ models.
- Fix `Model:getNodeScale` to properly return the scale instead of the rotation.
- Fix `Mat4:set` and `Mat4` constructors to properly use TRS order when scale is given.
- Fix `lovr.graphics.newShader` to work with `nil` shader code (uses `unlit` shader).
- Fix crash when `t.graphics.debug` is set but the validation layers aren't installed.
- Fix a few memory leaks with Readbacks, Fonts, and compute Shaders.
- Fix `Pass:setProjection` to use an infinite far plane by default.
- Fix `Texture:hasUsage`.
- Fix `Pass:points` / `Pass:line` to work with temporary vectors.
- Fix `lovr.event.quit` to properly exit on Android/Quest.
- Fix mounted zip paths sometimes not working with `lovr.filesystem.getDirectoryItems`.
- Fix issue where temporary vectors didn't work in functions that accept colors.
- Fix crash when rendering multiple identical torus shapes in the same Pass.
- Fix `lovr.graphics.newShader` to error properly if the push constants block is too big.
- Fix default `lovr.log` to not print second `string.gsub` result.
- Fix issue where `lovr.data.newImage` wouldn't initialize empty `Image` object pixels to zero.
- Fix `Model:getMaterial` to work when given a string.
- Fix issue where `Vec3:angle` would sometimes return NaNs.
- Fix OpenXR driver when used with AR headsets.
- Fix vertex tangents (previously they only worked if the `normalMap` shader flag was set).
- Fix error when minimizing the desktop window on Windows.
- Fix `DistanceJoint:getAnchors`.
- Fix issue where `Material:getProperties` returned an incorrect uvScale.
- Fix crash when uvShift/uvScale were given as tables in `lovr.graphics.newMaterial`.
- Fix retina macOS windows.
- Fix issue where `Pass:capsule` didn't render anything when its length was zero.
- Fix confusing console message when OpenXR is not installed.
- Fix issue where OBJ UVs were upside down.
- Fix issue where equirectangular skyboxes used an incorrect z direction.
- Fix seam when rendering equirectangular skyboxes with mipmaps.
- Fix font wrap when camera uses a projection matrix with a flipped up direction.
- Fix Model import when vertex colors are stored as vec3.
- Fix 24-bit WAV import.
- Fix issue with 3-letter vec3 swizzles.
- Fix error when subtracting a vector from a number.
- Fix error when adding a number and a temporary vector (in that order).
- Fix issue where `t.window = nil` wasn't working as intended.
- Fix bug where some fonts would render glyphs inside out.

### Deprecate

- Deprecate `lovr.graphics.getPass` (`lovr.graphics.newPass` should be used instead).
- Deprecate `Pass:getType`.  All Pass objects support both compute and rendering (computes run before draws).
- Deprecate `Pass:getTarget` (renamed to `Pass:getCanvas`).
- Deprecate `Pass:getSampleCount` (`Pass:getCanvas` returns a `samples` key).
- Deprecate `lovr.graphics.getBuffer` (Use `lovr.graphics.newBuffer` or tables).
- Deprecate variant of `lovr.graphics.newBuffer` that takes length/data as the first argument (put format first).
- Deprecate `lovr.headset.get/setDisplayFrequency` (it's named `lovr.headset.get/setRefreshRate` now).
- Deprecate `lovr.headset.getDisplayFrequencies` (it's named `lovr.headset.getRefreshRates` now).
- Deprecate `lovr.headset.getOriginType` (use `lovr.headset.isSeated`).
- Deprecate variant of `lovr.headset.animate` that takes a `Device` argument (just pass the `Model` now).
- Deprecate `Buffer:getPointer` (renamed to `Buffer:mapData`).

### Remove

- Remove `transfer` passes (methods on `Buffer` and `Texture` objects can be used instead).
- Remove `Pass:copy` (use `Buffer:setData` and `Texture:setPixels`).
- Remove `Pass:read` (use `Buffer:newReadback` and `Texture:newReadback`).
- Remove `Pass:clear` (use `Buffer:clear` and `Texture:clear`).
- Remove `Pass:blit` (use `Texture:setPixels`).
- Remove `Pass:mipmap` (use `Texture:generateMipmaps`).
- Remove `Tally` (use `lovr.graphics.setTimingEnabled` or `Pass:beginTally/finishTally`).
- Remove `lovr.event.pump` (it's named `lovr.system.pollEvents` now).
- Remove `t.headset.offset` (use `t.headset.seated`).
- Remove `mipmaps` flag from render passes (they always regenerate mipmaps now).

v0.16.0 - 2022-10-15
---

### Add

- Add ability to run individual Lua files in addition to folders.
- Add Vulkan renderer.
- Add capsule, cone, and torus shapes.
- Add `segments` option to planes.
- Add option to draw a single node of a model.
- Add indirect draws and indirect compute.
- Add depth clamp and depth offset render states.
- Add support for configuring "stencil fail" and "depth fail" stencil actions.
- Add viewport and scissor render states.
- Add `Pass` objects for recording rendering work.
- Add `Buffer` objects.
- Add `Buffer:getPointer` for accessing FFI pointers to GPU memory.
- Add support for half floats for vertex attributes and buffer data.
- Add texture views.
- Add single-pass rendering to multi-layer textures (array, cubemap, 3D).
- Add `r8`, `rg8`, `r16`, `rg16`, `rgba16`, and `rgb565` image and texture formats.
- Add `d32fs8` texture format.
- Add support for BC4, BC5, BC6, and BC7 texture formats.
- Add support for loading uncompressed formats from DDS and KTX files.
- Add support for loading KTX2 textures.
- Add support for loading non-2D textures from image files.
- Add support for loading DDS and KTX textures in glTF models.
- Add support for the `KHR_texture_transform` glTF extension.
- Add support for loading base64 images from glTF models.
- Add `Sampler` objects.
- Add `Tally` objects for GPU queries.
- Add `Readback` objects for asynchronous GPU readbacks.
- Add `t.graphics.shadercache` option to cache compiled shaders to disk.
- Add `lovr.graphics.compileShader` and the ability to load shaders from SPIR-V.
- Add `normal` default shader.
- Add `Font:getKerning`.
- Add `Font:getLines`.
- Add `Font:getVertices`.
- Add support for rendering multicolor text.
- Add `Pass:blit`, `Pass:clear`, `Pass:copy`, `Pass:mipmap`, and `Pass:read` for raw GPU transfers.
- Add `lovr.graphics.isFormatSupported` and `lovr.graphics.getDevice`.
- Add `eye/gaze` Device.
- Add `t.headset.submitdepth` flag to submit depth buffers to the OpenXR runtime.
- Add `t.headset.overlay` conf.lua flag to run as an OpenXR overlay.
- Add support for `keyboard` device pose on Oculus Quest.
- Add support for reading hand tracking pinch strength (it's mapped to trigger).
- Add `thumbrest` DeviceButton.
- Add OpenXR support for `a`, `b`, `x`, and `y` DeviceButtons.
- Add OpenXR support for `hand/left/point` and `hand/right/point` Devices.
- Add OpenXR support for Windows Mixed Reality controllers.
- Add OpenXR support for Oculus Quest hand tracking models via `lovr.headset.newModel`.
- Add OpenXR support for Vive tracker devices.
- Add OpenXR support for `lovr.headset.getDisplayFrequency`.
- Add `lovr.headset.setDisplayFrequency` and `lovr.headset.getDisplayFrequencies`.
- Add `lovr.headset.isFocused`.
- Add `lovr.headset.getDeltaTime`.
- Add support for passing `vec2`s as scale arguments.
- Add support for passing numbers when calling functions on vectors.
- Add functions to `ModelData` (it didn't have any before, now it has lots).
- Add bounding sphere to `Model` and `ModelData`.
- Add ability to procedurally animate model node scales.
- Add `Model:getMetadata` to return raw JSON from glTF models.
- Add `Rasterizer:getWidth`, `Rasterizer:getDimensions`, and `Rasterizer:getBoundingBox`.
- Add `Rasterizer:getKerning` and `Rasterizer:getBearing`.
- Add `Rasterizer:getCurves`.
- Add `Rasterizer:newImage`.
- Add `Vec2:angle`, `Vec3:angle`, and `Vec4:angle`.
- Add `Vec2:equals`, `Vec3:equals`, `Vec4:equals`, `Quat:equals`, and `Mat4:equals`.
- Add `Sound:getCapacity` and `Sound:getByteStride`.
- Add `Source:get/setPitch`.
- Add `Source:isSpatial` and `spatial` flag to `lovr.audio.newSource`.
- Add macOS support for requesting the `audiocapture` Permission with `lovr.system.requestPermission`.
- Add `World:getContacts`.
- Add `World:get/setStepCount`.
- Add `lovr.system.isKeyDown`.

### Change

- Change default headset driver to OpenXR.
- Change `lovr.headset.getSkeleton` to also return joint radii.
- Change shader GLSL version to 4.60.
- Change desktop window to be mono.
- Change maximum view count from 2 to 6.
- Change default projection to have an infinite far plane with a reversed Z range.
- Change skeletal animation to use compute shaders.
- Change OBJ loader to support non-triangular faces.
- Change `setColor` to accept a second alpha parameter when using a hexcode.
- Change `Source:setEffectEnabled` to error when called on a Source that has effects disabled.
- Change `lovr.headset.isDown` to return `nil` instead of `false` when a button is not supported by a Device.
- Change text rendering to print missing glyph characters instead of erroring on missing glyphs.
- Change `dt` parameter to use predicted headset display time delta instead of wallclock delta time, when available.
- Change `lovr.math.noise` to use simplex noise instead of Perlin noise.
- Change `lovr.filesystem.write` and `lovr.filesystem.append` to return success instead of byte count.
- Change window-related functions to be on `lovr.system` instead of `lovr.graphics`.
- Change `Font:get/setLineHeight` to `Font:get/setLineSpacing`.
- Change `lovr.graphics.stencil` to be split into separate "stencil test" and "stencil write" states.

### Fix

- Fix flickering on AMD GPUs
- Fix crash when calling `lovr.audio.setGeometry` or `lovr.physics.newMeshCollider` with zero vertices/indices.
- Fix `require` not finding plugins packaged as "all in one" Lua libraries.
- Fix audio capture issues on Android.
- Fix Oculus Quest hand model orientation.
- Fix issue that would cause vectors to rarely get corrupted with nans in LuaJIT.
- Fix potential crash when using `Object:release` rapidly.
- Fix bug where Collider friction had no effect, and set default friction to infinity instead of 0 (no behavior change).
- Fix issue preventing the audio, graphics, and headset modules from being `require`d in threads.
- Fix issue with `lovr.math.gammaToLinear` and `lovr.math.linearToGamma` when used with a table.
- Fix error when calling `lovr.filesystem.getDirectoryItems` when `table.sort` is unavailable.
- Fix `Mat4 * Vec3`.
- Fix issue with `World:raycast` and `MeshShape`s.
- Fix bug where error screen wouldn't show up if there was an error in `lovr.draw` when using OpenXR.
- Fix issue where `conf.lua` wouldn't get read properly on Android.
- Fix error when passing LÖVR objects to `Thread:start`.
- Fix bug where first command line argument would not be available in fused mode.
- Fix bug where Font vertex winding would be flipped when toggling `Font:setFlipEnabled`.

### Remove

- Remove OpenGL and WebGL renderers.
- Remove `rgb` and `rgba4` texture formats.
- Remove `Mesh` and `ShaderBlock` (they are replaced by `Buffer`).
- Remove `Canvas` (textures can be rendered to directly with render passes).
- Remove arc primitive.
- Remove line width.
- Remove `Font:setFlipEnabled` (font flips automatically when needed).
- Remove WebAssembly build (it will be back soon!)
- Remove `openvr`, `oculus`, `vrapi`, `webxr`, and `pico` headset drivers.
- Remove `beacon` Devices.
- Remove `lovr.headset.getDisplayMask`.
- Remove `effects = false` flag from `lovr.audio.newSource` (use `spatial` flag).

v0.15.0 - 2021-04-11
---

### Add

- Add support for Vive trackers (again)
- Add support for Quest 2
- Add `Device`s for elbows, shoulders, chest, waist, knees, feet, camera, and keyboard
- Add support for OpenXR on Linux
- Add Windows Mixed Reality controller bindings for OpenVR
- Add `lovr.system` module
- Add `lovr.permission` callback
- Add plugin system
- Add support for `require`ing libraries inside APKs
- Add UVs to `lovr.graphics.cylinder`
- Add a stack trace to thread errors
- Add `Mat4:target`
- Add support for binary STL models
- Add `t.audio.spatializer` to support switching audio spatializers
- Add support for Oculus Audio Spatializer
- Add support for Steam Audio
- Add support for WAV sounds
- Add support for MP3 sounds
- Add support for ambisonic sounds
- Add `SampleFormat` for creating floating point Sounds
- Add `t.audio.start` to control whether audio devices are automatically started
- Add `lovr.audio.getDevices`, `lovr.audio.setDevice`, `lovr.audio.start`, `lovr.audio.stop`, and `lovr.audio.isStarted`
- Add `lovr.audio.setGeometry` and `AudioMaterial`
- Add `lovr.audio.get/setAbsorption`
- Add `Effect`, `Source:isEffectEnabled`, and `Source:setEffectEnabled`
- Add `Source:get/setRadius`
- Add `Source:get/setDirectivity`
- Add `Source:clone`
- Add `VolumeUnit` parameter to `lovr.audio.get/setVolume` and `Source:get/setVolume`
- Add support for `lovr.headset.getVelocity` to the WebXR driver

### Change

- Change `SoundData` to be named `Sound`
- Change `TextureData` to be named `Image`
- Change `Image:encode` to return a `Blob` instead of writing to a file
- Change default physics linear damping from `.01` to `0`
- Change `lovr.mirror` to correctly render transparent headset textures
- Change `Mat4:lookAt` to compute a "look at" view matrix (use `Mat4:target` for the old behavior)
- Change `lovr.filesystem.write` and `lovr.filesystem.append` to accept Blobs
- Change `lovr.headset` to submit empty frames when `lovr.draw` is missing (instead of not submitting anything)
- Change default font shader to have more precise/sharp antialiasing
- Change `ShaderBlock:send` to accept a byte range when sending a Blob
- Change `lovr.graphics.newFont` to accept custom glyph padding and SDF range values
- Change `lovr.audio.isSpatialized` to `lovr.audio.getSpatializer`
- Change `Source:setCone` to `Source:setDirectivity`
- Change `samples` value of `TimeUnit` to be named `frames`.

### Fix

- Fix hand tracking orientation on Quest
- Fix improper vsync when changing headset drivers during a restart
- Fix graphics flickering on Intel GPUs
- Fix lovr.math when used on threads
- Fix crash when threads threw non-string errors
- Fix the `lovr.math.newCurve` variant that takes a single number for the point count
- Fix issue where `Mat4:unpack` would sometimes return `nan` angles
- Fix error screen erroring after a while when it ran out of vectors
- Fix error screen erroring when `t.math.globals` was `false`
- Fix crash with `vrapi` driver when requesting pose for devices other than head/hands
- Fix `lovr.filesystem.append` on unix
- Fix `lovr.headset.getDisplayDimensions` to return dimensions for a single eye on the desktop driver
- Fix `lovr.restart` and `arg.restart` on WebAssembly
- Fix `lovr.graphics.getBlendMode` when blending is disabled
- Fix crash when moving a `Shape` when it isn't attached to a Collider
- Fix issues with macOS Big Sur
- Fix text wrapping issue with lines ending in spaces
- Fix some crashes in lovr.physics to log warnings instead

### Remove

- Remove support for Oculus Go
- Remove `lua_modules` and `deps` from the default require paths
- Remove `--root` command line argument
- Remove `lovr.graphics.triangle`
- Remove C require path (use plugins)
- Remove `enet` (use the [`lovr-enet`](https://github.com/bjornbytes/lua-enet) plugin)
- Remove `cjson` (use the [`lovr-cjson`](https://github.com/bjornbytes/lua-cjson) plugin)
- Remove `AudioStream` (use `stream` Sounds instead)
- Remove `Source:getType` and `SourceType` (use `decode` option of newSource/Sound)
- Remove `Source:getBitDepth`, `Source:getChannelCount`, and `Source:getSampleRate`
- Remove `lovr.audio.getMicrophoneNames`, `lovr.audio.newMicrophone`, and `Microphone`
- Remove `lovr.audio.update`
- Remove `lovr.audio.pause`
- Remove `lovr.audio.get/setVelocity`, `Source:get/setVelocity`, and `lovr.audio.get/setDopplerEffect`
- Remove `Source:get/setFalloff`
- Remove `Source:get/setPitch`
- Remove `Source:get/setVolumeLimits`
- Remove `Source:is/setRelative`

v0.14.0 - 2020-10-05
---

### Add

- Add `lovr.headset.getSkeleton`.
- Add `lovr.headset.animate` and `animated` flag to `lovr.headset.newModel`.
- Add `lovr.headset.wasPressed` and `lovr.headset.wasReleased`.
- Add `lovr.headset.getViewCount`, `lovr.headset.getViewPose`, `lovr.headset.getViewAngles`.
- Add `beacon/1`, `beacon/2`, `beacon/3`, and `beacon/4` Devices.
- Add `lovr.headset.getDisplayFrequency`.
- Add `lovr.headset.getTime`.
- Add a WebXR headset driver.
- Add a Pico headset driver.
- Add `lovr.keypressed`, `lovr.keyreleased`, and `lovr.textinput` callbacks.
- Add `lovr.event.restart`, `lovr.restart` callback, and `arg.restart` for persisting data between restarts.
- Add `lovr.resize` callback.
- Add `lovr.log` callback.
- Add `World:getColliders`.
- Add `World:newMeshCollider`.
- Add `Joint:setEnabled` and `Joint:isEnabled`.
- Add `Shape:setSensor` and `Shape:isSensor`.
- Add `World:get/setResponseTime`, `BallJoint:get/setResponseTime`, and `DistanceJoint:get/setResponseTime`.
- Add `World:get/setTightness`, `BallJoint:get/setTightness`, `DistanceJoint:get/setTightness`.
- Add `lovr.graphics.getViewPose` and `lovr.graphics.setViewPose`.
- Add `lovr.graphics.getProjection`.
- Add `t.graphics.debug` config flag.
- Add `compute` limit to `lovr.graphics.getLimits`.
- Add `renderpasses`, `buffers`, `textures`, `buffermemory`, and `texturememory` graphics stats.
- Add `lovr.graphics.setColorMask` and `lovr.graphics.getColorMask`.
- Add `AudioStream:append` for procedural audio generation.
- Add `SoundData:getBlob` and `TextureData:getBlob`.
- Add support for shadow sampler uniforms.
- Add `Texture:getCompareMode` and `Texture:setCompareMode`.
- Add `sampler2DMultiview` and `textureMultiview` helpers to shaders.
- Add `highp` ShaderFlag.
- Add `Shader:hasBlock`.
- Add `Model:hasJoints`.
- Add `:release` to all objects to immediately destroy them from Lua.
- Add support for building with Lua 5.2, 5.3, and 5.4.

### Change

- Change physics module functions to accept vector objects.
- Change animated shaders to render properly with non-animated models.
- Change the desktop headset driver to rotate the controller based on mouse movement.
- Change the desktop headset driver to emit `lovr.focus` events when window focus changes.
- Change the default headset clipping planes to `0.1` and `100.0` on all backends.
- Change `Thread:start` to accept arguments to pass to the Thread's function.
- Change `Channel:peek` to return a second value indicating if a message was present.
- Change `Microphone:getData` to take new count/destination/offset arguments.
- Change `Mesh:setVertices` to accept a `Blob` with vertex data.
- Change `lovr.graphics.setProjection` to take a view index.
- Change WebAssembly build to enable the thread module by default.
- Change the 1ms sleep in the run loop to a 0ms sleep.
- Change `Curve:render` to always return 2 points if the Curve is a line.
- Change `Curve:render` to be faster.
- Change `lovr.data.newTextureData` to allow filling in initial contents with a Blob.
- Change `lovr.data.newTextureData` to accept a TextureData to clone.
- Change `lovr.filesystem.getDirectoryItems` to omit `.` and `..`.
- Change `lovr.filesystem.getDirectoryItems` to return sorted filenames.
- Change `Shader:send` to return a boolean instead of erroring on failure.
- Change `ShaderBlock:getShaderCode` to accept an optional namespace for accessing the block.
- Change OBJ model loading to be faster.
- Change `Vec3:set` to accept a `Mat4`.

### Fix

- Fix many issues with the OpenXR headset driver.
- Fix an issue when restarting some projects that had multiple textures per draw.
- Fix an issue where OBJ models had no AABB.
- Fix an issue where `Channel:push` errored when a userdata was its only argument.
- Fix a crash when passing `nil` to `lovr.thread.newThread`.
- Fix `lovr.data.newBlob(blob)` to properly copy data to the new Blob.
- Fix an issue where textures could load upside down sometimes when using threads.
- Fix an issue with screen tearing on the bottom of the window on macOS.
- Fix an issue where `lovr.headset.getHands` returned an empty table on Linux.
- Fix an issue with forced vsync breaking VR timing in new windows updates.
- Fix an issue where printing only whitespace characters would cause visual glitches.
- Fix several issues with stereo Shaders and Canvases on Android.
- Fix `lovr.graphics.getFeatures().compute` returning `false` on Android.
- Fix a confusing message when passing nothing to `lovr.graphics.points` / `lovr.graphics.line`.
- Fix issue with using Mesh attributes with integer types in shaders.
- Fix crash when using a `Blob` in `ShaderBlock:send`.
- Fix `Thread:wait` on Windows.

### Remove

- Remove `webvr` headset driver (use `webxr` instead).
- Remove `leap` headset driver (use the Ultraleap OpenXR API Layer instead).
- Remove `oculusmobile` headset driver (use `vrapi` instead).
- Remove `SoundData:getPointer` and `TextureData:getPointer` (use `:getBlob`).
- Remove `Source:resume` and `lovr.audio.resume` (use `Source:play`).
- Remove `Source:rewind` and `lovr.audio.rewind` (use `Source:seek(0)`).
- Remove `Source:isPaused` and `Source:isStopped` (use `:isPlaying`).
- Remove `lovr.event.quit('restart')` variant.
- Remove `t.hotkeys` config (use `lovr.keypressed` and `lovr.keyreleased`).
- Remove `lovr.filesystem.getApplicationId` (use `lovr.filesystem.getIdentity`).
- Remove `anisotropic` `FilterMode` (anisotropy can be used with any filter mode).

v0.13.0 - 2019-10-14
---

### Add

- Add support for the Oculus Quest.
- Add support for Valve Index controllers.
- Add support for SteamVR actions.
- Add support for Leap Motion.
- Add an OpenXR headset driver.
- Add a new `standard` PBR shader.
- Add the ability to have multiple headset tracking drivers active at once.
- Add `strength` and `frequency` parameters to `lovr.headset.vibrate`.
- Add `lovr.headset.getDisplayMask`.
- Add `lovr.headset.isTracked`.
- Add support for hexcode colors.
- Add `vec2` and `vec4` objects.
- Add vector properties and vector swizzles.
- Add support for loading KTX and ASTC textures.
- Add `TextureData:paste` for copying between TextureData objects.
- Add `TextureData:getFormat`.
- Add default hotkeys: Escape to quit and F5 to reload, configurable using the `t.hotkeys` conf flag.
- Add `flags` argument to `lovr.graphics.newShader` for configuring shaders without writing GLSL.
- Add `DefaultShader` enum for creating instances of built-in shaders.
- Add an `animated` shader flag to automatically make shaders animated.
- Add `lovr.graphics.tick` and `lovr.graphics.tock` for accurate GPU profiling.
- Add `getPose` and `setPose` functions to `lovr.audio`, `Source`, and `Collider`.
- Add `Model:pose` and `Model:getNodePose` functions for modifying individual joints in a Model.
- Add support for cubic spline animation keyframe interpolation in glTF assets.
- Add support for embedded base64 data in glTF assets.
- Add `astc`, `dxt`, `instancedstereo`, `multiview`, and `timers` GraphicsFeatures.
- Add experimental tup build system as an alternative to CMake.
- Add `lovr.math.drain` function for freeing temporary vectors.
- Add a `t.window.resizable` flag to make the mirror window resizable.
- Add a `t.window.vsync` hint to control vsync (may be ignored if necessary for proper VR timing).
- Add `getNodeCount`, `getMaterialCount`, and `getAnimationCount` to `Model`.

### Change

- Change mobile renderer to use optimized multiview rendering.
- Change all rendering to stall the GPU way less.  Everything is faster.
- Change Android to use LuaJIT by default.
- Change vectors to be temporary by default, use `new`-prefixed functions for permanent vectors.
- Change `lovr.filesystem.read` to accept and return the number of bytes read.
- Change `lovr.filesystem` to follow symbolic links.
- Change `lovr.graphics.newMesh` to accept a `Blob` for binary vertex data.
- Change `fake` headset driver to `desktop`.
- Change `TextureData:getPixel`/`setPixel` to support more formats: `rgb`, `r32f`, `rg32f`, `rgba32f`.
- Change `lovr.graphics.getSystemLimits` to `lovr.graphics.getLimits`.
- Change `lovr.graphics.getSupported` to `lovr.graphics.getFeatures`.
- Change `lovr.graphics.plane` to accept optional uv coordinates to draw a subregion of a Texture.
- Change `Font:getWidth` to also return the number of wrapped lines.
- Change `lovr.thread.newThread` to accept a filename or a `Blob`.
- Change `lovr.headset.getType` to `lovr.headset.getName`, returning the raw headset name string.
- Change `lovr.graphics.circle` size argument to be a radius instead of a diameter.
- Change `MULTICANVAS` shader define to be a `multicanvas` shader flag.
- Change `Source:getDirection`, `:setDirection` to `Source:getOrientation`, `setOrientation`.
- Change nogame screen to use a signed distance field shader instead of an image (!).

### Fix

- Fix problems when creating Blobs with a negative size.
- Fix many bugs or missing features with the glTF importer.
- Fix issue where refcounting was not thread safe.
- Fix vertex winding order of cylinders and spheres.
- Fix an issue where the error screen wouldn't show up when the headset module was disabled.
- Fix console output on Windows.

### Remove

- Remove `Animator` objects, they are replaced by the `Model:animate` function.
- Remove `Controller` objects, equivalent functions that accept a Device are now in `lovr.headset`.
- Remove `lovr.headset.isMounted`, use `lovr.headset.isDown('head', 'proximity')` instead.
- Remove controller-related callbacks (`controlleradded/removed/pressed/released`).
- Remove orientation arguments from `lovr.graphics.skybox`, rotation is automatically applied.
- Remove `Pool` objects, there is a default temporary vector Pool built in to `lovr.math`.
- Remove `t.gammacorrect` conf flag, all rendering is gamma correct now.

v0.12.0 - 2019-02-14
---

### Add

- Add an Android port, with official support for the Oculus Go and Gear VR.
- Add support for the Oculus Desktop SDK (LibOVR).
- Add automatic batching and instancing of draw calls.
- Add `vec3`, `mat4`, `quat`, and `Pool` objects to `lovr.math`.
- Add support for Vive trackers (they are exposed as Controllers).
- Add `go` and `gear` HeadsetTypes.
- Add `oculus` and `oculusmobile` HeadsetDrivers.
- Add the `lovr.mirror` callback for custom rendering to the desktop window.
- Add `Controller:getVelocity` and `Controller:getAngularVelocity`.
- Add `Curve` objects for working with Bézier curves.
- Add `Canvas:getDepthTexture`.
- Add `lovr.headset.getBoundsGeometry` (again).
- Add `lovr.headset.getMirrorTexture`.
- Add `lovr.graphics.discard`.
- Add `lovr.graphics.flush`.
- Add `lovr.graphics.setAlphaSampling` and `lovr.graphics.getAlphaSampling`.
- Add `lovr.graphics.setProjection`.
- Add `lovr.graphics.getPixelDensity`.
- Add `lovr.graphics.hasWindow`.
- Add `ubyte`, `short`, `ushort`, and `uint` types for Mesh attributes.
- Add `Font:hasGlyphs`.
- Add `Font:getRasterizer`.
- Add `Font:isFlipEnabled` and `Font:setFlipEnabled`.
- Add `ShaderBlock:read`.
- Add `lovr.filesystem.getApplicationId`.
- Add the `--root` command line argument to run a project from a directory inside an archive.
- Add a default HTML file used in the emscripten build, so you don't need to write it yourself.

### Change

- Change API functions to accept vectors and matrices in addition to numbers, where relevant.
- Change some matrix and vector math functions to use SIMD intrinsics.
- Change `lovr.headset.setMirrored` and the `t.headset.mirrored` conf.lua flag to take a HeadsetEye.
- Change the mirror window to use the currently active color and shader.
- Change `lovr.graphics.setBlendMode` to accept `nil` to disable blending.
- Change `lovr.graphics.triangle` to take multiple triangles to draw.
- Change `lovr.graphics.cylinder` to take position, rotation, and scale (length) arguments.
- Change `lovr.graphics.newShaderBlock` to take a `BlockType` instead of a `writable` flag.
- Change `lovr.graphics.newMesh` and `lovr.graphics.newShaderBlock` to accept a `readable` flag.
- Change `lovr.graphics.fill` to accept a subregion of the input Texture to use.
- Change `emissive` colors in Materials to default to black instead of white.
- Change `lovr.graphics.getSystemLimits` to return the maximum ShaderBlock size (`blocksize`).
- Change `lovr.filesystem.mount` to accept a path inside the archive to mount.
- Change the `depth` flag of `lovr.graphics.newCanvas` to additionally accept a table with `format` and `readable` keys for creating a Canvas with a readable depth buffer.
- Change `Animator` functions to accept both animation names and indices.
- Change `DrawMode` to `DrawStyle` and `MeshDrawMode` to `DrawMode` (enum values are unchanged).
- Change `DepthFormat` to be merged into `TextureFormat` (enum values are unchanged).
- Change `lovr.graphics.newCanvas` to create Textures with the `clamp` TextureWrap.
- Change `Font:setPixelDensity` to accept `nil` to reset the density to the default.
- Change the `computeshaders` graphics feature to `compute`.
- Change the default font to Varela Round.
- Change `lovr.getOS` to return `Android` on Android.
- Change build system to be more modular and configurable.  Modules can be individually included or excluded, and it's possible to choose whether to use libraries already on the system or compile them from the `deps` folder.

### Fix

- Fix a bug where `lovr.filesystem.unmount` was not exposed.
- Fix a memory corruption when using `Channel:peek`.
- Fix a bug where Microphones wouldn't clean up properly when they were destroyed.
- Fix fonts to render tabs as four spaces instead of...whatever they were doing.

### Remove

- Remove `Transform` objects (use `mat4` instead).
- Remove support for importing FBX models ([`fbx2gltf`](https://github.com/facebookincubator/FBX2glTF) can be used to convert to glTF).
- Remove `VertexData`.
- Remove most of the `ModelData` API.
- Remove `Model:getMesh`.
- Remove `replace` BlendMode.
- Remove `Mesh:drawInstanced` and `Model:drawInstanced` (`draw` takes an instance count now).
- Remove `lovr.headset.setMirrored`, `lovr.headset.isMirrored`, and the `t.headset.mirror` flag.
- Remove variant of `lovr.headset.getPose` (and getPosition/getOrientation) that took a HeadsetEye.
- Remove `ShaderBlock:isWritable` (use `ShaderBlock:getType`).
- Remove `Canvas:getDepthFormat` (use `Canvas:getDepthTexture` instead).

v0.11.0 - 2018-09-05
---

### Add

- Add `Microphone` objects, `lovr.audio.getMicrophoneNames`, and `lovr.audio.newMicrophone`.
- Add `SoundData` objects for accessing individual samples of sound files and `Source:getType`.
- Add support for compute shaders via `lovr.graphics.newComputeShader` and `lovr.graphics.compute`.
- Add `ShaderBlock` objects for efficiently sending large amounts of data to Shaders.
- Add `AudioStream:decode` to decode a chunk of audio and return it as a SoundData.
- Add a new `lineloop` value for `MeshDrawMode`.
- Add `lovr.math.noise` for perlin noise.
- Support `require`ing C libraries (.dll, .so) files as LÖVR modules.
- Add `Material:setTransform` and `Material:getTransform` for using subregions of textures or applying other texture coordinate transformations.
- Add `Texture:getMipmapCount`.
- Add support for new Texture/Canvas formats: `rgba4`, `r16f`, `r32f`, `rg16f`, `rg32f`, `rgb5a1`, `rgb10a2`.
- Add support for loading HDR image files in unclipped floating point texture formats.
- Add `Texture:getFormat`.
- Add `conf.headset.msaa` to control the antialiasing of the headset Canvas (it can be different from the MSAA of the desktop window now).
- Add `lovr.graphics.getSupported` so you can figure out if compute shaders are supported.

### Change

- Change rendering to use single pass stereo rendering on supported graphics cards.  This renders both eyes at the same time which should reduce CPU usage and nearly double rendering performance.
- Change fonts to use stb_truetype instead of FreeType, which seems to improve text quality.
- Change `lovr.audio.newSource` to accept a `SourceType` so you can create a Source backed by either an AudioStream or a SoundData.
- Change `lovr.graphics.newShader` to accept Blobs containing shader source code.
- Change `lovr.filesystem.getRequirePath` and `lovr.filesystem.setRequirePath` to support C require paths.
- Change the default `lovr.filesystem` to look in `lua_modules` for Lua modules for better luarocks integration.
- Change `Texture:replacePixels` to accept an x/y offset and a mipmap level to replace.
- Change `Texture:getWidth`, `Texture:getHeight`, and `Texture:getDimensions` to accept an optional mipmap level.
- Change `lovr.graphics.newShader` to work with strings longer than 8,192 characters.
- Change `lovr.data.newTextureData` to support an optional texture format.
- Change `Transform:getMatrix` to accept a table to fill with values.

### Fix

- Fix an issue where the no game screen's `conf.lua` wasn't loading properly.
- Fix `lovr.event.push` to work with custom event names.
- Fix `lovr.graphics.getBlendMode` to report correct values.
- Fix various memory leaks.

### Remove

- Remove `AudioStream:seek`.
- Remove support for COLLADA models.
- Remove support for loading tga, bmp, and gif textures.

v0.10.0 - 2018-06-13
---

### Add

- Add the `lovr.thread` module, `Thread` objects, `Channel` objects, and the `lovr.threaderror` callback.
- Add the ability to render to multiple Canvas objects simultaneously and `lovr.graphics.getCanvas` and `lovr.graphics.setCanvas`.
- Add support for 2D array textures and 3D volume textures.
- Add the `lovr.mount` callback and `lovr.headset.isMounted` for detecting when/if the headset is on a head.
- Add `lovr.data` module and `AudioStream`, `ModelData`, `Rasterizer`, `TextureData`, and `VertexData` objects.
- Add a variant of `lovr.event.quit` that restarts instead of quitting.
- Add `Texture:replacePixels` for modifying Texture contents.
- Add variants of `lovr.graphics.clear` for clearing the depth and stencil buffers to custom values.
- Add a second parameter to `lovr.graphics.setDepthTest` for controlling whether the depth buffer is written to.
- Add `ModelData:getVertexData` for access to raw vertex data in models.
- Add new `MaterialTexture`s for PBR maps: `emissive`, `metalness`, `roughness`, `occlusion`, and `normal`.  They are loaded for new Models by default.
- Add `MaterialScalar`, `Material:getScalar`, and `Material:setScalar` for metalness and roughness properties.
- Add an `emissive` `MaterialColor`.
- Add a `mipmaps` flag to `lovr.graphics.newTexture` and `lovr.graphics.newCanvas`.
- Add `lovr.audio.getDopplerEffect` and `lovr.audio.setDopplerEffect` (removed in a previous version).
- Add `lovrTangent` as a vec3 accessible to Shaders and load tangents from Models.
- Add `Mesh:attachAttributes` and `Mesh:detachAttributes` for improved instancing support.
- Add `lovr.filesystem.getRequirePath` and `lovr.filesystem.setRequirePath`.
- Add `lovr.filesystem.getWorkingDirectory`.
- Add `Canvas:newTextureData` for reading pixel data from Canvas objects.

### Change

- Change `lovr.timer.getFPS` to average over the last 90 frames instead of 60.
- Change `lovr.graphics.newCanvas` to error when given negative/zero sizes.
- Change `Canvas:renderTo` to error if a function isn't supplied to it.
- Change `Canvas:renderTo` to pass extra arguments to the render callback.
- Change `lovr.graphics.plane` to accept separate x/y scales for the plane.
- Change `Source:getChannels` to `Source:getChannelCount`.
- Change the fake headset driver to use right click instead of left click for the controller trigger.
- Change `lovr.controllerpressed` and `lovr.controllerreleased` to pass `nil` instead of `'unknown'` for unknown controller buttons.
- Change `lovr.graphics.newMesh` and `Mesh:setVertices` to accept a `VertexData` object to load vertices from.
- Change `Mesh:setVertexMap` to accept a `Blob` for fast vertex map updating.
- Change `Shader:send` to accept a `Blob` for updating Shaders with arbitrary binary data.
- Change `Collider:getShapeList` to `Collider:getShapes`.
- Change `Collider:getJointList` to `Collider:getJoints` and also fix it.
- Change the fake headset driver to respect the `lovr.headset.setMirrored` function.
- Change the default window size.
- Change the main loop to a cooperative coroutine-based loop.
- Change `Blob:getFilename` to `Blob:getName`.

### Fix

- Fix sphere UV coordinates to be flipped for correct texture mapping.
- Fix issues with transforms of non-animated models.
- Fix `require` when used with dot-separated module paths.
- Fix a bug where `conf.lua` would be loaded from the working directory if one wasn't present in the project.
- Fix a crash when `lovr.headset.getDisplayDimensions` was called before the first frame was rendered.
- Fix `Joint:getType` returning `nil` for DistanceJoints.
- Fix `Texture:setFilter` not doing anything for cubemap textures.
- Fix a bug where sources would sometimes not be able to play again after ending.
- Fix `lovr.audio.newSource` to show an error message when passed an invalid file.
- Fix an edge case in quaternion interpolation that caused some animations with rotations to be bad.
- Fix `Model:getAABB`.
- Fix the winding order of cubes rendered with `lovr.graphics.cube`.
- Fix `lovr.load` to correctly pass in the argument table.

### Remove

- Remove the `lovr.step` callback.  It is now returned from `lovr.run`.
- Remove `lovr.headset.getBoundsGeometry`.
- Remove `lovr.headset.isPresent`.
- Remove the ability to modify the view matrix and the `MatrixType` enum.
- Remove the variant `lovr.graphics.plane(texture)` for drawing fullscreen quads, it has its own `lovr.graphics.fill` function now.
- Remove a variant of `lovr.filesystem.newBlob` that creates a Blob from a string, use `lovr.data.newBlob` instead.

v0.9.1 - 2017-12-31
---

### Add

- Add `Canvas` objects.
- Add floating point texture formats.
- Add official support for Windows Mixed Reality headsets through OpenVR.
- Add `lovr.graphics.stencil`, `lovr.graphics.getStencilTest`, and `lovr.graphics.setStencilTest`.
- Add `lovr.headset.getPose` and `Controller:getPose`.
- Add `lovr.graphics.getStats`.

### Change

- Change error messages to be better when requiring or loading files that don't exist.

### Fix

- Fix a bug where updating array uniforms wasn't working.
- Fix a bug where animations had their weight initially set to 0.
- Fix a bug where controller render models weren't working properly.

### Remove

- Remove `Texture:renderTo` and variants of `lovr.graphics.newTexture` that created framebuffers.
- Remove `lovr.headset.getEyePosition`.  An HeadsetEye can now optionally be specified as the first argument to `lovr.headset.getPosition`, `lovr.headset.getOrientation`, and `lovr.headset.getPose`.

v0.9.0 - 2017-12-01
---

### Add

- Add `Material` objects.  Materials are sets of colors and textures that can be applied to Models and Meshes.  Models automatically load referenced materials and textures.
- Add `Animator` objects for skeletal animation playback.
- Add fallback mouse/keyboard controls used when VR hardware is not present.
- Add support for loading glTF models.
- Add gamma correct rendering, `lovr.graphics.isGammaCorrect`, and the `gammacorrect` option to `conf.lua`.
- Add `lovr.math.gammaToLinear` and `lovr.math.linearToGamma`.
- Add `lovr.graphics.circle`.
- Add `lovr.graphics.arc`.
- Add `lovr.math.orientationToDirection`.
- Add `Transform:getMatrix` and `Transform:setMatrix`.
- Add support for vertex colors and add the built in `lovrVertexColor` vertex attribute to shaders.
- Add `Model:getMesh`.
- Add `Mesh:drawInstanced` and `Model:drawInstanced`.
- Add `lines` and `linestrip` draw modes to Meshes.
- Add `Shader:hasUniform`.
- Add the ability to customize VR runtime preferences using the `headset.drivers` table in `conf.lua`.

### Change

- Change `Shader:send` to accept more types of uniforms, including arrays.
- Change `lovr.graphics.newTexture` to accept 6 arguments to create a cube map texture.
- Change functions that deal with colors to use a floating point range of 0 - 1 instead of 0 - 255.

### Fix

- Fix `Controller:getHand`, `lovr.graphics.setPointSize`, HRTF audio, and ZIP archive mounting for WebVR.
- Fix a bug with `World:raycast` that occurred when the ray hit multiple things.
- Fix loading and rendering of more complicated models with node hierarchies and multiple materials.
- Fix a bug with text rendering that caused flickering.
- Fix Shader error messages to include correct line numbers.
- Fix crash on Windows that occurred when paths had non ASCII characters.

### Remove

- Remove `Model:setTexture`, `Model:getTexture`, `Mesh:setTexture`, and `Mesh:getTexture`.  They have been replaced by `Model:setMaterial`, `Model:getMaterial`, `Mesh:setMaterial`, and `Mesh:getMaterial`.
- Remove `Skybox` object.  It has been replaced with `lovr.graphics.skybox`.
- Remove the ability to pass Textures as the optional first argument of drawing primitives (Materials can be passed instead).
- Remove `lovr.filesystem.exists`.  The `lovr.filesystem.isFile` and `lovr.filesystem.isDirectory` functions can be used instead.

v0.8.0 - 2017-08-27
---

### Add

- Add support for Linux.
- Add official support for Oculus Touch.
- Add the `lua-enet` library for multiplayer networking.
- Add signed distance field font rendering.
- Add support for compressed textures (DXT1, DXT3, and DXT5).
- Add texture mipmaps.
- Add support for trilinear and anisotropic texture sampling.
- Add `anisotropy` system limit returned by `lovr.graphics.getSystemLimits`.
- Add `lovr.graphics.getDefaultFilter` and `lovr.graphics.setDefaultFilter`.
- Add `lovr.math.lookAt`.
- Add `lovr.math.random`, `lovr.math.randomNormal`, and `RandomGenerator` objects.
- Add new controller buttons: `unknown`, `trigger`, `a`, `b`, `x`, and `y`.
- Add the `grip` `ControllerAxis`.
- Add `Controller:getHand` and `ControllerHand`.
- Add `Controller:isTouched`.
- Add `HeadsetOrigin`, `lovr.headset.getOriginType`, and `headset.offset` option to `conf.lua`.
- Add `HeadsetType`.
- Add `lovr.graphics.createWindow` and `window` options to `conf.lua`.
- Add `lovrModel` and `lovrView` uniform matrices for shaders.
- Add `MatrixType` and allow transform stack functions to manipulate either the model or the view matrix.

### Change

- Change emscripten build to compile to WebAssembly by default.
- Change `lovr.graphics.plane` to take an angle/axis orientation instead of a normal vector.
- Change `FilterMode`.  The new values are `nearest`, `bilinear`, `trilinear`, and `anisotropic`.
- Change the default depth test from `less` to `lequal`.
- Change `lovr.graphics.translate`, `lovr.graphics.rotate`, `lovr.graphics.scale`, and `lovr.graphics.transform` to accept an optional `MatrixType` as the first parameter to control whether the model matrix or the view matrix is affected.
- Change the version of OpenVR to 1.0.9.

### Fix

- Fix bug where using custom fonts could cause a crash.
- Fix crash when the graphics module was disabled.
- Fix `lovr.graphics.setFont` when specifying a Font of `nil`.
- Fix fullscreen framebuffer texture coordinates.
- Fix the error screen to use a more readable font size.
- Fix `lovr.headset.getType` to return correct values.
- Fix the error reporting mechanism to capture many more kinds of errors and show the error screen for them instead of printing the message to the console and closing the window.
- Fix several graphics performance issues.
- Fix the normal matrix passed to shaders.
- Fix issues with nonuniform scaling transformations.

### Remove

- Remove `lovr.graphics.getScissor` and `lovr.graphics.setScissor`.
- Remove `lovr.graphics.getColorMask` and `lovr.graphics.setColorMask`.
- Remove undocumented `Texture:bind` function.

v0.7.1 - 2017-06-23
---

### Add

- Add `lovr.graphics.cylinder`.
- Add `lovr.graphics.sphere`.
- Add `lovr.graphics.box`.
- Add `Model:getAABB`.
- Add `lovrNormalMatrix` uniform to shaders.

### Change

- Change the minimum required OpenGL version from 2.1 to 3.3.
- Change the minimum required OpenGL ES version from 2.0 to 3.0.
- Change the minimum required WebGL version from 1.0 to 2.0.
- Change `lovr.math.newTransform`, `Transform:setTransformation`, and `lovr.graphics.transform` to accept 3 scale parameters instead of 1.

### Fix

- Fix `Mesh:getVertexMap` indices.
- Fix bug where Meshes wouldn't unmap sometimes.
- Fix `Collider:getAABB`.
- Fix `Collider:applyTorque`.

### Remove

- Remove `lovr.audio.getDopplerEffect` and `lovr.audio.setDopplerEffect`.

v0.7.0 - 2017-06-11
---

### Add

- Add `lovr.physics` module (YES!)
- Add `lovr.timer.getAverageDelta`

### Change

- Change the window to no longer be resizable.
- Change the window to have an improved title.
- Improve error messages in several places.

### Fix

- Fix unintended interactions between window mirroring and shaders.
- Fix memory leak in `require`.
- Fix lack of clean up when using Mesh vertex maps.
- Fix occasional crash when transform stack overflows.
- Fix WebVR rendering bug when drawing controllers.

v0.6.0-2017-05-06
---

### Add

- Add support for JSON encoding and decoding using lua-cjson.

### Change

- Add support for additional OpenGL versions including OpenGL ES.
- Add support for running in a web browser with WebVR.

### Fix

- Fix potential crash when loading equirectangular skyboxes.
- Fix crash on windows if the save directory does not exist.

### Remove

- Remove headset bounds visibility functions.

v0.5.0 - 2017-04-03
---

### Add

- Add a no game screen.
- Add an error screen.
- Add `lovr.controllerpressed` and `lovr.controllerreleased` callbacks.
- Add `lovr.focus` callback.
- Add support for creating skyboxes from a single equirectangular image.
- Add `lovr --version` command line flag.
- Add `lovr.getOS`.
- Add `Source:getFalloff` and `Source:setFalloff`.
- Add `Source:getVolumeLimits` and `Source:setVolumeLimits`.
- Add `Source:getCone` and `Source:setCone`.
- Add `Source:isRelative` and `Source:setRelative`.
- Add `Source:getVelocity` and `Source:setVelocity`.
- Add `lovr.audio.getVelocity` and `lovr.audio.setVelocity` (called automatically in `lovr.run`).
- Add `lovr.audio.getDopplerEffect` and `lovr.audio.setDopplerEffect`.
- Add `lovr.audio.isSpatialized`.
- Add `lovr.filesystem.load`.
- Add `lovr.filesystem.createDirectory`.
- Add `lovr.filesystem.getAppdataDirectory`.
- Add `lovr.filesystem.getDirectoryItems`.
- Add `lovr.filesystem.getLastModified`.
- Add `lovr.filesystem.getSaveDirectory`.
- Add `lovr.filesystem.getSize`.
- Add `lovr.filesystem.isFused`.
- Add `lovr.filesystem.newBlob`.
- Add `lovr.filesystem.remove`.
- Add `lovr.filesystem.mount` and `lovr.filesystem.unmount`.
- Add `Mesh:isAttributeEnabled` and `Mesh:setAttributeEnabled`.
- Add `lovr.headset.isMirrored` and `lovr.headset.setMirrored`.  The initial value can be set in `conf.lua` (`t.headset.mirrored`).
- Add parameters to `lovr.graphics.print` for aligning text horizontally and vertically.
- Add `Font:getPixelDensity` and `Font:setPixelDensity` for controlling scales of fonts independent of units.
- Add `Font:getWidth`, `Font:getHeight`, `Font:getAscent`, `Font:getDescent`, and `Font:getBaseline`.
- Add `lovr.graphics.getBlendMode` and `lovr.graphics.setBlendMode`.
- Add `lovr.graphics.getSystemLimits` for returning the maximum point size, the maximum texture size, and the maximum supported msaa level for render textures.

### Change

- Rename `Buffer` to `Mesh`.
- `lovr.graphics.scale` accepts a single argument that will scale all 3 axes.
- `lovr.graphics.rotate` defaults to rotating around the y axis.
- Allow sending `Transform`s to `mat4` uniforms in shaders.
- Framebuffers now clear their depth buffer when created.
- Rename `Source:getOrientation` and `Source:setOrientation` to `Source:getDirection` and `Source:setDirection`.
- Allow `lovr.headset.getEyePosition` to accept `nil` as a parameter.
- Change the signature of `lovr.graphics.print`.
- Change functions that created resources from files to accept `Blob`s.

### Fix

- Fix coordinate system not resetting when calling `lovr.graphics.reset`.
- Explicitly error when attempting to position a stereo Source.
- Fix a few edge cases in reference counting that resulted in crashes.
- Fix a crash if the error handler caused an error.

### Remove

- Remove `Source:getOrientation` and `Source:setOrientation` (renamed to `getDirection` and `setDirection`).
- Remove support for several 3D model file formats to reduce executable size.  The supported formats are now OBJ, FBX, and COLLADA.
- Remove support for several physfs archive formats to reduce executable size.  The only supported format is now ZIP files.
- Remove `lovr.graphics.setProjection`.

v0.4.0 - 2017-02-17
---

### Add

- Add `lovr.math` module and `Transform` objects.
- Add `lovr.graphics.transform`.
- Add support for font rendering.
- Add `Buffer:getVertexFormat`.
- Add `lovr.headset.getEyePosition(eye)`.

### Change

- Change `lovr.graphics.cube`, `Model:draw`, and `Buffer:draw` to accept a `Transform`.
- Change `lovr.graphics.newModel` to accept a `Texture` to apply to the model as the second argument.

### Fix

- Fix memory allocation issue in `lovr.event.poll`.
- Fix crash when SteamVR is unavailable.
- Fix Skybox rendering when culling is enabled.

v0.3.0 - 2017-01-15
---

### Add

- Add support for framebuffers using `Texture:renderTo`.
- Add functions for drawing textured primitives by passing a Texture instead of a DrawMode to `lovr.graphics.cube` and `lovr.graphics.plane`.
- Add shorthand for drawing fullscreen quads with `lovr.graphics.plane(texture)`.

### Change

- Upgrade to OpenVR 1.0.5.
- headset display to the window on the desktop.

v0.2.0 - 2017-01-06
---

### Add

- Add `lovr.audio` module.
- Add support for array uniforms in Shaders.

### Fix

- Fix `lovr.headset.getOrientation` and `Controller:getOrientation`

v0.1.0 - 2016-12-08
---

### Add

- Add `lovr.event` module.
- Add `lovr.filesystem` module.
- Add `lovr.graphics` module.
- Add `lovr.headset` module.
- Add `lovr.timer` module.
