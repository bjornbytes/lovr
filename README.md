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

Then, run your game:

```sh
$ lovr path/to/myGame
```

Dependencies
---

- LuaJIT
- GLFW (3.2+) and OpenGL (3.2+)
- assimp
- SteamVR (OpenVR) or Oculus SDK

Supported Hardware
---

- Oculus Rift (touch controllers or gamepad)
- HTC Vive

Compiling
---

### Windows (CMake)

- Install [lovr-deps](https://github.com/bjornbytes/lovr-deps):

```sh
cd lovr
git clone --recursive https://github.com/bjornbytes/lovr-deps deps
```

Next, build using the CMake GUI or using the CMake command line.

```sh
mkdir build
cd build
cmake ..
```

This should output a Visual Studio solution, which can be built using Visual Studio or by using CMake:

```sh
cmake --build .
```

The executable will then exist at `/path/to/lovr/build/Debug`.

### OSX (tup)

Used for development, not generally recommended.

```sh
cd lovr
git clone git@github.com:ValveSoftware/openvr ..
export DYLD_LIBRARY_PATH=`pwd`/../openvr/lib/osx32
brew install assimp glfw3 luajit
tup
```
