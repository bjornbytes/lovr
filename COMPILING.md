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

These can be found as submodules in the `deps` directory of the repository.  To initialize the
submodules, clone LÖVR with the `--recursive` option or run this command in an existing repo:

```sh
git submodule update --init
```

Windows
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

macOS
---

Install the dependencies using your package manager of choice:

```sh
brew install assimp glfw3 luajit physfs freetype openal-soft ode
```

You may need to set the `PKG_CONFIG_PATH` environment variable for OpenAL to be located properly.
If you run into this, see [Troubleshooting](#troubleshooting) below for more info.

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

Linux
---

First, install the dependencies using your package manager of choice.

#### Arch Linux

```sh
pacman -S assimp glfw-x11 luajit physfs freetype2 openal ode
```

#### Debian/Ubuntu

```sh
sudo apt-get install build-essential cmake libassimp-dev libglfw3-dev libluajit-5.1-dev libphysfs-dev libfreetype6-dev libopenal-dev libode-dev
```

Then, build with CMake:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

On Linux, LÖVR needs to run within the Steam Runtime.  To do this, first [install
Steam](https://wiki.archlinux.org/index.php/Steam#Installation).  Next, [install the Steam udev
rules](https://github.com/ValveSoftware/SteamVR-for-Linux#usb-device-requirements).  Then, run LÖVR
within the Steam runtime:

```sh
~/.steam/steam/ubuntu12_32/steam-runtime/run.sh lovr
```

If you receive errors related to `libstdc++`, set the `LD_PRELOAD` environment variable when running
the command:

```sh
LD_PRELOAD='/usr/$LIB/libstdc++.so.6 /usr/$LIB/libgcc_s.so.1' ~/.steam/steam/ubuntu12_32/steam-runtime/run.sh lovr
```

Currently, there are performance issues between SteamVR and OpenGL apps.  These are being rapidly
resolved with newer versions of graphics drivers and SteamVR.

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
- On macOS, if you run into an error message about not being able to find OpenAL, make sure you've
  added the proper OpenAL directory (usually something like
  `/usr/local/opt/openal-soft/lib/pkgconfig`) to your `PKG_CONFIG_PATH` environment variable.
  Installing openal-soft with brew will print out a message telling you how to do this.
