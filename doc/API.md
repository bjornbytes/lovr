`lovr.headset`
---

- `isPresent = isPresent()` - Whether or not the headset is connected and available for use.
- `getType()` - "Vive" for the Vive or "Rift" for the Oculus Rift.  Currently only Vive is
  supported.
- `renderTo(callback)` - Render to the headset using a function.  The callback function will be
  called twice, once for each eye.  This function sets the correct transformation and projection
  matrices before rendering.
- `x, y, z = getPosition()` - Get the position of the headset in meters.
- `angle, axisX, axisY, axisZ = getOrientation()` - Get the orientation of the headset returned in
  angle/axis format (radians).
- `vx, vy, vz = getVelocity()` - Get the velocity of the headset.
- `x, y, z = getAngularVelocity()` - Get the angular velocity of the headset.
  `hand` is "left" or "right".
- `setClipDistance(number near, number far)` - Set the clip distance of the virtual camera.
- `near, far = getClipDistance()` - Retrieve the clip distance of the virutal camera.
- `width, depth = getTrackingSize()` - Dimensions for the rectangle enclosing the available play
  area, in meters.
- `isVisible = isBoundsVisible()` - Whether or not the boundary grid is currently visible.
- `setBoundsVisible(boolean isVisible)` - Request the boundary grid to be shown or hidden.

Controllers:

- `Controller = lovr.headset.getController(string hand)` - Return a controller object for the specified hand.
- `isPresent = Controller:isPresent` - Whether or not the controller is connected and available for
  use.
- `x, y, z = Controller:getPosition()`
- `angle, axisX, axisY, axisZ = Controller:getOrientation()`

Note: The headset module requires SteamVR to properly function.  It can be disabled using a
`conf.lua` file:

```
-- conf.lua
function lovr.conf(t)
  t.modules.headset = false
end
```

`lovr.graphics`
---

Primitives:

- `lovr.graphics.cube(mode, x, y, z, size, angle, axisX, axisY, axisZ)`
  - `mode` is either "fill" or "line".
  - `x`, `y`, `z` default to 0.
  - `size` defaults to 1.0.
  - `angle` is in radians.
  - `axis` values should represent a normalized vector to rotate the cube around.
- `lovr.graphics.plane(mode, x, y, z, size, normalX, normalY, normalZ)`
  - `mode` is either "fill" or "line".
  - `x`, `y`, `z` default to 0.
  - `size` defaults to 1.0.
  - `normal` direction defaults to `(0, 1, 0)` (flat plane on the "ground").
- `lovr.graphics.line(points)`
  - Coordinates need `x`, `y`, and `z` components, and can be specified as arguments or as a single
    table.

Setup and transforms:

- `reset()`, `clear(color, depth)`, `present()` - These are generally called automatically by
  `lovr.run`.
- `push`, `pop`, `origin`, `translate(x, y, z)`, `rotate(angle, axisX, axisY, axisZ)`, `scale(x, y, z)`
- `setProjection(near, far, fov)` - FOV is in radians
- `getWidth`, `getHeight`, `getDimensions` - Return the dimensions of the window in pixels.

Graphics state: All setters have corresponding getters

- `setColor(r, g, b, a)` - Color channels are from 0 to 255.
- `setColorMask(r, g, b, a)` - Boolean values determine which color channels are manipulated when
  drawing.
- `setScissor(x, y, w, h)` - Or use `nil` to disable the scissor.
- `setLineWidth(width)` - Currently at the whim of graphics driver support.
- `setPolygonWinding(winding)` - Either "clockwise" or "counterclockwise".
- `setCullingEnabled(isEnabled)` and `isCullingEnabled()`

Shaders:

- `Shader = lovr.graphics.newShader(string vertex, string fragment)` - Creates a new shader.  Some
  uniforms are automatically available:
  - `lovrTransform` is a `mat4` containing the current View * Model matrix.
  - `lovrProjection` is a `mat4` containing the current projection matrix.
  - `lovrColor` is the color set by `lovr.graphics.setColor`.
  - Source for the default shader:

```
shader = lovr.graphics.newShader([[
void main() {
  gl_Position = lovrProjection * lovrTransform * vec4(position.xyz, 1.0);
}
]], [[
void main() {
  color = lovrColor;
}
]])
```

- `Shader:send(string uniformName, number/table value)` - Send a value to a Shader's uniform. Vectors and
  matrices should be flat tables.

Buffers:

- `Buffer = lovr.graphics.newBuffer(size, mode, usage)`
  - `size` is the number of vertices in the buffer.
  - `mode` is the draw mode.  Supported draw modes are "points", "strip", "triangles", and "fan".
    The default draw mode is "fan".
  - `usage` is a string indicating how the Buffer is intended to be used.  It can be "static",
    "dynamic", or "stream".  The default is "dynamic".
- `Buffer:draw()` - Draws the buffer.
- `Buffer:setDrawMode(mode)` - Sets the draw mode.
- `Buffer:setVertex(index, x, y, z)` - Set the value of a vertex.
- `Buffer:setVertexMap(map)` - Set the draw order of the vertices.  It is possible to re-use
  indices.
- `Buffer:setDrawRange(start, count)` - Limits the vertices draw to a subset of the total.
- `Buffer:getVertexCount()` - Retrieve the number of vertices in the Buffer.

`lovr.event`
---

- `poll` - Automatically called on every update by `lovr.run`.
- `quit` - Quits the game.

`lovr.timer`
---

- `dt = step()` - Measures the time since this function was last called.  This is called
  automatically by `lovr.run` every tick and passed to `lovr.update`.
- `sleep(duration)` - Sleeps for the specified  number of seconds.
