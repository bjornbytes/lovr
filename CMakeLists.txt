cmake_minimum_required(VERSION 3.1.0)
cmake_policy(SET CMP0063 NEW)
project(lovr)

# Options
option(LOVR_ENABLE_AUDIO "Enable the audio module" ON)
option(LOVR_ENABLE_DATA "Enable the data module" ON)
option(LOVR_ENABLE_EVENT "Enable the event module" ON)
option(LOVR_ENABLE_FILESYSTEM "Enable the filesystem module" ON)
option(LOVR_ENABLE_GRAPHICS "Enable the graphics module" ON)
option(LOVR_ENABLE_HEADSET "Enable the headset module" ON)
option(LOVR_ENABLE_MATH "Enable the math module" ON)
option(LOVR_ENABLE_PHYSICS "Enable the physics module" ON)
option(LOVR_ENABLE_THREAD "Enable the thread module" ON)
option(LOVR_ENABLE_TIMER "Enable the timer module" ON)

option(LOVR_ENABLE_ENET "Bundle with lua-enet" ON)
option(LOVR_ENABLE_JSON "Bundle with lua-cjson" ON)

option(LOVR_USE_LUAJIT "Use LuaJIT instead of Lua" ON)
option(LOVR_USE_OPENVR "Enable the OpenVR backend for the headset module" ON)
option(LOVR_USE_OPENXR "Enable the OpenXR backend for the headset module" OFF)
option(LOVR_USE_WEBXR "Enable the WebXR backend for the headset module" OFF)
option(LOVR_USE_OCULUS "Enable the LibOVR backend for the headset module (be sure to also set LOVR_OCULUS_PATH to point to the Oculus SDK)" OFF)
option(LOVR_USE_VRAPI "Enable the VrApi backend for the headset module" OFF)
option(LOVR_USE_PICO "Enable the Pico backend for the headset module" OFF)
option(LOVR_USE_DESKTOP_HEADSET "Enable the keyboard/mouse backend for the headset module" ON)
option(LOVR_USE_OCULUS_AUDIO "Enable the Oculus Audio spatializer (be sure to also set LOVR_OCULUS_AUDIO_PATH)" OFF)
option(LOVR_USE_LINUX_EGL "Use the EGL graphics extension on Linux" OFF)

option(LOVR_SYSTEM_ENET "Use the system-provided enet" OFF)
option(LOVR_SYSTEM_GLFW "Use the system-provided glfw" OFF)
option(LOVR_SYSTEM_LUA "Use the system-provided Lua" OFF)
option(LOVR_SYSTEM_ODE "Use the system-provided ODE" OFF)
option(LOVR_SYSTEM_OPENXR "Use the system-provided OpenXR" OFF)

option(LOVR_BUILD_EXE "Build an executable (or an apk on Android)" ON)
option(LOVR_BUILD_SHARED "Build a shared library (takes precedence over LOVR_BUILD_EXE)" OFF)
option(LOVR_BUILD_BUNDLE "On macOS, build a .app bundle instead of a raw program" OFF)

option(LOVR_USE_THREADLOCAL "Allow use of thread local storage; disable to run on Windows XP as a DLL" ON)

# Setup
if(EMSCRIPTEN)
  string(CONCAT LOVR_EMSCRIPTEN_FLAGS
    "-Os "
    "-s USE_WEBGL2=1 "
    "-s FORCE_FILESYSTEM=1 "
    "-s \"EXPORTED_FUNCTIONS=['_main','_lovrDestroy','_webxr_attach','_webxr_detach','_lovrCanvasCreateFromHandle','_lovrCanvasDestroy','_lovrGraphicsSetBackbuffer','_lovrGraphicsSetViewMatrix','_lovrGraphicsSetProjection']\" "
    "-s \"EXTRA_EXPORTED_RUNTIME_METHODS=['getValue','setValue']\" "
    "--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/webxr.js\" "
    "--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lovr.html\""
  )
  if(LOVR_ENABLE_THREAD)
    set(LOVR_EMSCRIPTEN_FLAGS "${LOVR_EMSCRIPTEN_FLAGS} -s USE_PTHREADS=1")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(LOVR_USE_WEBXR ON)
  set(LOVR_USE_OPENVR OFF)
