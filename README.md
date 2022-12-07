# LÖVR

<a href="https://lovr.org"><img align="right" src="https://lovr.org/static/img/logo.svg" width="180"/></a>

> **A simple Lua framework for rapidly building VR experiences.**

You can use LÖVR to easily create VR experiences without much setup or programming experience.  The framework is tiny, fast, open source, and supports lots of different platforms and devices.

[![Build](https://github.com/bjornbytes/lovr/actions/workflows/build.yml/badge.svg?event=push)](https://github.com/bjornbytes/lovr/actions/workflows/build.yml)
[![Version](https://img.shields.io/github/release/bjornbytes/lovr.svg?label=version)](https://github.com/bjornbytes/lovr/releases)
[![Matrix](https://img.shields.io/badge/chat-matrix-7e4e76.svg)](https://lovr.org/matrix)

[**Homepage**](https://lovr.org) | [**Documentation**](https://lovr.org/docs) | [**FAQ**](https://lovr.org/docs/FAQ)

<p align="left">
  <span><img src="http://lovr.org/static/img/screen1.jpg" width="32.5%"/></span>
  <span><img src="http://lovr.org/static/img/screen2.jpg" width="32.5%"/></span>
  <span><img src="http://lovr.org/static/img/screen3.jpg" width="32.5%"/></span>
</p>

Features
---

- **Cross-Platform** - Runs on Windows, Mac, Linux, Android, WebXR.
- **Cross-Device** - Supports Vive/Index, Oculus Rift/Quest, Windows MR, and has a VR simulator.
- **Beginner-friendly** - Simple VR scenes can be created in just a few lines of Lua.
- **Fast** - Writen in C11 and scripted with LuaJIT, includes optimized single-pass stereo rendering.
- **Asset Import** - Supports 3D models (glTF, OBJ), skeletal animation, HDR textures, cubemaps, fonts, etc.
- **Spatialized Audio** - Audio is automatically spatialized using HRTFs.
- **Vector Library** - Efficient first-class support for 3D vectors, quaternions, and matrices.
- **3D Rigid Body Physics** - Including 4 collider shapes, triangle mesh colliders, and 4 joint types.
- **Compute Shaders** - For high performance GPU tasks, like particles.

Getting Started
---

It's really easy to get started making things with LÖVR.  Grab a copy of the executable from <https://lovr.org/download>,
then write a `main.lua` script and drag it onto the executable.  Here are some example projects to try:

#### Hello World

```lua
function lovr.draw(pass)
  pass:text('Hello World!', 0, 1.7, -3, .5)
end
```

#### Spinning Cube

```lua
function lovr.draw(pass)
  pass:cube(0, 1.7, -1, .5, lovr.timer.getTime())
end
```

#### Hand Tracking

```lua
function lovr.draw(pass)
  for _, hand in ipairs(lovr.headset.getHands()) do
    pass:sphere(vec3(lovr.headset.getPosition(hand)), .1)
  end
end
```

#### 3D Models

```lua
function lovr.load()
  model = lovr.graphics.newModel('model.gltf')
end

function lovr.draw(pass)
  pass:draw(model, x, y, z)
end
```

More examples are on the [docs page](https://lovr.org/docs/Intro/Hello_World).

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
- [**Matrix Room**](https://matrix.to/#/!XVAslexgYDYQnYnZBP:matrix.org): Decentralized alternative to Slack.
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
- [@jmiskovic](https://github.com/jmiskovic)
- [@wallbraker](https://github.com/wallbraker)

License
---

MIT, see [`LICENSE`](LICENSE) for details.
