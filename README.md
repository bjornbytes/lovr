<p align="center"><a href="https://lovr.org"><img src="https://lovr.org/static/img/README.png" width="256"/></a></p>

# LÖVR

> **A simple Lua framework for rapidly building VR experiences.**

You can use LÖVR to easily create VR experiences without much setup or programming experience.  The framework is tiny, fast, open source, and supports lots of different platforms and devices.

[![Build status](https://ci.appveyor.com/api/projects/status/alx3kdi35bmxka8c/branch/master?svg=true)](https://ci.appveyor.com/project/bjornbytes/lovr/branch/master)
[![Version](https://img.shields.io/github/release/bjornbytes/lovr.svg?label=version)](https://github.com/bjornbytes/lovr/releases)
[![Slack](https://img.shields.io/badge/chat-slack-7e4e76.svg)](https://lovr.org/slack)

[**Homepage**](https://lovr.org) | [**Documentation**](https://lovr.org/docs) | [**FAQ**](https://lovr.org/docs/FAQ)

<p align="left">
  <span><img src="http://lovr.org/static/img/wattle.jpg" width="32%"/></span>
  <span><img src="http://lovr.org/static/img/levrage.jpg" width="32%"/></span>
  <span><img src="http://lovr.org/static/img/planets.jpg" width="32%"/></span>
</p>

Features
---

- **Cross-Platform** - Runs on Windows, Mac, Linux, Android, and even on the web using WebAssembly and WebVR.
- **Cross-Device** - Supports Vive/Index, Oculus Rift/Go/Quest, Windows MR, and includes a desktop VR simulator.
- **Beginner-friendly** - Simple VR scenes can be created in just a few lines of Lua.
- **Fast** - Writen in C99 and scripted with LuaJIT, includes optimized single-pass stereo rendering.
- **Asset Import** - Supports 3D models (glTF, OBJ), skeletal animation, HDR textures, cubemaps, fonts, etc.
- **Spatialized Audio** - Audio is automatically spatialized using HRTFs.
- **Vector Library** - Efficient first-class support for 3D vectors, quaternions, and matrices.
- **3D Rigid Body Physics** - Including 4 collider shapes and 4 joint types.
- **Compute Shaders** - For high performance GPU tasks, like particles.
- **Multiplayer** - Includes enet for multi-user VR experiences.

Getting Started
---

It's really easy to get started making things with LÖVR.  Grab a copy of the executable from <https://lovr.org/download>,
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

#### Hand Tracking

```lua
function lovr.draw()
  for _, hand in ipairs(lovr.headset.getHands()) do
    lovr.graphics.sphere(vec3(lovr.headset.getPosition(hand)), .1)
  end
end
```

#### 3D Models

```lua
function lovr.load()
  model = lovr.graphics.newModel('model.gltf')
end

function lovr.draw()
  model:draw(x, y, z)
end
```

You can try more examples in your browser on the [docs page](https://lovr.org/docs/Hello_World).

Building
---

You can build LÖVR from source using CMake.  Here are the steps using the command line:

```console
mkdir build
cd build
cmake ..
cmake --build .
```

See the [Compiling Guide](https://lovr.org/docs/Compiling) for more info.

Resources
---

- [**Documentation**](https://lovr.org/docs): Guides, tutorials, examples, and API documentation.
- [**FAQ**](https://lovr.org/docs/FAQ): Frequently Asked Questions.
- [**Slack Group**](https://lovr.org/slack): For general LÖVR discussion and support.
- [**matrix.org**](https://matrix.to/#/!XVAslexgYDYQnYnZBP:matrix.org): Decentralized alternative to Slack.
- [**Nightly Builds**](https://lovr.org/download/nightly): Nightly builds for Windows.
- [**Compiling Guide**](https://lovr.org/docs/Compiling): Information on compiling LÖVR from source.
- [**Contributing**](https://lovr.org/docs/Contributing): Guide for helping out with development 💜
- [**LÖVE**](https://love2d.org): LÖVR is heavily inspired by LÖVE, a 2D game framework.

Contributors
---

- [@bjornbytes](https://github.com/bjornbytes)
- [@shakesoda](https://github.com/shakesoda)
- [@bcampbell](https://github.com/bcampbell)
- [@mcclure](https://github.com/mcclure)
- [@nevyn](https://github.com/nevyn)
- [@porglezomp](https://github.com/porglezomp)

License
---

MIT, see [`LICENSE`](LICENSE) for details.