elseif(ANDROID)
  find_package(Java REQUIRED)
  set(LOVR_USE_OPENVR OFF)
  set(LOVR_USE_DESKTOP_HEADSET OFF)
  if(LOVR_BUILD_EXE)
    set(LOVR_BUILD_SHARED ON)
  endif()
elseif(UNIX)
  find_package(PkgConfig)
  if(NOT APPLE)
    set(CMAKE_SKIP_RPATH OFF)
  endif()
endif()

# enet
if(LOVR_ENABLE_ENET AND NOT EMSCRIPTEN)
  if(LOVR_SYSTEM_ENET)
    pkg_search_module(ENET REQUIRED enet)
    include_directories(${ENET_INCLUDE_DIRS})
    set(LOVR_ENET ${ENET_LIBRARIES})
  else()
    add_subdirectory(deps/enet enet)
    include_directories(deps/enet/include)
    set(LOVR_ENET enet)
    if(WIN32)
      set(LOVR_ENET ${LOVR_ENET} ws2_32 winmm)
    endif()
  endif()
endif()

# GLFW
if(NOT (EMSCRIPTEN OR ANDROID))
  if(LOVR_SYSTEM_GLFW)
    pkg_search_module(GLFW REQUIRED glfw3)
    include_directories(${GLFW_INCLUDE_DIRS})
    set(LOVR_GLFW ${GLFW_LIBRARIES})
  else()
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "")
    add_subdirectory(deps/glfw glfw)
    include_directories(deps/glfw/include)
    set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
  endif()
  unset(LIB_SUFFIX CACHE)
endif()

# Lua
if(LOVR_USE_LUAJIT AND NOT EMSCRIPTEN)
  if (APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pagezero_size 10000 -image_base 100000000")
  endif()
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUAJIT REQUIRED luajit)
    include_directories(${LUAJIT_INCLUDE_DIRS})
    set(LOVR_LUA ${LUAJIT_LIBRARIES})
  else()
    add_subdirectory(deps/luajit luajit)
    include_directories(deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
    set(LOVR_LUA libluajit)
  endif()
else()
  if(EMSCRIPTEN)
    option(LUA_USE_RELATIVE_LOADLIB OFF)
    option(LUA_USE_ULONGJMP OFF)
    option(LUA_USE_POPEN OFF)
  endif()
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUA REQUIRED lua)
    include_directories(${LUA_INCLUDE_DIRS})
    set(LOVR_LUA ${LUA_LIBRARIES})
  else()
    add_subdirectory(deps/lua lua)
    include_directories(deps/lua/src ${CMAKE_BINARY_DIR}/lua)
    set(LOVR_LUA liblua_static)
    set_target_properties(lua luac PROPERTIES EXCLUDE_FROM_ALL 1)
  endif()
endif()

# MSDF
if(LOVR_ENABLE_DATA)
  add_subdirectory(deps/msdfgen lib_msdfgen)
  include_directories(deps/msdfgen)
  set(LOVR_MSDF lib_msdfgen)
  if(APPLE)
    set_target_properties(lib_msdfgen PROPERTIES MACOSX_RPATH ON)
  endif()
endif()

# ODE
if(LOVR_ENABLE_PHYSICS)
  if(LOVR_SYSTEM_ODE)
    pkg_search_module(ODE REQUIRED ode)
    pkg_search_module(CCD REQUIRED ccd)
    include_directories(${ODE_INCLUDE_DIRS} ${CCD_INCLUDE_DIRS})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")
    set(LOVR_ODE ode ccd)
  else()
    if(EMSCRIPTEN)
      set(ODE_BUILD_SHARED OFF CACHE BOOL "")
    else()
      set(ODE_BUILD_SHARED ON CACHE BOOL "")
    endif()
    add_subdirectory(deps/ode ode)
    if(NOT WIN32)
      set_target_properties(ode PROPERTIES COMPILE_FLAGS "-Wno-unused-volatile-lvalue -Wno-array-bounds -Wno-undefined-var-template")
    endif()
    include_directories(deps/ode/include "${CMAKE_CURRENT_BINARY_DIR}/ode/include")
    set(LOVR_ODE ode)
  endif()
