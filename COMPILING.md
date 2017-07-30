Compiling LÖVR
===

You might want to compile LÖVR from source so you can use LÖVR on other operating systems or create
a custom build.  Below is a guide for setting up all the dependencies and compiling the code on
various operating systems.

Dependencies
---

- Lua or LuaJIT
- GLFW (3.2+)
- OpenGL (3.3, ES3, or WebGL 2)
- assimp
- OpenVR (1.0.9, for `lovr.headset`)
- PhysicsFS
- OpenAL (1.17+ recommended for HRTF support)
- FreeType
- msdfgen
- ODE (for `lovr.physics`)
- Emscripten (optional, for compiling for web)

The [lovr-deps](https://github.com/bjornbytes/lovr-deps) repository contains all dependencies as
git submodules.  The build script expects this repository to exist under `lovr/deps`:

```sh
cd lovr
git clone --recursive https://github.com/bjornbytes/lovr-deps deps
```

Windows (CMake)
---

From the `lovr` folder, run these commands to create a build folder and compile the project using
CMake:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

The executable will then exist at `/path/to/lovr/build/Debug/lovr.exe`.  A LÖVR project (a folder
containing a `main.lua` script) can then be dropped onto `lovr.exe` to run it, or it can be run
via the command line as `lovr.exe path/to/project`.

Unix (CMake)
---

Install the dependencies using your package manager of choice:

```sh
brew install assimp glfw3 luajit physfs freetype openal-soft ode
```

Next, build using CMake, as above:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

The `lovr` executable should exist in `lovr/build` now.  It's recommended to set up an alias or
symlink so that this executable exists on your path.  Once that's done, you can run a game like this:

```sh
lovr /path/to/myGame
```

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
