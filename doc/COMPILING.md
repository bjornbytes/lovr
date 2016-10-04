Compiling
===

How to compile LÖVR from source.

Dependencies
---

- LuaJIT
- GLFW (3.2+) and OpenGL (3.2+) and GLEW
- assimp (`lovr.model` and `lovr.graphics.newModel`)
- OpenVR (`lovr.headset`)

Windows (Using CMake)
---

First, install [lovr-deps](https://github.com/bjornbytes/lovr-deps):

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

### OSX (tup)

Used for development, not generally recommended.

One-time setup:

```sh
cd lovr
git clone git@github.com:ValveSoftware/openvr ..
export DYLD_LIBRARY_PATH=`pwd`/../openvr/lib/osx32
brew install assimp glfw3 luajit tup
```

Compiling:

```sh
tup
```

Running a game:

```sh
$ ./lovr /path/to/game
```
