Plugins
===

Plugins are optional native libraries that are built with LÖVR.  CMake will scan `plugins` for any
folder with a `CMakeLists.txt` and include it in the build.  Any CMake targets the plugin declares
will automatically include and link with LÖVR's copy of Lua, and any shared libraries they produce
will be copied next to the `lovr` executable, allowing them to be used from Lua via `require`.

Prebuilt plugin libraries can also be copied next to the lovr executable.  However, this won't work
easily for Android builds, since the APK requires re-signing if it is modified.

See the [Plugins documentation](https://lovr.org/docs/Plugins) for more info and a list of known
plugins.

LÖVR includes some "core plugins" here which are included in builds by default.  They are entirely
optional -- their folders here can be removed or their libraries can be deleted after building
without disturbing the LÖVR build.
