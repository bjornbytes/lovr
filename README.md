LÖVR
===

LÖVR lets you make VR games with Lua!  See <http://bjornbyt.es/lovr>.

Example
---

In a folder called `myGame`, create a file called `main.lua` with this in it:

```lua
function lovr.update(dt)
  t = (t or 0) + dt
end

function lovr.draw(eye)
  local x, y, z = 0, .25, -2
  local size = 1
  local angle, rx, ry, rz = t * 2, 0, 1, 0

  lovr.graphics.setColor(128, 0, 255)
  lovr.graphics.cube('line', x, y, z, size, angle, rx, ry, rz)
end
```

Drag the `myGame` folder onto a shortcut to `lovr.exe`.  You should see a spinning purple cube!

Supported Hardware
---

- HTC Vive

Support for other hardware will happen eventually.

Compiling
---

See the [compilation guide](doc/COMPILING.md).
