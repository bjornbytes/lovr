LÖVR
===

LÖVR is a framework for making VR games with Lua!  Inspired heavily by [LÖVE](http://love2d.org).  Still under heavy development.

Example
---

In a directory called `myGame`, create a file called `main.lua`:

```lua
function lovr.load()
  headset = lovr.device.getHeadset()
end

function lovr.update(dt)
  print('Headset position:', headset:getPosition())
end
```

Then, start the OSVR server and run your game:

```sh
$ cd path/to/myGame
$ lovr .
```

Dependencies
---

- LuaJIT
- GLFW and OpenGL 4
- assimp
- OSVR

Compiling (CMake)
---

```sh
mkdir build
cd build
cmake ..
```
