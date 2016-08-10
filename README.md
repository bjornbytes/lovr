LÖVR
===

LÖVR is a framework for making VR games with Lua!  Inspired heavily by [LÖVE](http://love2d.org).  Still under heavy development.

Example
---

In a directory called `myGame`, create a file called `main.lua`:

```lua
function lovr.update(dt)
  print(lovr.headset.getPosition())
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
- GLFW (3.2) and OpenGL (3.2)
- assimp
- OSVR

Compiling
---

CMake:

```sh
mkdir build
cd build
cmake ..
```

tup (OSX only):

```sh
tup
./lovr
```
