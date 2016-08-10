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
- GLFW (3.2+) and OpenGL (3.2+)
- assimp
- OSVR-Core

Compiling
---

### Windows (CMake)

- Install [Boost 1.57](http://www.boost.org/users/history/version_1_57_0.html) to `C:\local\boost_1_57_0`.
- Download and extract a pre-built [OSVR-Core](http://access.osvr.com/binary/osvr-core).
- Install [lovr-deps](https://github.com/bjornbytes/lovr-deps):

```sh
cd lovr
git clone --recursive https://github.com/bjornbytes/lovr-deps deps
```

Finally, build using the CMake GUI or using the CMake command line.  Make sure you pass the `OSVR_PATH` option:

```sh
mkdir build
cd build
cmake -DOSVR_PATH=/path/to/osvr-core ..
```

This should output a Visual Studio solution, which can be built using Visual Studio or by using CMake:

```sh
cmake --build .
```

### OSX (tup)

```sh
tup
```
