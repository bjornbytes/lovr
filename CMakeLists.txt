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
option(LOVR_ENABLE_SYSTEM "Enable the system module" ON)
option(LOVR_ENABLE_THREAD "Enable the thread module" ON)
option(LOVR_ENABLE_TIMER "Enable the timer module" ON)

option(LOVR_USE_LUAJIT "Use LuaJIT instead of Lua" ON)
option(LOVR_USE_GLSLANG "Use glslang to compile GLSL shaders" ON)
option(LOVR_USE_VULKAN "Use the Vulkan renderer" ON)
option(LOVR_USE_WEBGPU "Use the WebGPU renderer" OFF)
option(LOVR_USE_OPENXR "Enable the OpenXR backend for the headset module" ON)
option(LOVR_USE_WEBXR "Enable the WebXR backend for the headset module" OFF)
option(LOVR_USE_DESKTOP "Enable the keyboard/mouse backend for the headset module" ON)
option(LOVR_USE_STEAM_AUDIO "Enable the Steam Audio spatializer (be sure to also set LOVR_STEAM_AUDIO_PATH)" OFF)
option(LOVR_USE_OCULUS_AUDIO "Enable the Oculus Audio spatializer (be sure to also set LOVR_OCULUS_AUDIO_PATH)" OFF)
option(LOVR_USE_LINUX_EGL "Use the EGL graphics extension on Linux" OFF)
option(LOVR_SANITIZE "Enable Address Sanitizer" OFF)

option(LOVR_SYSTEM_GLFW "Use the system-provided glfw" OFF)
option(LOVR_SYSTEM_LUA "Use the system-provided Lua" OFF)
option(LOVR_SYSTEM_ODE "Use the system-provided ODE" OFF)
option(LOVR_SYSTEM_OPENXR "Use the system-provided OpenXR" OFF)

option(LOVR_BUILD_EXE "Build an executable (or an apk on Android)" ON)
option(LOVR_BUILD_SHARED "Build a shared library (takes precedence over LOVR_BUILD_EXE)" OFF)
option(LOVR_BUILD_BUNDLE "On macOS, build a .app bundle instead of a raw program" OFF)

set(LOVR_SYMBOL_VISIBILITY "hidden" CACHE STRING "What should the C symbol visibility be? hidden reduces binary size, default lets other binaries use this one.")

# Setup
if(EMSCRIPTEN)
  string(CONCAT EMSCRIPTEN_LINKER_FLAGS
    "-Os "
    "-s USE_WEBGPU=1 "
    "-s FORCE_FILESYSTEM=1 "
    "-s EXPORTED_FUNCTIONS=_main,_lovrDestroy,_webxr_attach,_webxr_detach "
    "-s \"EXPORTED_RUNTIME_METHODS=['getValue','setValue']\" "
    "--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/etc/webxr.js\" "
    "--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/etc/lovr.html\""
  )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os")
  if(LOVR_ENABLE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  endif()
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(LOVR_USE_WEBXR ON)
  set(LOVR_USE_OPENXR OFF)
  set(LOVR_USE_WEBGPU ON)
  set(LOVR_USE_VULKAN OFF)
elseif(ANDROID)
  set(ANDROID_FLAVOR "oculus" CACHE STRING "Which Android flavor to build (oculus or pico)")
  find_package(Java REQUIRED)
  set(LOVR_USE_DESKTOP OFF)
  if(LOVR_BUILD_EXE)
    set(LOVR_BUILD_SHARED ON)
  endif()
elseif(UNIX)
  find_package(PkgConfig)
  if(NOT APPLE)
    set(CMAKE_SKIP_RPATH OFF)
  endif()
endif()

if(NOT ANDROID AND LOVR_BUILD_SHARED)
  set(LOVR_BUILD_EXE OFF)
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
    set(GLFW_INSTALL OFF CACHE BOOL "")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "")
    add_subdirectory(deps/glfw glfw)
    include_directories(deps/glfw/include)
    set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
  endif()
  unset(LIB_SUFFIX CACHE)
endif()

