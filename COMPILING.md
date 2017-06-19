Compiling LÖVR
===

You might want to compile LÖVR from source so you can use LÖVR on other operating systems or create
a custom build.  Below is a guide for setting up all the dependencies and compiling the code on
various operating systems.

Dependencies
---

- LuaJIT
- GLFW (3.2+)
- OpenGL (3.3, ES3, or WebGL 2)
- assimp
- OpenVR (1.0.5, for `lovr.headset`)
- PhysicsFS
- OpenAL (1.17+ recommended for HRTF support)
- FreeType
- ODE (for `lovr.physics`)
- Emscripten (optional, for compiling for web)

See [lovr-deps](https://github.com/bjornbytes/lovr-deps) for a GitHub repo containing all of these
as submodules.

Windows (CMake)
---

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

Unix (CMake)
---

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

WebVR
---

First, install the Emscripten SDK.  Make sure you're running [this
branch](https://github.com/bjornbytes/emscripten/tree/lovr) of Emscripten.

Unix:

```sh
mkdir build
cd build
emcmake cmake -DCMAKE_BUILD_TYPE=Release ..
emmake make -j2
```

Windows (from a Visual Studio Command Prompt, make sure the Emscripten SDK is on PATH):

```sh
mkdir build
cd build
emcmake cmake -G "NMake Makefiles" ..
emmake nmake
```

The above commands will output `lovr.html`, `lovr.js`, and `lovr.js.mem`.  To package a game, run:

```
python /path/to/emscripten/tools/file_packager.py game.data --preload /path/to/game@/ --js-output=game.js
```

Which will output `game.js` and `game.data`.  The `lovr.html` file will need to be modified to
include `game.js` in a script tag.

Troubleshooting
---

- If you get "CMake no CMAKE_CXX_COMPILER found" on Windows, then install Visual Studio and create a
  blank C++ project, which will prompt you to install the compilers and tools necessary to compile
  LÖVR.