endif()

# OpenGL
if(NOT (WIN32 OR EMSCRIPTEN OR ANDROID))
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_OPENGL ${OPENGL_LIBRARIES})
endif()

# OpenVR
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OPENVR)
  set(BUILD_SHARED ON CACHE BOOL "")
  set(BUILD_UNIVERSAL OFF CACHE BOOL "")
  include_directories(deps/openvr/headers)
  include_directories(deps/openvr/src)
  include_directories(deps/openvr/src/vrcommon)
  if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LOVR_OPENVR openvr_api64)
  else()
    set(LOVR_OPENVR openvr_api)
  endif()
  add_subdirectory(deps/openvr)
  set_target_properties(${LOVR_OPENVR} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openvr_api"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openvr_api"
  )
endif()

# OpenXR
# Currently, to use OpenXR, add the OpenXR SDK to the deps folder:
#   git submodule add https://github.com/khronosgroup/openxr-sdk deps/openxr
# On Android, download the Oculus OpenXR loader and place the "OpenXR" folder at "deps/OpenXR-Oculus"
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OPENXR)
  include_directories(deps/openxr/include)
  if(ANDROID)
    set(LOVR_OPENXR_OCULUS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/OpenXR-Oculus" CACHE STRING "The path to the Oculus OpenXR loader")
    add_library(openxr_loader SHARED IMPORTED)
    include_directories("${LOVR_OPENXR_OCULUS_PATH}/Include")
    set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${LOVR_OPENXR_OCULUS_PATH}/Libs/Android/${ANDROID_ABI}/Release/libopenxr_loader.so")
    set(LOVR_OPENXR openxr_loader)
  else()
    if(LOVR_SYSTEM_OPENXR)
      pkg_search_module(OPENXR openxr)
      if(NOT OPENXR_FOUND)
        message(FATAL_ERROR "OpenXR not found.")
      endif()
      include_directories(${OPENXR_INCLUDE_DIRS})
      set(LOVR_OPENXR ${OPENXR_LIBRARIES})
    else()
      add_subdirectory(deps/openxr openxr)
      set(LOVR_OPENXR openxr_loader)
    endif()
  endif()
endif()

# Oculus SDK -- expects Oculus SDK 1.26.0 or later
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OCULUS)
  if(NOT LOVR_OCULUS_PATH)
    message(FATAL_ERROR "LOVR_USE_OCULUS requires the LOVR_OCULUS_PATH to be set to the location of the Oculus Desktop SDK (LibOVR) folder")
  endif()
  set(OCULUS_BUILD_TYPE "Release")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OCULUS_ARCH "x64")
  else()
    set(OCULUS_ARCH "Win32")
  endif()
  include_directories("${LOVR_OCULUS_PATH}/LibOVR/Include")
  link_directories("${LOVR_OCULUS_PATH}/LibOVR/Lib/Windows/${OCULUS_ARCH}/${OCULUS_BUILD_TYPE}/VS2017")
  set(LOVR_OCULUS LibOVR)
endif()