# Lua
if(LOVR_USE_LUAJIT AND NOT EMSCRIPTEN)
  # Required to make luajit compile without breaking linker cache on macOS Big Sur. Also, is a better,
  # newer GC so enable it on all builds.
  set(LUAJIT_ENABLE_GC64 ON)

  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUAJIT REQUIRED luajit)
    include_directories(${LUAJIT_INCLUDE_DIRS})
    set(LOVR_LUA ${LUAJIT_LIBRARIES})
  else()
    add_subdirectory(deps/luajit luajit)
    set_target_properties(luajit PROPERTIES EXCLUDE_FROM_ALL 1)
    include_directories(deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
    set(LOVR_LUA libluajit)
  endif()
else()
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUA REQUIRED lua)
    include_directories(${LUA_INCLUDE_DIRS})
    set(LOVR_LUA ${LUA_LIBRARIES})
  else()
    set(LUA_SRC
      lapi.c lauxlib.c lbaselib.c lcode.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c
      liolib.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c
      lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c lvm.c lzio.c
    )
    list(TRANSFORM LUA_SRC PREPEND deps/lua/)
    add_library(lua SHARED ${LUA_SRC})
    target_link_libraries(lua m)
    target_link_libraries(lua dl)
    target_compile_definitions(lua PRIVATE -DLUA_USE_DLOPEN)
    include_directories(deps/lua)
    set(LOVR_LUA lua)
  endif()
endif()

