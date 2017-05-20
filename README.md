<p align="center"><a href="http://lovr.org"><img src="src/data/logo.png" width="160"></a></p>

<h1 align="center">LÖVR</h1>

LÖVR is a simple framework for creating virtual reality experiences with Lua.

Features
---

- Automatically detects and renders to connected VR headsets (works without a headset too!)
- Simple 3D graphics API supporting primitives, 3D models, fonts, shaders, and skyboxes.
- Export projects to standalone executables, or export to the web using WebVR.
- Spatialized audio
- 3D physics

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

To run it, first create a folder for your project and put the code in a file called `main.lua`.
Then, just drop the `project` folder onto `lovr.exe` (or run `lovr.exe path/to/project` on the
command line).  Put on your headset and you should see the text at the front of your play area!

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

To compile from source to create a custom build or contribute to LÖVR, see
[`COMPILING.md`](COMPILING.md).

License
---

MIT, see [`LICENSE`](LICENSE) for details.