# VrApi (Oculus Mobile SDK) -- tested on 1.34.0
if(LOVR_ENABLE_HEADSET AND LOVR_USE_VRAPI)
  set(LOVR_VRAPI_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/VrApi" CACHE STRING "The path to the VrApi folder of the Oculus Mobile SDK")
  add_library(VrApi SHARED IMPORTED)
  include_directories("${LOVR_VRAPI_PATH}/Include")
  set_target_properties(VrApi PROPERTIES IMPORTED_LOCATION "${LOVR_VRAPI_PATH}/Libs/Android/${ANDROID_ABI}/Release/libvrapi.so")
  set(LOVR_VRAPI VrApi)
endif()

# Pico Native SDK (1.3.3)
if(LOVR_ENABLE_HEADSET AND LOVR_USE_PICO)
  set(LOVR_PICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/pico" CACHE STRING "The path to the Pico SDK folder (unzipped aar)")
  add_library(Pvr_NativeSDK SHARED IMPORTED)
  set_target_properties(Pvr_NativeSDK PROPERTIES IMPORTED_LOCATION "${LOVR_PICO_PATH}/jni/${ANDROID_ABI}/libPvr_NativeSDK.so")
  set(LOVR_PICO Pvr_NativeSDK)
endif()

# pthreads
if(LOVR_ENABLE_THREAD)
  if(NOT WIN32 AND NOT EMSCRIPTEN)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    set(LOVR_PTHREADS Threads::Threads)
  endif()
  if (LOVR_USE_THREADLOCAL)
    add_definitions(-DUSE_LOVR_STBI_THREADLOCAL)
  endif()
endif()

# LÖVR

# Resources
file(GLOB LOVR_RESOURCES "src/resources/*.ttf" "src/resources/*.json" "src/resources/*.lua")
foreach(path ${LOVR_RESOURCES})

  # Turn the absolute path into a C variable like src_resources_boot_lua
  file(RELATIVE_PATH identifier ${CMAKE_CURRENT_SOURCE_DIR} ${path})
  string(MAKE_C_IDENTIFIER ${identifier} identifier)

  # Read the file and turn the bytes into comma-separated hex literals
  file(READ ${path} data HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," data ${data})

  # Generate the output filename by adding .h to the input filename
  string(CONCAT output ${path} ".h")

  # Write some xxd-compatible C code!
  file(WRITE ${output} "const unsigned char ${identifier}[] = {${data}};\nconst unsigned int ${identifier}_len = sizeof(${identifier});\n")
endforeach()

set(LOVR_SRC
  src/main.c
  src/core/arr.c
  src/core/fs.c
  src/core/map.c
  src/core/ref.c
  src/core/utf.c
  src/core/util.c
  src/core/zip.c
  src/api/api.c
  src/api/l_lovr.c
)

if(LOVR_USE_PICO)
  list(REMOVE_ITEM LOVR_SRC src/main.c)
endif()

if(LOVR_BUILD_SHARED)
  add_library(lovr SHARED ${LOVR_SRC})
elseif(LOVR_BUILD_EXE)
  add_executable(lovr ${LOVR_SRC})
else()
  return()
endif()

set_target_properties(lovr PROPERTIES C_VISIBILITY_PRESET default)
set_target_properties(lovr PROPERTIES C_STANDARD 99)
target_include_directories(lovr PRIVATE src src/modules)
target_link_libraries(lovr
  ${LOVR_ENET}
  ${LOVR_GLFW}
  ${LOVR_LUA}
  ${LOVR_MSDF}
  ${LOVR_ODE}
  ${LOVR_OPENGL}
  ${LOVR_OPENVR}
  ${LOVR_OPENXR}
  ${LOVR_OCULUS}
  ${LOVR_VRAPI}
  ${LOVR_PICO}
  ${LOVR_PTHREADS}
  ${LOVR_EMSCRIPTEN_FLAGS}
)

if(LOVR_ENABLE_AUDIO)
  add_definitions(-DLOVR_ENABLE_AUDIO)
  target_sources(lovr PRIVATE
    src/modules/audio/audio.c
    src/modules/audio/spatializers/dummy_spatializer.c
    src/api/l_audio.c
    src/api/l_audio_source.c
    src/lib/miniaudio/miniaudio.c
  )

  if(LOVR_USE_OCULUS_AUDIO)
    if(NOT LOVR_OCULUS_AUDIO_PATH)
      message(FATAL_ERROR "LOVR_USE_OCULUS_AUDIO requires the LOVR_OCULUS_AUDIO_PATH to be set to the location of the Oculus Spatializer Native (AudioSDK) folder")
    endif()

    include_directories("${LOVR_OCULUS_AUDIO_PATH}/Include")

    add_library(OculusAudio SHARED IMPORTED)
    if (ANDROID)
      set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/Android/arm64-v8a/libovraudio64.so")
    elseif(WIN32) # Note: This has *not* been tested.
      if (CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        set_target_properties(OculusAudio PROPERTIES IMPORTED_IMPLIB "${LOVR_OCULUS_AUDIO_PATH}/Lib/x64/ovraudio64.lib")
        set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/x64/ovraudio64.dll")
      else () # ARM and ARM64 are also valid values for CMAKE_GENERATOR_PLATFORM but we will ignore this
        set_target_properties(OculusAudio PROPERTIES IMPORTED_IMPLIB "${LOVR_OCULUS_AUDIO_PATH}/Lib/Win32/ovraudio32.lib")
        set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/Win32/ovraudio32.dll")
      endif()
    elseif (APPLE) # This *might* be possible-- there is a .framework in "macub/" ("universal binary"?)
      message(FATAL_ERROR "LOVR_USE_OCULUS_AUDIO is not currently supported on Apple platforms")
    else() # Assume Linux. Note: This has *not* been tested. FIXME: When is the .so copied?
      set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/Linux64/libovraudio64.so")
    endif()

    set(LOVR_OCULUS_AUDIO OculusAudio)
    add_definitions(-DLOVR_ENABLE_OCULUS_AUDIO)
    target_sources(lovr PRIVATE src/modules/audio/spatializers/oculus_spatializer.c)
    target_link_libraries(lovr ${LOVR_OCULUS_AUDIO}) # TODO: Reorder?
  endif()
endif()

if(LOVR_ENABLE_DATA)
  add_definitions(-DLOVR_ENABLE_DATA)
  target_sources(lovr PRIVATE
    src/modules/data/blob.c
    src/modules/data/modelData.c
    src/modules/data/modelData_gltf.c
    src/modules/data/modelData_obj.c
    src/modules/data/rasterizer.c
    src/modules/data/soundData.c
    src/modules/data/textureData.c
    src/api/l_data.c
    src/api/l_data_blob.c
    src/api/l_data_modelData.c
    src/api/l_data_rasterizer.c
    src/api/l_data_soundData.c
    src/api/l_data_textureData.c
    src/lib/stb/stb_image.c
    src/lib/stb/stb_truetype.c
    src/lib/stb/stb_vorbis.c
    src/lib/jsmn/jsmn.c
  )
endif()

if(LOVR_ENABLE_EVENT)
  add_definitions(-DLOVR_ENABLE_EVENT)
  target_sources(lovr PRIVATE
    src/modules/event/event.c
    src/api/l_event.c
  )
endif()

if(LOVR_ENABLE_FILESYSTEM)
  add_definitions(-DLOVR_ENABLE_FILESYSTEM)
  target_sources(lovr PRIVATE
    src/modules/filesystem/filesystem.c
    src/modules/filesystem/file.c
    src/api/l_filesystem.c
  )
endif()

if(LOVR_ENABLE_GRAPHICS)
  add_definitions(-DLOVR_ENABLE_GRAPHICS)
  target_sources(lovr PRIVATE
    src/modules/graphics/font.c
    src/modules/graphics/graphics.c
    src/modules/graphics/material.c
    src/modules/graphics/model.c
    src/modules/graphics/opengl.c
    src/api/l_graphics.c
    src/api/l_graphics_canvas.c
    src/api/l_graphics_font.c
    src/api/l_graphics_material.c
    src/api/l_graphics_mesh.c
    src/api/l_graphics_model.c
    src/api/l_graphics_shader.c
    src/api/l_graphics_shaderBlock.c
    src/api/l_graphics_texture.c
    src/resources/shaders.c
    src/lib/glad/glad.c
  )
endif()

if(LOVR_ENABLE_HEADSET)
  add_definitions(-DLOVR_ENABLE_HEADSET)
  target_sources(lovr PRIVATE
    src/api/l_headset.c
    src/modules/headset/headset.c
  )

  if(LOVR_USE_OPENVR)
    add_definitions(-DLOVR_USE_OPENVR)
    target_sources(lovr PRIVATE src/modules/headset/headset_openvr.c)
    if(UNIX)
      target_compile_options(lovr PRIVATE -Wno-typedef-redefinition)
    endif()
  endif()
  if(LOVR_USE_OPENXR)
    add_definitions(-DLOVR_USE_OPENXR)
    target_sources(lovr PRIVATE src/modules/headset/headset_openxr.c)
  endif()
  if(LOVR_USE_OCULUS)
    add_definitions(-DLOVR_USE_OCULUS)
    target_sources(lovr PRIVATE src/modules/headset/headset_oculus.c)
  endif()
  if(LOVR_USE_VRAPI)
    add_definitions(-DLOVR_USE_VRAPI)
    target_sources(lovr PRIVATE src/modules/headset/headset_vrapi.c)
  endif()
  if(LOVR_USE_PICO)
    add_definitions(-DLOVR_USE_PICO)
    target_sources(lovr PRIVATE src/modules/headset/headset_pico.c)
  endif()
  if(LOVR_USE_WEBXR)
    add_definitions(-DLOVR_USE_WEBXR)
    target_sources(lovr PRIVATE src/modules/headset/headset_webxr.c)
  endif()
  if(LOVR_USE_DESKTOP_HEADSET)
    add_definitions(-DLOVR_USE_DESKTOP_HEADSET)
    target_sources(lovr PRIVATE src/modules/headset/headset_desktop.c)
  endif()
endif()

if(LOVR_ENABLE_MATH)
  add_definitions(-DLOVR_ENABLE_MATH)
  target_sources(lovr PRIVATE
    src/modules/math/math.c
    src/modules/math/curve.c
    src/modules/math/pool.c
    src/modules/math/randomGenerator.c
    src/api/l_math.c
    src/api/l_math_curve.c
    src/api/l_math_randomGenerator.c
    src/api/l_math_vectors.c
    src/lib/noise1234/noise1234.c
  )
endif()

if(LOVR_ENABLE_PHYSICS)
  add_definitions(-DLOVR_ENABLE_PHYSICS)
  target_sources(lovr PRIVATE
    src/modules/physics/physics.c
    src/api/l_physics.c
    src/api/l_physics_collider.c
    src/api/l_physics_joints.c
    src/api/l_physics_shapes.c
    src/api/l_physics_world.c
  )
endif()

if(LOVR_ENABLE_THREAD)
  add_definitions(-DLOVR_ENABLE_THREAD)
  target_sources(lovr PRIVATE
    src/modules/thread/channel.c
    src/modules/thread/thread.c
    src/api/l_thread.c
    src/api/l_thread_channel.c
    src/api/l_thread_thread.c
    src/lib/tinycthread/tinycthread.c
  )
endif()

if(LOVR_ENABLE_TIMER)
  add_definitions(-DLOVR_ENABLE_TIMER)
  target_sources(lovr PRIVATE src/modules/timer/timer.c src/api/l_timer.c)
endif()

if(LOVR_ENABLE_ENET)
  add_definitions(-DLOVR_ENABLE_ENET)
  target_sources(lovr PRIVATE src/lib/lua-enet/enet.c)
endif()

if(LOVR_ENABLE_JSON)
  add_definitions(-DLOVR_ENABLE_JSON)
  target_sources(lovr PRIVATE
    src/lib/lua-cjson/fpconv.c
    src/lib/lua-cjson/lua_cjson.c
    src/lib/lua-cjson/strbuf.c
  )
endif()

# Platforms
if(WIN32)
  target_sources(lovr PRIVATE src/core/os_win32.c)
  set_target_properties(lovr PROPERTIES COMPILE_FLAGS "/wd4244 /MP")
  set_target_properties(lovr PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
  set_target_properties(lovr PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
  target_compile_definitions(lovr PUBLIC -D_CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(lovr PUBLIC -D_CRT_NONSTDC_NO_WARNINGS)

  if(MSVC_VERSION VERSION_LESS 1900)
    target_compile_definitions(lovr PUBLIC -Dinline=__inline -Dsnprintf=_snprintf)
  endif()

  function(move_dll)
    if(TARGET ${ARGV0})
      add_custom_command(TARGET lovr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${ARGV0}>
        $<TARGET_FILE_DIR:lovr>/$<TARGET_FILE_NAME:${ARGV0}>
      )
    endif()
  endfunction()

  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_ODE})
  move_dll(${LOVR_OPENVR})
  move_dll(${LOVR_MSDF})
  move_dll(${LOVR_OCULUS_AUDIO})
  target_compile_definitions(lovr PRIVATE -DLOVR_GL)
elseif(APPLE)
  target_link_libraries(lovr objc)
  target_sources(lovr PRIVATE src/core/os_macos.c)
  target_compile_definitions(lovr PRIVATE -DLOVR_GL)
  if(LOVR_BUILD_BUNDLE)
    function(move_lib)
      if(TARGET ${ARGV0})
        add_custom_command(TARGET lovr POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_SONAME_FILE:${ARGV0}>
          ${CMAKE_CURRENT_BINARY_DIR}/lovr.app/Contents/MacOS/$<TARGET_SONAME_FILE_NAME:${ARGV0}> # Bad
        )
      endif()
    endfunction()

    move_lib(${LOVR_GLFW})
    move_lib(${LOVR_LUA})
    move_lib(${LOVR_ODE})
    move_lib(${LOVR_OPENVR})
    move_lib(${LOVR_MSDF})

    target_sources(lovr PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lovr.icns")
    set_target_properties(lovr PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_RPATH TRUE
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "@executable_path"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/Info.plist"
      RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lovr.icns"
    )
  endif()
elseif(EMSCRIPTEN)
  target_sources(lovr PRIVATE src/core/os_web.c)
  target_compile_definitions(lovr PRIVATE -DLOVR_WEBGL)
elseif(ANDROID)
  target_link_libraries(lovr log EGL GLESv3 OpenSLES android)
  target_compile_definitions(lovr PRIVATE -DLOVR_GLES)
  target_include_directories(lovr PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")

  # Dynamically linked targets output libraries in lib/<ABI> for easy including in apk with aapt
  set_target_properties(
    lovr ${LOVR_ODE}
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/${ANDROID_ABI}"
  )

  if(LOVR_BUILD_EXE)
    set(ANDROID_JAR "${ANDROID_SDK}/platforms/${ANDROID_PLATFORM}/android.jar")
    set(ANDROID_TOOLS "${ANDROID_SDK}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}")

    # If assets are included in the apk then add '-A assets' to aapt, otherwise don't add any flags
    if(ANDROID_ASSETS)
      set(ANDROID_ASSETS -A ${ANDROID_ASSETS})
    endif()

    # Flavor-specific config:
    # - Imported targets need to have their libraries manually copied to lib/<ABI>
    # - Figure out which Java class (Activity) and AndroidManifest.xml to use
    # - Oculus uses the regular android os layer, pico implements its own in the headset backend
    # - Some of the Pico SDK is in a jar that has to be added to the classpath and dx invocation
    # TODO error (probably way earlier) if no headset API is defined, since everything will break
    if(LOVR_USE_OPENXR)
      set(MANIFEST "oculus")
      set(ACTIVITY "openxr")
      target_sources(lovr PRIVATE src/core/os_android.c)
      get_target_property(OPENXR_LIB ${LOVR_OPENXR} IMPORTED_LOCATION)
      file(COPY ${OPENXR_LIB} DESTINATION lib/${ANDROID_ABI})
      set(ANDROID_CLASSPATH "${ANDROID_JAR}")
    elseif(LOVR_USE_VRAPI)
      set(MANIFEST "oculus")
      set(ACTIVITY "vrapi")
      target_sources(lovr PRIVATE src/core/os_android.c)
      get_target_property(VRAPI_LIB ${LOVR_VRAPI} IMPORTED_LOCATION)
      file(COPY ${VRAPI_LIB} DESTINATION lib/${ANDROID_ABI})
      set(ANDROID_CLASSPATH "${ANDROID_JAR}")
    elseif(LOVR_USE_PICO)
      set(MANIFEST "pico")
      set(ACTIVITY "pico")
      get_target_property(PICO_LIB ${LOVR_PICO} IMPORTED_LOCATION)
      file(COPY ${PICO_LIB} DESTINATION lib/${ANDROID_ABI})
      set(EXTRA_JAR "${LOVR_PICO_PATH}/classes.jar")
      if(WIN32)
        set(ANDROID_CLASSPATH "${ANDROID_JAR};${EXTRA_JAR}")
      else()
        set(ANDROID_CLASSPATH "${ANDROID_JAR}:${EXTRA_JAR}")
      endif()
    endif()

    set(ANDROID_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/AndroidManifest_${MANIFEST}.xml" CACHE STRING "The AndroidManifest.xml file to use")

    if (ANDROID_KEYSTORE_PASS) # Trick so that --ks-pass is not passed if no password is given.
      set(ANDROID_APKSIGNER_KEYSTORE_PASS --ks-pass)
    endif()

    if (ANDROID_EXTRAPACKAGES) # Only pass --extra-packages if var is set
      set(ANDROID_EXTRAPACKAGES --extra-packages ${ANDROID_EXTRAPACKAGES})
    endif()

    # Make an apk
    add_custom_target(
      buildAPK ALL
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy "${ANDROID_MANIFEST}" AndroidManifest.xml
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/Activity_${ACTIVITY}.java Activity.java
      COMMAND ${Java_JAVAC_EXECUTABLE} -classpath "${ANDROID_CLASSPATH}" -d . Activity.java
      COMMAND ${ANDROID_TOOLS}/dx --dex --output classes.dex ${EXTRA_JAR} org/lovr/app/Activity.class
      COMMAND
        ${ANDROID_TOOLS}/aapt
        package -f
        -M AndroidManifest.xml
        -I ${ANDROID_JAR}
        -F lovr.unaligned.apk
        ${ANDROID_EXTRAPACKAGES}
        ${ANDROID_ASSETS}
      COMMAND
        ${ANDROID_TOOLS}/aapt
        add -f lovr.unaligned.apk
        classes.dex lib/${ANDROID_ABI}/*.so
      COMMAND ${ANDROID_TOOLS}/zipalign -f 4 lovr.unaligned.apk lovr.unsigned.apk
      COMMAND ${ANDROID_TOOLS}/apksigner
        sign
        --ks ${ANDROID_KEYSTORE}
        ${ANDROID_APKSIGNER_KEYSTORE_PASS} ${ANDROID_KEYSTORE_PASS}
        --in lovr.unsigned.apk
        --out lovr.apk
      COMMAND ${CMAKE_COMMAND} -E remove lovr.unaligned.apk lovr.unsigned.apk AndroidManifest.xml Activity.java classes.dex
      COMMAND ${CMAKE_COMMAND} -E remove_directory org
    )
  endif()
elseif(UNIX)
  if(LOVR_USE_LINUX_EGL)
    target_sources(lovr PRIVATE src/core/os_linux.c)
    target_compile_definitions(lovr PRIVATE -DLOVR_LINUX_EGL)
  else()
    target_sources(lovr PRIVATE src/core/os_linux.c)
    target_compile_definitions(lovr PRIVATE -DLOVR_LINUX_X11)
  endif()
  target_compile_definitions(lovr PRIVATE -DLOVR_GL)
endif()
