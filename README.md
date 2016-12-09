LÖVR
===

LÖVR is a simple framework for creating virtual reality experiences with Lua.  For more information,
visit the [website](http://bjornbyt.es/lovr).

Getting Started
---

You can download LÖVR from the [LÖVR website](http://bjornbyt.es/lovr).  Once you have `lovr.exe`,
create a folder called `myGame` to hold the files for your game.  Next, create a file called
`main.lua` in `myGame` and put this in it:

```lua
function lovr.draw(eye)
  local x, y, z = 0, .25, -2
  local size = 1
  local angle = lovr.timer.getTime()

  lovr.graphics.setColor(128, 0, 255)
  lovr.graphics.cube('line', x, y, z, size, angle)
end
```

Finally, start SteamVR and drag the `myGame` folder onto `lovr.exe`.  Put on your headset and you
should see a spinning purple cube!

#### 3D Models

LÖVR supports most 3D model file formats:

```lua
function lovr.load()
  model = lovr.graphics.newModel('teapot.fbx')
end

function lovr.draw()
  model:draw()
end
```

For more examples, check out the [`examples`](examples) folder.

Hardware Support
---

- HTC Vive (full support via OpenVR)
- Oculus Touch (partial support via OpenVR)
- WebVR (partial support, see `emscripten` branch)
- Mobile VR (no support currently)

Documentation
---

See <http://bjornbyt.es/lovr/docs> for examples and API reference.  The documentation is open source
and can be found [here](https://github.com/bjornbytes/lovr-docs).

Compiling
---

You might want to compile LÖVR from source so you can use LÖVR on other operating systems or create
a custom build.

### Dependencies

- LuaJIT
- GLFW (3.2+)
- OpenGL (Unix) or GLEW (Windows)
- assimp (for `lovr.model` and `lovr.graphics.newModel`)
- OpenVR (for `lovr.headset`)
- PhysicsFS

#### Windows (CMake)

First, install [lovr-deps](https://github.com/bjornbytes/lovr-deps):

```sh
cd lovr
git clone --recursive https://github.com/bjornbytes/lovr-deps deps
```

Next, use CMake to generate the build files:

```sh
mkdir build
cd build
cmake ..
```

This should output a Visual Studio solution, which can be built using Visual Studio.  Or you can
just build it with CMake:

```sh
cmake --build .
```

The executable will then exist at `/path/to/lovr/build/Debug/lovr.exe`.  The recommended way to
create and run a game from this point is:

- Create a shortcut to the `lovr.exe` executable somewhere convenient.
- Create a folder for your game: `MySuperAwesomeGame`.
- Create a `main.lua` file in the folder and put your code in there.
- Drag the `MySuperAwesomeGame` folder onto the shortcut to `lovr.exe`.

#### Unix (CMake)

First, clone [OpenVR](https://github.com/ValveSoftware/openvr).  For this example, we'll clone
`openvr` into the same directory that `lovr` was cloned into.  Next, install the other dependencies
above using your package manager of choice:

```sh
brew install assimp glfw3 luajit physfs
```

On OSX, you'll need to set the `DYLD_LIBRARY_PATH` environment variable to be
`/path/to/openvr/lib/osx32`.

Next, build using CMake:

```sh
mkdir build
cd build
cmake .. -DOPENVR_DIR=../openvr
cmake --build .
```

The `lovr` executable should exist in `lovr/build` now.  You can run a game like this:

```sh
./lovr /path/to/myGame
```

You can also copy or symlink LÖVR into a directory on your `PATH` environment variable (e.g.
`/usr/local/bin`) and run games from anywhere by just typing `lovr`.

License
---

MIT, see [`LICENSE`](LICENSE) for details.