# MSDF
if(LOVR_ENABLE_DATA)
  set(BUILD_SHARED_LIBS ON)
  add_subdirectory(deps/msdfgen msdfgen)
  include_directories(deps/msdfgen)
  set(LOVR_MSDF msdfgen)
  if(APPLE)
    set_target_properties(msdfgen PROPERTIES MACOSX_RPATH ON)
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
    if(MSVC)
      set_target_properties(ode PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267")
      target_compile_definitions(ode PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
      set_target_properties(ode PROPERTIES COMPILE_FLAGS "-Wno-unused-volatile-lvalue -Wno-array-bounds -Wno-undefined-var-template")
    endif()
    include_directories(deps/ode/include "${CMAKE_CURRENT_BINARY_DIR}/ode/include")
    set(LOVR_ODE ode)
  endif()
endif()

# glslang
if(LOVR_USE_GLSLANG)
  set(ENABLE_HLSL OFF CACHE BOOL "")
  set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
  set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
  set(ENABLE_GLSLANG_INSTALL OFF CACHE BOOL "")
  set(ENABLE_OPT OFF CACHE BOOL "")
  set(ENABLE_CTEST OFF CACHE BOOL "")
  set(BUILD_EXTERNAL OFF CACHE BOOL "")
  set(BUILD_SHARED_LIBS OFF)
  include_directories(deps/glslang/glslang/Include deps/glslang/StandAlone)
  add_subdirectory(deps/glslang glslang)
  target_sources(glslang PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/glslang/StandAlone/ResourceLimits.cpp")
  target_sources(glslang PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps/glslang/StandAlone/resource_limits_c.cpp")
  set(LOVR_GLSLANG glslang SPIRV)
endif()

# Vulkan
if(LOVR_USE_VULKAN)
  find_package(Vulkan REQUIRED)
  include_directories(${Vulkan_INCLUDE_DIRS})
endif()

# OpenXR
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OPENXR)
  include_directories(deps/openxr/include)
  if(ANDROID)
    add_library(openxr_loader SHARED IMPORTED)
    set(LOVR_OPENXR openxr_loader)
    if(ANDROID_FLAVOR STREQUAL "oculus")
      set(LOVR_OPENXR_OCULUS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/oculus-openxr" CACHE STRING "The path to the Oculus OpenXR loader")
      set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${LOVR_OPENXR_OCULUS_PATH}/Libs/Android/${ANDROID_ABI}/Release/libopenxr_loader.so")
    elseif(ANDROID_FLAVOR STREQUAL "pico")
      set(LOVR_OPENXR_PICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/pico-openxr" CACHE STRING "The path to the Pico OpenXR loader")
      set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${LOVR_OPENXR_PICO_PATH}/Libs/Android/${ANDROID_ABI}/libopenxr_loader.so")
    else()
      message(FATAL_ERROR "Unsupported Android flavor (expected: oculus or pico)")
    endif()
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

# pthreads
if(LOVR_ENABLE_THREAD AND NOT (WIN32 OR EMSCRIPTEN))
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set(LOVR_PTHREADS Threads::Threads)
endif()

# Steam Audio (aka Phonon)
if(LOVR_USE_STEAM_AUDIO)
  if(NOT LOVR_STEAM_AUDIO_PATH)
    message(FATAL_ERROR "LOVR_USE_STEAM_AUDIO requires the LOVR_STEAM_AUDIO_PATH to be set to the location of the Steam Audio folder")
  endif()
  if(CMAKE_SIZEOF_VOID_P EQUAL 4) # Note: SteamAudio distributes 32 bit libs but wiring them up is very verbose
    message(FATAL_ERROR "LOVR_USE_STEAM_AUDIO is not currently available on 32 bit builds")
  endif()
  include_directories("${LOVR_STEAM_AUDIO_PATH}/include")
  add_library(Phonon SHARED IMPORTED)
  if(ANDROID)
    set_target_properties(Phonon PROPERTIES IMPORTED_LOCATION "${LOVR_STEAM_AUDIO_PATH}/lib/Android/arm64/libphonon.so")
  elseif(WIN32)
    set_target_properties(Phonon PROPERTIES IMPORTED_IMPLIB "${LOVR_STEAM_AUDIO_PATH}/lib/Windows/x64/phonon.lib")
    set_target_properties(Phonon PROPERTIES IMPORTED_LOCATION "${LOVR_STEAM_AUDIO_PATH}/bin/Windows/x64/phonon.dll")
  elseif(APPLE)
    set_target_properties(Phonon PROPERTIES IMPORTED_LOCATION "${LOVR_STEAM_AUDIO_PATH}/lib/OSX/libphonon.dylib"
                                            IMPORTED_SONAME "@rpath/libphonon.dylib")  # It doesn't make sense this line is required, but it is
  else() # Assume Linux. Note: This has *not* been tested. FIXME: When is the .so copied?
    set_target_properties(Phonon PROPERTIES IMPORTED_LOCATION "${LOVR_STEAM_AUDIO_PATH}/lib/Linux/x64/libphonon.so")
  endif()

  set(LOVR_PHONON Phonon)
endif()

# Oculus Audio
if(LOVR_USE_OCULUS_AUDIO)
  if(NOT LOVR_OCULUS_AUDIO_PATH)
    message(FATAL_ERROR "LOVR_USE_OCULUS_AUDIO requires the LOVR_OCULUS_AUDIO_PATH to be set to the location of the Oculus Spatializer Native (AudioSDK) folder")
  endif()

  include_directories("${LOVR_OCULUS_AUDIO_PATH}/Include")

  add_library(OculusAudio SHARED IMPORTED)
  if(ANDROID)
    set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/Android/arm64-v8a/libovraudio64.so")
  elseif(WIN32) # Note: This has *not* been tested.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set_target_properties(OculusAudio PROPERTIES IMPORTED_IMPLIB "${LOVR_OCULUS_AUDIO_PATH}/Lib/x64/ovraudio64.lib")
      set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/x64/ovraudio64.dll")
    else () # ARM and ARM64 are also valid values for CMAKE_GENERATOR_PLATFORM but we will ignore this
      set_target_properties(OculusAudio PROPERTIES IMPORTED_IMPLIB "${LOVR_OCULUS_AUDIO_PATH}/Lib/Win32/ovraudio32.lib")
      set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/Win32/ovraudio32.dll")
    endif()
  elseif(APPLE)
    # Oculus Audio ships with an intel .framework, but (as of 25.0.0) it is broken. We can convert it to a working dylib with install_name_tool in a custom target:
    SET(OCULUS_AUDIO_MAC_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/ovraudio")
    SET(OCULUS_AUDIO_MAC_LIB_FILE "${OCULUS_AUDIO_MAC_LIB_DIR}/ovraudio64.dylib")
    SET(OCULUS_AUDIO_MAC_RPATH "@rpath/ovraudio64.dylib")
    file(MAKE_DIRECTORY "${OCULUS_AUDIO_MAC_LIB_DIR}")
    add_custom_command(OUTPUT "${OCULUS_AUDIO_MAC_LIB_FILE}"
                   COMMAND ${CMAKE_COMMAND} -E copy "${LOVR_OCULUS_AUDIO_PATH}/Lib/mac64/OVRAudio64.framework/Versions/A/ovraudio64" "${OCULUS_AUDIO_MAC_LIB_FILE}"
                   COMMAND install_name_tool -id ${OCULUS_AUDIO_MAC_RPATH} "${OCULUS_AUDIO_MAC_LIB_FILE}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    add_custom_target(OculusAudioFixed DEPENDS "${OCULUS_AUDIO_MAC_LIB_FILE}")
    add_dependencies(OculusAudio OculusAudioFixed)
    # IMPORTED_SONAME required because the automatic rpath requires the dylib file to exist before builds start
    set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${OCULUS_AUDIO_MAC_LIB_FILE}"
                                                 IMPORTED_SONAME "${OCULUS_AUDIO_MAC_RPATH}")
  else() # Assume Linux. Note: This has *not* been tested. FIXME: When is the .so copied?
    set_target_properties(OculusAudio PROPERTIES IMPORTED_LOCATION "${LOVR_OCULUS_AUDIO_PATH}/Lib/Linux64/libovraudio64.so")
  endif()

  set(LOVR_OCULUS_AUDIO OculusAudio)
endif()

# LÖVR

# Plugins
set(LOVR 1)
link_libraries(${LOVR_LUA})
file(GLOB LOVR_PLUGINS ${CMAKE_SOURCE_DIR}/plugins/*)
foreach(PLUGIN_PATH ${LOVR_PLUGINS})
  if(IS_DIRECTORY ${PLUGIN_PATH} AND EXISTS ${PLUGIN_PATH}/CMakeLists.txt)
    get_filename_component(PLUGIN "${PLUGIN_PATH}" NAME)
    add_subdirectory(${PLUGIN_PATH} "${CMAKE_CURRENT_BINARY_DIR}/plugins/${PLUGIN}")
    get_directory_property(PLUGIN_TARGETS DIRECTORY ${PLUGIN_PATH} DEFINITION LOVR_PLUGIN_TARGETS)
    if(NOT PLUGIN_TARGETS)
      get_directory_property(PLUGIN_TARGETS DIRECTORY ${PLUGIN_PATH} BUILDSYSTEM_TARGETS)
    endif()
    set_target_properties(${PLUGIN_TARGETS} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/${PLUGIN}")
    list(APPEND ALL_PLUGIN_TARGETS ${PLUGIN_TARGETS})
  endif()
endforeach()

set(LOVR_SRC
  src/core/fs.c
  src/core/zip.c
  src/api/api.c
  src/api/l_lovr.c
  src/util.c
)

if(LOVR_BUILD_EXE)
  list(APPEND LOVR_SRC src/main.c)
endif()

if(LOVR_BUILD_SHARED)
  add_library(lovr SHARED ${LOVR_SRC})
  target_compile_definitions(lovr PRIVATE LOVR_BUILDING_SHARED)
elseif(LOVR_BUILD_EXE)
  add_executable(lovr ${LOVR_SRC})
else()
  return()
endif()

if(NOT LOVR_BUILD_EXE)
  target_compile_definitions(lovr PUBLIC LOVR_OMIT_MAIN) # specifically for win32 WinMain
endif()

set_target_properties(lovr PROPERTIES C_VISIBILITY_PRESET ${LOVR_SYMBOL_VISIBILITY})
set_target_properties(lovr PROPERTIES C_STANDARD 11)
set_target_properties(lovr PROPERTIES C_STANDARD_REQUIRED ON)
target_include_directories(lovr PRIVATE src src/modules src/lib/stdatomic etc)
target_link_libraries(lovr
  ${LOVR_GLFW}
  ${LOVR_LUA}
  ${LOVR_MSDF}
  ${LOVR_ODE}
  ${LOVR_GLSLANG}
  ${LOVR_OPENXR}
  ${LOVR_OCULUS_AUDIO}
  ${LOVR_PTHREADS}
  ${EMSCRIPTEN_LINKER_FLAGS}
)

if (LOVR_SANITIZE)
  set(LOVR_SANITIZE_FLAGS "-fsanitize=address,undefined" "-O1" "-fno-omit-frame-pointer")
  target_compile_options(lovr PRIVATE ${LOVR_SANITIZE_FLAGS})
  target_link_options(lovr PRIVATE ${LOVR_SANITIZE_FLAGS})
endif()

if(LOVR_ENABLE_AUDIO OR LOVR_ENABLE_DATA)
  target_sources(lovr PRIVATE
    src/lib/miniaudio/miniaudio.c
  )
endif()

if(LOVR_ENABLE_AUDIO)
  target_sources(lovr PRIVATE
    src/modules/audio/audio.c
    src/modules/audio/spatializer_simple.c
    src/api/l_audio.c
    src/api/l_audio_source.c
  )

  if(LOVR_USE_STEAM_AUDIO)
    target_compile_definitions(lovr PRIVATE LOVR_ENABLE_PHONON_SPATIALIZER)
    target_sources(lovr PRIVATE src/modules/audio/spatializer_phonon.c)
    # Dynamically linked at runtime, so this is not otherwise a dependency
    add_dependencies(lovr ${LOVR_PHONON})
  endif()

  if(LOVR_USE_OCULUS_AUDIO)
    target_compile_definitions(lovr PRIVATE LOVR_ENABLE_OCULUS_SPATIALIZER)
    target_sources(lovr PRIVATE src/modules/audio/spatializer_oculus.c)
  endif()
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_AUDIO)
endif()

if(LOVR_ENABLE_DATA)
  target_sources(lovr PRIVATE
    src/modules/data/blob.c
    src/modules/data/image.c
    src/modules/data/modelData.c
    src/modules/data/modelData_gltf.c
    src/modules/data/modelData_obj.c
    src/modules/data/modelData_stl.c
    src/modules/data/rasterizer.c
    src/modules/data/sound.c
    src/api/l_data.c
    src/api/l_data_blob.c
    src/api/l_data_image.c
    src/api/l_data_modelData.c
    src/api/l_data_rasterizer.c
    src/api/l_data_sound.c
    src/lib/minimp3/minimp3.c
    src/lib/stb/stb_image.c
    src/lib/stb/stb_truetype.c
    src/lib/stb/stb_vorbis.c
    src/lib/jsmn/jsmn.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_DATA)
endif()

if(LOVR_ENABLE_EVENT)
  target_sources(lovr PRIVATE
    src/modules/event/event.c
    src/api/l_event.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_EVENT)
endif()

if(LOVR_ENABLE_FILESYSTEM)
  target_sources(lovr PRIVATE
    src/modules/filesystem/filesystem.c
    src/modules/filesystem/file.c
    src/api/l_filesystem.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_FILESYSTEM)
endif()

if(LOVR_ENABLE_GRAPHICS)
  target_sources(lovr PRIVATE
    src/core/spv.c
    src/modules/graphics/graphics.c
    src/api/l_graphics.c
    src/api/l_graphics_buffer.c
    src/api/l_graphics_texture.c
    src/api/l_graphics_sampler.c
    src/api/l_graphics_shader.c
    src/api/l_graphics_pass.c
  )

  if(LOVR_USE_GLSLANG)
    target_compile_definitions(lovr PRIVATE LOVR_USE_GLSLANG)
  endif()

  if(LOVR_USE_VULKAN)
    target_compile_definitions(lovr PRIVATE LOVR_VK)
    target_sources(lovr PRIVATE src/core/gpu_vk.c)
  endif()

  if(LOVR_USE_WEBGPU)
    target_compile_definitions(lovr PRIVATE LOVR_WGPU)
    target_sources(lovr PRIVATE src/core/gpu_webgpu.c)
  endif()

  add_custom_target(compile_shaders ALL)
  add_dependencies(lovr compile_shaders)

  function(compile_shaders)
    file(GLOB shader_files "etc/shaders/*.${ARGV0}")
    foreach(shader_file ${shader_files})
      string(REGEX MATCH "([^\/]+)\.${ARGV0}" shader ${shader_file})
      string(REPLACE ".${ARGV0}" "" shader ${shader})
      add_custom_command(TARGET compile_shaders POST_BUILD
        COMMAND
        glslangValidator
        --quiet
        --target-env vulkan1.1
        --vn lovr_shader_${shader}_${ARGV0}
        -o ${shader_file}.h
        ${shader_file}
      )
    endforeach()
  endfunction()

  compile_shaders("vert")
  compile_shaders("frag")
  compile_shaders("comp")
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_GRAPHICS)
endif()

if(LOVR_ENABLE_HEADSET)
  target_sources(lovr PRIVATE
    src/api/l_headset.c
    src/modules/headset/headset.c
  )

  if(LOVR_USE_OPENXR)
    target_compile_definitions(lovr PRIVATE LOVR_USE_OPENXR)
    target_sources(lovr PRIVATE src/modules/headset/headset_openxr.c)
  endif()
  if(LOVR_USE_WEBXR)
    target_compile_definitions(lovr PRIVATE LOVR_USE_WEBXR)
    target_sources(lovr PRIVATE src/modules/headset/headset_webxr.c)
  endif()
  if(LOVR_USE_DESKTOP)
    target_compile_definitions(lovr PRIVATE LOVR_USE_DESKTOP)
    target_sources(lovr PRIVATE src/modules/headset/headset_desktop.c)
  endif()
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_HEADSET)
endif()

if(LOVR_ENABLE_MATH)
  target_sources(lovr PRIVATE
    src/modules/math/math.c
    src/modules/math/curve.c
    src/modules/math/pool.c
    src/modules/math/randomGenerator.c
    src/api/l_math.c
    src/api/l_math_curve.c
    src/api/l_math_randomGenerator.c
    src/api/l_math_vectors.c
    src/lib/noise/simplexnoise1234.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_MATH)
endif()

if(LOVR_ENABLE_PHYSICS)
  target_sources(lovr PRIVATE
    src/modules/physics/physics.c
    src/api/l_physics.c
    src/api/l_physics_collider.c
    src/api/l_physics_joints.c
    src/api/l_physics_shapes.c
    src/api/l_physics_world.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_PHYSICS)
endif()

if(LOVR_ENABLE_SYSTEM)
  target_sources(lovr PRIVATE
    src/modules/system/system.c
    src/api/l_system.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_SYSTEM)
endif()

if(LOVR_ENABLE_THREAD)
  target_sources(lovr PRIVATE
    src/modules/thread/channel.c
    src/modules/thread/thread.c
    src/api/l_thread.c
    src/api/l_thread_channel.c
    src/api/l_thread_thread.c
    src/lib/tinycthread/tinycthread.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_THREAD)
endif()

if(LOVR_ENABLE_TIMER)
  target_sources(lovr PRIVATE src/modules/timer/timer.c src/api/l_timer.c)
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_TIMER)
endif()

# Resources
file(GLOB LOVR_RESOURCES "etc/*.ttf" "etc/*.lua")
foreach(path ${LOVR_RESOURCES})

  # Turn the absolute path into a C variable like etc_boot_lua
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

# Add a custom target that is always out of date so libraries are always moved
add_custom_target(move_libraries ALL)

# Platforms
if(WIN32)
  target_sources(lovr PRIVATE src/core/os_win32.c)
  target_sources(lovr PRIVATE etc/lovr.rc)
  if (MSVC)
    set_target_properties(lovr PROPERTIES COMPILE_FLAGS "/wd4244 /MP")
  else()
    set_target_properties(lovr PROPERTIES COMPILE_FLAGS "-MP")
  endif()
  if(NOT LOVR_BUILD_SHARED)
    set_target_properties(lovr PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:console /ENTRY:WinMainCRTStartup")
    set_target_properties(lovr PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:WinMainCRTStartup")
  endif()
  target_compile_definitions(lovr PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(lovr PRIVATE _CRT_NONSTDC_NO_WARNINGS)

  if(MSVC_VERSION VERSION_LESS 1900)
    target_compile_definitions(lovr PRIVATE inline=__inline snprintf=_snprintf)
  endif()

  function(move_dll)
    if(TARGET ${ARGV0})
      add_custom_command(TARGET move_libraries POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${ARGV0}>
        $<TARGET_FILE_DIR:lovr>/$<TARGET_FILE_NAME:${ARGV0}>
      )
    endif()
  endfunction()

  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_ODE})
  move_dll(${LOVR_MSDF})
  move_dll(${LOVR_OCULUS_AUDIO})
  move_dll(${LOVR_PHONON})
  foreach(target ${ALL_PLUGIN_TARGETS})
    move_dll(${target})
  endforeach()
elseif(APPLE)
  find_library(AVFOUNDATION AVFoundation)
  target_link_libraries(lovr objc ${AVFOUNDATION})
  target_sources(lovr PRIVATE src/core/os_macos.c)
  set_source_files_properties(src/core/os_macos.c PROPERTIES COMPILE_FLAGS -xobjective-c)
  set_target_properties(lovr PROPERTIES
    MACOSX_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path"
  )
  if(LOVR_BUILD_BUNDLE)
    set(EXE_DIR ${CMAKE_CURRENT_BINARY_DIR}/lovr.app/Contents/MacOS)
    target_sources(lovr PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/etc/lovr.icns")
    set_target_properties(lovr PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/etc/Info.plist"
      RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/etc/lovr.icns"
    )
  else()
    set(EXE_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set_target_properties(lovr PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  endif()
  function(move_lib)
    if(TARGET ${ARGV0})
      get_target_property(TARGET_TYPE ${ARGV0} TYPE)
      if(${TARGET_TYPE} STREQUAL "MODULE_LIBRARY")
        add_custom_command(TARGET move_libraries POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${ARGV0}>
          ${EXE_DIR}/$<TARGET_FILE_NAME:${ARGV0}>
        )
      else()
        add_custom_command(TARGET move_libraries POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_SONAME_FILE:${ARGV0}>
          ${EXE_DIR}/$<TARGET_SONAME_FILE_NAME:${ARGV0}>
        )
      endif()
    endif()
  endfunction()
  move_lib(${LOVR_GLFW})
  move_lib(${LOVR_LUA})
  move_lib(${LOVR_ODE})
  move_lib(${LOVR_MSDF})
  move_lib(${LOVR_OCULUS_AUDIO})
  move_lib(${LOVR_PHONON})
  foreach(target ${ALL_PLUGIN_TARGETS})
    move_lib(${target})
  endforeach()
elseif(EMSCRIPTEN)
  target_sources(lovr PRIVATE src/core/os_wasm.c)
  configure_file(etc/lovr.ico favicon.ico COPYONLY)
elseif(ANDROID)
  target_sources(lovr PRIVATE src/core/os_android.c)
  target_link_libraries(lovr log EGL android dl)
  target_include_directories(lovr PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")

  # Dynamically linked targets output libraries in raw/lib/<ABI> for easy including in apk with aapt
  set_target_properties(
    lovr
    ${LOVR_ODE}
    ${LOVR_MSDF}
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/raw/lib/${ANDROID_ABI}"
  )

  if(LOVR_BUILD_EXE)
    set(ANDROID_JAR "${ANDROID_SDK}/platforms/${ANDROID_PLATFORM}/android.jar")
    set(ANDROID_TOOLS "${ANDROID_SDK}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}")
    set(ANDROID_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/etc/AndroidManifest_${ANDROID_FLAVOR}.xml" CACHE STRING "The AndroidManifest.xml file to use")

    # If assets are included in the apk then add '-A assets' to aapt, otherwise don't add any flags
    if(ANDROID_ASSETS)
      set(ANDROID_ASSETS -A ${ANDROID_ASSETS})
    endif()

    if(ANDROID_PACKAGE)
      set(PACKAGE_RENAME "--rename-manifest-package" "${ANDROID_PACKAGE}")
    endif()

    if(LOVR_USE_OPENXR)
      get_target_property(OPENXR_LIB ${LOVR_OPENXR} IMPORTED_LOCATION)
      file(COPY ${OPENXR_LIB} DESTINATION raw/lib/${ANDROID_ABI})
    endif()

    if(LOVR_USE_OCULUS_AUDIO)
      get_target_property(OCULUS_AUDIO_LIB ${LOVR_OCULUS_AUDIO} IMPORTED_LOCATION)
      file(COPY ${OCULUS_AUDIO_LIB} DESTINATION raw/lib/${ANDROID_ABI})
    endif()

    if(LOVR_USE_STEAM_AUDIO)
      get_target_property(PHONON_LIB ${LOVR_PHONON} IMPORTED_LOCATION)
      file(COPY ${PHONON_LIB} DESTINATION raw/lib/${ANDROID_ABI})
    endif()

    # Make an apk
    add_custom_target(
      buildAPK ALL
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy "${ANDROID_MANIFEST}" AndroidManifest.xml
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/etc/Activity.java Activity.java
      COMMAND ${Java_JAVAC_EXECUTABLE} -classpath "${ANDROID_JAR}" -d . Activity.java
      COMMAND ${ANDROID_TOOLS}/d8 --min-api ${ANDROID_NATIVE_API_LEVEL} --output raw org/lovr/app/Activity.class
      COMMAND
        ${ANDROID_TOOLS}/aapt
        package -f
        ${PACKAGE_RENAME}
        -0 so
        -M AndroidManifest.xml
        -I ${ANDROID_JAR}
        -F lovr.unaligned.apk
        ${ANDROID_ASSETS}
        raw
      COMMAND ${ANDROID_TOOLS}/zipalign -f -p 4 lovr.unaligned.apk lovr.unsigned.apk
      COMMAND ${ANDROID_TOOLS}/apksigner
        sign
        --ks ${ANDROID_KEYSTORE}
        $<$<BOOL:${ANDROID_KEYSTORE_PASS}>:--ks-pass> $<$<BOOL:${ANDROID_KEYSTORE_PASS}>:${ANDROID_KEYSTORE_PASS}>
        $<$<BOOL:${ANDROID_KEY_PASS}>:--key-pass> $<$<BOOL:${ANDROID_KEY_PASS}>:${ANDROID_KEY_PASS}>
        --in lovr.unsigned.apk
        --out lovr.apk
      COMMAND ${CMAKE_COMMAND} -E remove lovr.unaligned.apk lovr.unsigned.apk AndroidManifest.xml Activity.java
      COMMAND ${CMAKE_COMMAND} -E remove_directory org
    )

    add_dependencies(buildAPK lovr)

    if(CMAKE_BUILD_TYPE STREQUAL Release)
      add_custom_target(
        strip ALL
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_STRIP} raw/lib/${ANDROID_ABI}/*.so
      )
      add_dependencies(strip lovr)
      add_dependencies(buildAPK strip)
    endif()

    # Copy plugin libraries to lib folder before packaging APK
    foreach(target ${ALL_PLUGIN_TARGETS})
      add_custom_command(TARGET move_libraries POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${target}>
          raw/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:${target}>
      )
    endforeach()
    add_dependencies(buildAPK move_libraries)
  endif()
elseif(UNIX)
  if(LOVR_USE_LINUX_EGL)
    target_compile_definitions(lovr PRIVATE LOVR_LINUX_EGL)
  else()
    target_compile_definitions(lovr PRIVATE LOVR_LINUX_X11)
  endif()
  target_sources(lovr PRIVATE src/core/os_linux.c)
  set_target_properties(lovr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "\$ORIGIN"
  )
  function(move_lib)
    if(TARGET ${ARGV0})
      get_target_property(TARGET_TYPE ${ARGV0} TYPE)
      if(${TARGET_TYPE} STREQUAL "MODULE_LIBRARY")
        add_custom_command(TARGET move_libraries POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${ARGV0}>
          ${CMAKE_BINARY_DIR}/bin/$<TARGET_FILE_NAME:${ARGV0}>
        )
      else()
        add_custom_command(TARGET move_libraries POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_SONAME_FILE:${ARGV0}>
          ${CMAKE_BINARY_DIR}/bin/$<TARGET_SONAME_FILE_NAME:${ARGV0}>
        )
      endif()
    endif()
  endfunction()
  function(move_app_image_file)
    set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/etc/${ARGV0}")
    set(DST "${CMAKE_BINARY_DIR}/bin/${ARGV0}")
    add_custom_command(TARGET lovr POST_BUILD
       DEPENDS ${SRC}
       COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
    )
  endfunction()
  move_lib(${LOVR_GLFW})
  move_lib(${LOVR_LUA})
  move_lib(${LOVR_ODE})
  move_lib(${LOVR_MSDF})
  move_lib(${LOVR_OPENXR})
  move_lib(${LOVR_OCULUS_AUDIO})
  foreach(target ${ALL_PLUGIN_TARGETS})
    move_lib(${target})
  endforeach()
  if(LOVR_BUILD_BUNDLE)
    move_app_image_file("lovr.desktop")
    move_app_image_file("AppRun")
    move_app_image_file("logo.svg")
  endif()
endif()
