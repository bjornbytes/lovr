LÖVR
===

LÖVR lets you make VR games with Lua!

Example
---

In a folder called `myGame`, create a file called `main.lua` with this in it:

```lua
function lovr.update(dt)
  t = (t or 0) + dt
end

function lovr.draw(eye)
  lovr.graphics.setColor(80, 0, 160)
  lovr.graphics.translate(0, 1, -1)
  lovr.graphics.rotate(t * 2, .70, .70, 0)
  lovr.graphics.translate(0, -1, 1)
  lovr.graphics.cube(0, 1, -1, 1)
end

```

Drag the `myGame` folder onto a shortcut to `lovr.exe`.  You should see a spinning purple cube!  See instructions below on compiling `lovr.exe`.

Dependencies
---

- LuaJIT
- GLFW (3.2+) and OpenGL (3.2+)
- assimp
- SteamVR (OpenVR)

Supported Hardware
---

- HTC Vive

Support for other hardware will happen eventually.

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

This should output a Visual Studio solution, which can be built using Visual Studio.  Or you can just build it with CMake:

```sh
cmake --build .
```

The executable will then exist at `/path/to/lovr/build/Debug/lovr.exe`.  The recommended way to create and run a game from this point is:

- Create a shortcut to the `lovr.exe` executable somewhere convenient.
- Create a folder for your game: `MySuperAwesomeGame`.
- Create a `main.lua` file in the folder and put your code in there.
- Drag the `MySuperAwesomeGame` folder onto the shortcut to `lovr.exe`.

### OSX (tup)

Used for development, not generally recommended.

One-time setup:

```sh
cd lovr
git clone git@github.com:ValveSoftware/openvr ..
export DYLD_LIBRARY_PATH=`pwd`/../openvr/lib/osx32
brew install assimp glfw3 luajit
```

Compiling:

```sh
tup
```

Running a game:

```sh
$ ./lovr /path/to/game
```
