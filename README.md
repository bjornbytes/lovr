# <p align="center"><a href="https://lovr.org"><img src="https://lovr.org/static/img/README.png" width="256"/></a></p>

> **A simple Lua framework for rapidly building VR experiences.**

[![Build status](https://ci.appveyor.com/api/projects/status/alx3kdi35bmxka8c/branch/master?svg=true)](https://ci.appveyor.com/project/bjornbytes/lovr/branch/master)
[![Version](https://img.shields.io/github/release/bjornbytes/lovr.svg?label=version)](https://github.com/bjornbytes/lovr/releases)

[**Site**](https://lovr.org) | [**Docs**](https://lovr.org/docs) | [**Slack**](https://lovr.org/slack)

<p align="left">
  <span><img src="http://lovr.org/static/img/wattle.jpg" width="32%"/></span>
  <span><img src="http://lovr.org/static/img/levrage.jpg" width="32%"/></span>
  <span><img src="http://lovr.org/static/img/planets.jpg" width="32%"/></span>
</p>

Features
---

- **Cross-Platform** - Runs on Windows, Mac, Linux, and even on the web using WebAssembly and WebVR.
- **Cross-Device** - Support for many HMDs including HTC Vive, Oculus Touch, and Windows MR headsets.  There's also a keyboard/mouse VR simulator so you can prototype without hardware.
- **Beginner-friendly** - Simple VR scenes can be created in just a few lines of Lua.
- **Fast** - Writen in C99 and scripted with LuaJIT, includes optimized single-pass stereo rendering.
- **Asset Import** - Supports 3D models (glTF, FBX, OBJ), skeletal animation, HDR textures, cubemaps, fonts, etc.
- **Spatialized Audio** - Audio is automatically spatialized using HRTFs.
- **3D Rigid Body Physics** - Including 4 collider shapes and 4 joint types.
- **Compute Shaders** - For high performance GPU tasks, like particles.
- **Multiplayer** - Includes enet for multi-user VR experiences.
- **Umlauts** - !!!

Getting Started
---

It's really easy to get started making things with LÖVR!  Grab a copy of the executable from <https://lovr.org/download>,
then write a `main.lua` script and drag its parent folder onto the executable.  Here are some example projects to try:

#### Hello World

```lua
function lovr.draw()
  lovr.graphics.print('Hello World!', 0, 1.7, -3, .5)
end
```

#### Spinning Cube

```lua
function lovr.draw()
  lovr.graphics.cube('line', 0, 1.7, -1, .5, lovr.timer.getTime())
end
```

#### 3D Models

```lua
function lovr.load()
  model = lovr.graphics.newModel('teapot.fbx', 'teapot.png')
end

function lovr.draw()
  local x, y, z = 0, 0, 0
  model:draw(x, y, z)
end
```

You can also find lots of other WebVR examples on the [docs page](https://lovr.org/docs/Hello_World).

Resources
---

- [**Documentation**](https://lovr.org/docs): Guides, tutorials, examples, and API documentation.
- [**Slack Group**](https://lovr.org): For general LÖVR discussion and support.
- [**Nightly Builds**](https://lovr.org/download/nightly): Nightly builds for Windows.
- [**Compiling Guide**](https://lovr.org/docs/Compiling): Information on compiling LÖVR from source.
- [**Contributing**](CONTRIBUTING.md): Guide for helping out with development :heart:

License
---

MIT, see [`LICENSE`](LICENSE) for details.
