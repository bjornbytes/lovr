<p align="center"><a href="http://lovr.org"><img src="src/data/logo.png" width="160"></a></p>

<h1 align="center">LÖVR</h1>

LÖVR is a simple framework for creating virtual reality experiences with Lua.

Features
---

- Automatically detects and renders to connected VR headsets (works without a headset too!)
- Simple graphics API supporting 3D primitives, 3D models, fonts, shaders, and skyboxes.
- Export projects to standalone executables, or export to the web using WebVR.
- Spatialized audio for more immersive experiences.
- 3D physics system

Getting Started
---

You can download precompiled binaries from the [website](http://lovr.org).  There, you
can also find documentation and a set of tutorials and examples.  Here is the hello world example
for LÖVR:

```lua
function lovr.draw()
  lovr.graphics.print('Hello World!', 0, 1, -1)
end
```

Put that code in a file called `main.lua`, and drop the `main.lua` folder onto `lovr.exe`.  Put
on your headset and you should see the text at the front of your play area!

#### Spinning Cube

```lua
function lovr.draw()
  lovr.graphics.cube('line', 0, 1, 0, .5, lovr.timer.getTime())
end
```

#### 3D Models

```lua
function lovr.load()
  model = lovr.graphics.newModel('teapot.fbx', 'teapot.png')
end

function lovr.draw()
  model:draw()
end
```

#### Audio

```lua
function lovr.load()
  local sound = lovr.audio.newSource('darudeSandstorm.ogg')
  sound:play()
end
```

For more examples, see <http://lovr.org/examples>.

Documentation
---

See <http://lovr.org/docs> for guides and API reference.  The documentation is open source
and can be found [here](https://github.com/bjornbytes/lovr-docs).

Compiling
---

You might want to compile LÖVR from source so you can use LÖVR on other operating systems or create
a custom build.

### Dependencies

- LuaJIT
- GLFW (3.2+)
- OpenGL (3+)
- assimp
- OpenVR (1.0.5, for `lovr.headset`)
- PhysicsFS
- OpenAL (1.17+ recommended for HRTF support)
- FreeType
- Emscripten (optional, for compiling for web)

See [lovr-deps](https://github.com/bjornbytes/lovr-deps) for a GitHub repo containing all of these
as submodules.

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
`openvr` into the same directory that `lovr` was cloned into.

```sh
git clone --branch v1.0.5 https://github.com/ValveSoftware/openvr.git
```

Next, install the other dependencies above using your package manager of choice:

```sh
brew install assimp glfw3 luajit physfs freetype openal-soft
```

On OSX, you'll need to set the `DYLD_LIBRARY_PATH` environment variable to be
`/path/to/openvr/lib/osx32`.

Next, build using CMake:

```sh
mkdir build
cd build
cmake .. -DOPENVR_DIR=../../openvr
cmake --build .
```

The `lovr` executable should exist in `lovr/build` now.  You can run a game like this:

```sh
./lovr /path/to/myGame
```

You can also copy or symlink LÖVR into a directory on your `PATH` environment variable (e.g.
`/usr/local/bin`) and run games from anywhere by just typing `lovr`.

#### WebVR

First, install the Emscripten SDK.  Make sure you're running [this
branch](https://github.com/bjornbytes/emscripten/tree/lovr) of Emscripten.

```sh
mkdir build
cd build
emcmake cmake -DCMAKE_BUILD_TYPE=Release ..
emmake make -j2
```

The above commands will output `lovr.html`, `lovr.js`, and `lovr.js.mem`.  To package a game, run:

```
python /path/to/emscripten/tools/file_packager.py game.data --preload /path/to/game@/ --js-output=game.js
```

Which will output `game.js` and `game.data`.  The `lovr.html` file will need to be modified to
include `game.js` in a script tag.

License
---

MIT, see [`LICENSE`](LICENSE) for details.
