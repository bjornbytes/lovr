cmake_minimum_required(VERSION 3.5.0)
cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")
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

option(LOVR_ENABLE_UTF8 "Enable the utf8 module" ON)

option(LOVR_USE_GLFW "Use GLFW for desktop windows" ON)
option(LOVR_USE_LUAJIT "Use LuaJIT instead of Lua" ON)
option(LOVR_USE_GLSLANG "Use glslang to compile GLSL shaders" ON)
option(LOVR_USE_VULKAN "Use the Vulkan renderer" ON)
option(LOVR_USE_WEBGPU "Use the WebGPU renderer" OFF)
option(LOVR_USE_OPENXR "Enable the OpenXR backend for the headset module" ON)
option(LOVR_USE_WEBXR "Enable the WebXR backend for the headset module" OFF)
option(LOVR_USE_SIMULATOR "Enable the keyboard/mouse backend for the headset module" ON)
option(LOVR_USE_STEAM_AUDIO "Enable the Steam Audio spatializer (be sure to also set LOVR_STEAM_AUDIO_PATH)" OFF)
option(LOVR_USE_OCULUS_AUDIO "Enable the Oculus Audio spatializer (be sure to also set LOVR_OCULUS_AUDIO_PATH)" OFF)

option(LOVR_SANITIZE "Enable Address Sanitizer" OFF)
option(LOVR_PROFILE "Enable Tracy integration" OFF)

option(LOVR_SYSTEM_GLFW "Use the system-provided glfw" OFF)
option(LOVR_SYSTEM_LUA "Use the system-provided Lua" OFF)
option(LOVR_SYSTEM_OPENXR "Use the system-provided OpenXR" OFF)

option(LOVR_BUILD_EXE "Build an executable (or an apk on Android)" ON)
option(LOVR_BUILD_SHARED "Build a shared library (takes precedence over LOVR_BUILD_EXE)" OFF)
option(LOVR_BUILD_BUNDLE "On macOS, build a .app bundle instead of a raw program" OFF)
option(LOVR_BUILD_WITH_SYMBOLS "Build with C function symbols exposed" OFF)

# Setup
if(EMSCRIPTEN)
  string(CONCAT EMSCRIPTEN_LINKER_FLAGS
    "-Os "
    "-sUSE_WEBGPU=1 "
    "-sFORCE_FILESYSTEM=1 "
    "-sINITIAL_HEAP=1677721600 "
    "-sEXPORTED_FUNCTIONS=_main,_webxr_attach,_webxr_detach "
    "-sEXPORTED_RUNTIME_METHODS=getValue,setValue "
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
elseif(MSVC)
  add_compile_options(/MP)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
elseif(ANDROID)
  find_package(Java REQUIRED)
  set(LOVR_USE_SIMULATOR OFF)
  if(LOVR_BUILD_EXE)
    set(LOVR_BUILD_SHARED ON)
  endif()
elseif(UNIX)
  find_package(PkgConfig)
  if(APPLE)
    set(LOVR_USE_OPENXR OFF)
  else()
    set(CMAKE_SKIP_RPATH OFF)
  endif()
endif()

if(NOT ANDROID AND LOVR_BUILD_SHARED)
  set(LOVR_BUILD_EXE OFF)
endif()

# GLFW
if(LOVR_USE_GLFW AND NOT (EMSCRIPTEN OR ANDROID))
  if(LOVR_SYSTEM_GLFW)
    pkg_search_module(GLFW REQUIRED glfw3)
    list(APPEND LOVR_INCLUDES ${GLFW_INCLUDE_DIRS})
    set(LOVR_GLFW ${GLFW_LIBRARIES})
  else()
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "")
    set(GLFW_INSTALL OFF CACHE BOOL "")
    set(GLFW_LIBRARY_TYPE "SHARED")
    add_subdirectory(deps/glfw glfw)
    list(APPEND LOVR_INCLUDES deps/glfw/include)
    set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
  endif()
  unset(LIB_SUFFIX CACHE)
endif()

# Lua
if(LOVR_USE_LUAJIT AND NOT EMSCRIPTEN)
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUAJIT REQUIRED luajit)
    set(LOVR_LUA_INCLUDE ${LUAJIT_INCLUDE_DIRS})
    set(LOVR_LUA ${LUAJIT_LIBRARIES})
  else()
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory(deps/luajit luajit)
    set_target_properties(luajit PROPERTIES EXCLUDE_FROM_ALL 1)
    if(MSVC)
      target_compile_definitions(libluajit PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_definitions(minilua PRIVATE _CRT_SECURE_NO_WARNINGS)
      target_compile_definitions(buildvm PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    set(LOVR_LUA_INCLUDE deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
    set(LOVR_LUA libluajit)
  endif()
else()
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUA REQUIRED lua)
    set(LOVR_LUA_INCLUDE ${LUA_INCLUDE_DIRS})
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
    set(LOVR_LUA_INCLUDE deps/lua)
    set(LOVR_LUA lua)
  endif()
endif()

# MSDF
if(LOVR_ENABLE_DATA)
  set(BUILD_SHARED_LIBS ON)
  set(MSDFGEN_CORE_ONLY ON CACHE BOOL "")
  set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "")
  set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "")
  set(MSDFGEN_USE_CPP11 OFF CACHE BOOL "")
  add_subdirectory(deps/msdfgen msdfgen)
  list(APPEND LOVR_INCLUDES deps/msdfgen)
  set(LOVR_MSDF msdfgen-core)
  if(APPLE)
    set_target_properties(msdfgen-core PROPERTIES MACOSX_RPATH ON)
  endif()
endif()

# Jolt
if(LOVR_ENABLE_PHYSICS)
  set(BUILD_SHARED_LIBS OFF)
  set(JPH_BUILD_SHARED ON CACHE BOOL "")
  set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "")
  set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "")
  set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "")
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "")
  endif()
  add_subdirectory(deps/joltc jolt)
  set_target_properties(Jolt PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  set_target_properties(joltc PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  set(LOVR_JOLT joltc)
endif()

# glslang
if(LOVR_USE_GLSLANG)
  set(ENABLE_HLSL OFF CACHE BOOL "")
  set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
  set(ENABLE_GLSLANG_INSTALL OFF CACHE BOOL "")
  set(ENABLE_OPT OFF CACHE BOOL "")
  set(ENABLE_CTEST OFF CACHE BOOL "")
  set(BUILD_EXTERNAL OFF CACHE BOOL "")
  set(BUILD_SHARED_LIBS OFF)
  if(NOT EMSCRIPTEN)
    set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "")
  else()
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
  endif()
  list(APPEND LOVR_INCLUDES deps/glslang/glslang/Include deps/glslang/glslang/Public)
  add_subdirectory(deps/glslang glslang)
  set(LOVR_GLSLANG glslang SPIRV glslang-default-resource-limits)
endif()

# Vulkan
if(LOVR_USE_VULKAN)
  list(APPEND LOVR_INCLUDES deps/vulkan-headers/include)
endif()

# OpenXR
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OPENXR)
  if(LOVR_SYSTEM_OPENXR AND NOT ANDROID)
    pkg_search_module(OPENXR openxr REQUIRED)
    list(APPEND LOVR_INCLUDES ${OPENXR_INCLUDE_DIRS})
    set(LOVR_OPENXR ${OPENXR_LIBRARIES})
  else()
    set(DYNAMIC_LOADER ON CACHE BOOL "")
    set(BUILD_WITH_WAYLAND_HEADERS OFF CACHE BOOL "")
    list(APPEND LOVR_INCLUDES deps/openxr/include)
    add_subdirectory(deps/openxr openxr)
    set(LOVR_OPENXR openxr_loader)
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
  list(APPEND LOVR_INCLUDES "${LOVR_STEAM_AUDIO_PATH}/include")
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

  list(APPEND LOVR_INCLUDES "${LOVR_OCULUS_AUDIO_PATH}/Include")

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

set(LOVR_SRC
  src/core/fs.c
  src/api/api.c
  src/api/l_lovr.c
  src/util.c
)

if(LOVR_BUILD_EXE)
  list(APPEND LOVR_SRC src/main.c)
endif()

if(LOVR_BUILD_SHARED)
  add_library(lovr SHARED ${LOVR_SRC})
elseif(LOVR_BUILD_EXE)
  add_executable(lovr ${LOVR_SRC})
else()
  return()
endif()

if(NOT LOVR_BUILD_EXE)
  target_compile_definitions(lovr PUBLIC LOVR_OMIT_MAIN) # specifically for win32 WinMain
endif()

if(LOVR_BUILD_WITH_SYMBOLS)
  set_target_properties(lovr PROPERTIES C_VISIBILITY_PRESET "default")
else()
  set_target_properties(lovr PROPERTIES C_VISIBILITY_PRESET "hidden")
endif()

set_target_properties(lovr PROPERTIES C_STANDARD 11)
set_target_properties(lovr PROPERTIES C_STANDARD_REQUIRED ON)

target_include_directories(lovr PRIVATE
  ${LOVR_INCLUDES}
  ${LOVR_LUA_INCLUDE}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/std
  ${CMAKE_CURRENT_SOURCE_DIR}/etc
)

target_link_libraries(lovr
  ${LOVR_GLFW}
  ${LOVR_LUA}
  ${LOVR_MSDF}
  ${LOVR_JOLT}
  ${LOVR_GLSLANG}
  ${LOVR_OPENXR}
  ${LOVR_OCULUS_AUDIO}
  ${LOVR_PTHREADS}
  ${EMSCRIPTEN_LINKER_FLAGS}
)

if(LOVR_SANITIZE)
  set(LOVR_SANITIZE_FLAGS "-fsanitize=address,undefined" "-O1" "-fno-omit-frame-pointer")
  target_compile_options(lovr PRIVATE ${LOVR_SANITIZE_FLAGS})
  target_link_options(lovr PRIVATE ${LOVR_SANITIZE_FLAGS})
endif()

if(LOVR_PROFILE)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "You probably want to build in release mode when Tracy is enabled")
  endif()
  option(TRACY_ENABLE "" ON)
  add_subdirectory(deps/tracy)
  target_compile_definitions(lovr PRIVATE LOVR_PROFILE)
  target_link_libraries(lovr Tracy::TracyClient)
  target_include_directories(lovr PRIVATE deps/tracy/public/tracy)
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
    src/api/l_filesystem.c
    src/api/l_filesystem_file.c
    src/lib/miniz/miniz_tinfl.c
  )

  # dmon
  if(NOT EMSCRIPTEN)
    target_sources(lovr PRIVATE src/lib/dmon/dmon.c)
    if(APPLE)
      find_library(CORE_FOUNDATION CoreFoundation)
      find_library(CORE_SERVICES CoreServices)
      target_link_libraries(lovr "${CORE_FOUNDATION}" "${CORE_SERVICES}")
    endif()
  endif()
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
    src/api/l_graphics_material.c
    src/api/l_graphics_font.c
    src/api/l_graphics_mesh.c
    src/api/l_graphics_model.c
    src/api/l_graphics_readback.c
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
    target_compile_definitions(lovr PRIVATE LOVR_WEBGPU)
    target_sources(lovr PRIVATE src/core/gpu_web.c)
  endif()

  function(compile_shaders)
    if(LOVR_USE_GLSLANG AND ENABLE_GLSLANG_BINARIES AND NOT ANDROID)
      set(GLSLANG_VALIDATOR $<TARGET_FILE:glslang-standalone>)
    elseif(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
      set(GLSLANG_VALIDATOR "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
    else()
      find_program(GLSLANG_VALIDATOR glslang)
      if(NOT GLSLANG_VALIDATOR)
        message(FATAL_ERROR "Need glslangValidator installed or LOVR_USE_GLSLANG enabled")
      endif()
    endif()
    set(LOVR_GLSL "${CMAKE_CURRENT_SOURCE_DIR}/etc/shaders/lovr.glsl")
    file(GLOB shader_files "etc/shaders/*.${ARGV0}")
    foreach(shader_file ${shader_files})
      string(REGEX MATCH "([^\/]+)\.${ARGV0}" shader ${shader_file})
      string(REPLACE ".${ARGV0}" "" shader ${shader})
      add_custom_command(
        OUTPUT ${shader_file}.h
        DEPENDS ${shader_file} ${LOVR_GLSL}
        COMMAND
          ${GLSLANG_VALIDATOR}
          --quiet
          $<$<CONFIG:Debug>:-gVS>
          --target-env vulkan1.1
          --vn lovr_shader_${shader}_${ARGV0}
          -o ${shader_file}.h
          ${shader_file}
      )
      target_sources(lovr PRIVATE ${shader_file}.h)
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
    src/modules/headset/headset.c
    src/api/l_headset.c
    src/api/l_headset_layer.c
  )

  if(LOVR_USE_OPENXR)
    target_compile_definitions(lovr PRIVATE LOVR_USE_OPENXR)
    target_sources(lovr PRIVATE src/modules/headset/headset_openxr.c)
  endif()
  if(LOVR_USE_WEBXR)
    target_compile_definitions(lovr PRIVATE LOVR_USE_WEBXR)
    target_sources(lovr PRIVATE src/modules/headset/headset_webxr.c)
  endif()
  if(LOVR_USE_SIMULATOR)
    target_compile_definitions(lovr PRIVATE LOVR_USE_SIMULATOR)
    target_sources(lovr PRIVATE src/modules/headset/headset_simulator.c)
  endif()
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_HEADSET)
endif()

if(LOVR_ENABLE_MATH)
  target_sources(lovr PRIVATE
    src/modules/math/math.c
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
    src/api/l_physics_contact.c
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

  if(LOVR_USE_GLFW)
    target_compile_definitions(lovr PRIVATE LOVR_USE_GLFW)
  endif()
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_SYSTEM)
endif()

if(LOVR_ENABLE_THREAD)
  target_sources(lovr PRIVATE
    src/core/job.c
    src/modules/thread/thread.c
    src/api/l_thread.c
    src/api/l_thread_channel.c
    src/api/l_thread_thread.c
  )
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_THREAD)
endif()

if(LOVR_ENABLE_TIMER)
  target_sources(lovr PRIVATE src/modules/timer/timer.c src/api/l_timer.c)
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_TIMER)
endif()

if(LOVR_ENABLE_UTF8)
  target_sources(lovr PRIVATE src/lib/luax/lutf8lib.c)
else()
  target_compile_definitions(lovr PRIVATE LOVR_DISABLE_UTF8)
endif()

# Plugins
if(NOT EMSCRIPTEN)
  set(LOVR 1)
  if(NOT DEFINED LOVR_PLUGINS)
    file(GLOB LOVR_PLUGINS ${CMAKE_SOURCE_DIR}/plugins/*)
  endif()
  function(glob_targets dir targets)
    get_directory_property(subdirectories DIRECTORY "${dir}" SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
      glob_targets("${subdirectory}" ${targets})
    endforeach()
    get_directory_property(dir_targets DIRECTORY "${dir}" BUILDSYSTEM_TARGETS)
    set(${targets} ${${targets}} ${dir_targets} PARENT_SCOPE)
  endfunction()
  foreach(PLUGIN_PATH ${LOVR_PLUGINS})
    if(IS_DIRECTORY ${PLUGIN_PATH} AND EXISTS ${PLUGIN_PATH}/CMakeLists.txt)
      get_filename_component(PLUGIN "${PLUGIN_PATH}" NAME)
      add_subdirectory(${PLUGIN_PATH} "${CMAKE_CURRENT_BINARY_DIR}/plugins/${PLUGIN}")
      get_directory_property(PLUGIN_TARGETS DIRECTORY ${PLUGIN_PATH} DEFINITION LOVR_PLUGIN_TARGETS)
      if(NOT PLUGIN_TARGETS)
        glob_targets(${PLUGIN_PATH} PLUGIN_TARGETS)
      endif()
      set_target_properties(${PLUGIN_TARGETS} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/${PLUGIN}")
      foreach(PLUGIN_TARGET ${PLUGIN_TARGETS})
        target_include_directories(${PLUGIN_TARGET} PRIVATE ${LOVR_LUA_INCLUDE})
        target_link_libraries(${PLUGIN_TARGET} ${LOVR_LUA})
      endforeach()
      list(APPEND ALL_PLUGIN_TARGETS ${PLUGIN_TARGETS})
    endif()
  endforeach()
endif()

# Resources
file(GLOB LOVR_RESOURCES "etc/*.ttf" "etc/*.lua" "etc/shaders/*.glsl")
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

if(NOT ANDROID AND NOT EMSCRIPTEN)
  if(APPLE AND LOVR_BUILD_BUNDLE)
    set(NOGAME_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/lovr.app/Contents/Resources/nogame.lovr")
  else()
    set(NOGAME_BUNDLE "$<TARGET_FILE:lovr>")
  endif()
  add_custom_command(TARGET lovr POST_BUILD
    DEPENDS "etc/nogame"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/etc/nogame"
    COMMAND ${CMAKE_COMMAND} -E tar c "${CMAKE_CURRENT_BINARY_DIR}/nogame.zip" --format=zip .
    COMMAND ${CMAKE_COMMAND} -E cat "${CMAKE_CURRENT_BINARY_DIR}/nogame.zip" >> ${NOGAME_BUNDLE}
  )
endif()

if(LOVR_VERSION_HASH)
  target_compile_definitions(lovr PRIVATE LOVR_VERSION_HASH=${LOVR_VERSION_HASH})
endif()

# Add a custom target that is always out of date so libraries are always moved
add_custom_target(move_files ALL)

# Moves something from etc to the build directory
function(move_resource)
  set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/etc/${ARGV0}")
  add_custom_command(TARGET lovr POST_BUILD
    DEPENDS ${SRC}
    COMMAND ${CMAKE_COMMAND} -E copy ${SRC} $<TARGET_FILE_DIR:lovr>/${ARGV0}
  )
endfunction()

# Platforms
if(WIN32)
  target_sources(lovr PRIVATE src/core/os_win32.c)
  target_sources(lovr PRIVATE etc/lovr.rc)
  if(MSVC)
    set_target_properties(lovr PROPERTIES COMPILE_FLAGS /wd4244)
    # Excuse anonymous union for type punning
    set_source_files_properties(src/util.c src/modules/graphics/graphics.c PROPERTIES COMPILE_FLAGS /wd4116)
    # Excuse unsigned negation for flag-magic bit math
    set_source_files_properties(src/modules/audio/audio.c PROPERTIES COMPILE_FLAGS /wd4146)
    set_source_files_properties(src/lib/minimp3/minimp3.c PROPERTIES COMPILE_FLAGS /wd4267)
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
      add_custom_command(TARGET move_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${ARGV0}>
        $<TARGET_FILE_DIR:lovr>/$<TARGET_FILE_NAME:${ARGV0}>
      )
    endif()
  endfunction()

  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_JOLT})
  move_dll(${LOVR_MSDF})
  move_dll(${LOVR_OCULUS_AUDIO})
  move_dll(${LOVR_PHONON})
  move_dll(${LOVR_OPENXR})
  foreach(target ${ALL_PLUGIN_TARGETS})
    move_dll(${target})
  endforeach()
  move_resource("lovrc.bat")
elseif(APPLE)
  find_library(AVFOUNDATION AVFoundation)
  target_link_libraries(lovr objc ${AVFOUNDATION})
  target_sources(lovr PRIVATE src/core/os_macos.c)
  set_source_files_properties(src/core/os_macos.c PROPERTIES COMPILE_FLAGS -xobjective-c)
  set_target_properties(lovr PROPERTIES
    MACOSX_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path"
    ENABLE_EXPORTS ON
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
      if(${TARGET_TYPE} STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET move_files POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_SONAME_FILE:${ARGV0}>
          ${EXE_DIR}/$<TARGET_SONAME_FILE_NAME:${ARGV0}>
        )
      else()
        add_custom_command(TARGET move_files POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${ARGV0}>
          ${EXE_DIR}/$<TARGET_FILE_NAME:${ARGV0}>
        )
      endif()
    endif()
  endfunction()
  move_lib(${LOVR_GLFW})
  move_lib(${LOVR_LUA})
  move_lib(${LOVR_JOLT})
  move_lib(${LOVR_MSDF})
  move_lib(${LOVR_OCULUS_AUDIO})
  move_lib(${LOVR_PHONON})
  foreach(target ${ALL_PLUGIN_TARGETS})
    move_lib(${target})
  endforeach()
elseif(EMSCRIPTEN)
  target_sources(lovr PRIVATE src/core/os_wasm.c)
  set_target_properties(lovr PROPERTIES ENABLE_EXPORTS ON)
  configure_file(etc/lovr.ico favicon.ico COPYONLY)
elseif(ANDROID)
  set(ANDROID_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/etc/AndroidManifest.xml" CACHE STRING "The AndroidManifest.xml file to use")
  file(READ ${ANDROID_MANIFEST} ANDROID_MANIFEST_CONTENT)
  string(REGEX MATCH "package=\"([^\"]*)" _ ${ANDROID_MANIFEST_CONTENT})
  set(ANDROID_PACKAGE ${CMAKE_MATCH_1})

  string(REPLACE "." "_" ANDROID_PACKAGE_C ${ANDROID_PACKAGE})
  string(REPLACE "." "/" ANDROID_PACKAGE_JAVA ${ANDROID_PACKAGE})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/etc/Activity.java ${CMAKE_CURRENT_BINARY_DIR}/Activity.java)
  target_compile_definitions(lovr PRIVATE "LOVR_JAVA_PACKAGE=${ANDROID_PACKAGE_C}")

  target_sources(lovr PRIVATE src/core/os_android.c)
  target_link_libraries(lovr log android dl)
  target_include_directories(lovr PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")

  # Dynamically linked targets output libraries in raw/lib/<ABI> for easy including in apk with aapt
  set_target_properties(
    lovr
    ${LOVR_JOLT}
    ${LOVR_MSDF}
    ${LOVR_LUA}
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/raw/lib/${ANDROID_ABI}"
  )

  if(LOVR_BUILD_EXE)
    set(ANDROID_JAR "${ANDROID_SDK}/platforms/${ANDROID_PLATFORM}/android.jar")
    set(ANDROID_TOOLS "${ANDROID_SDK}/build-tools/${ANDROID_BUILD_TOOLS_VERSION}")
    set(ANDROID_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/etc/nogame" CACHE STRING "The project folder to include in the APK")

    if(LOVR_USE_OPENXR)
      add_custom_command(TARGET move_files POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_SONAME_FILE:${LOVR_OPENXR}>
        raw/lib/${ANDROID_ABI}/libopenxr_loader.so
      )

      configure_file("${ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" "raw/lib/${ANDROID_ABI}/libc++_shared.so" COPYONLY)
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
      COMMAND ${Java_JAVAC_EXECUTABLE} -classpath "${ANDROID_JAR}" -d . Activity.java
      COMMAND ${ANDROID_TOOLS}/d8 --min-api ${ANDROID_NATIVE_API_LEVEL} --output raw ${ANDROID_PACKAGE_JAVA}/Activity.class
      COMMAND
        ${ANDROID_TOOLS}/aapt
        package -f
        ${PACKAGE_RENAME}
        -0 so
        -M AndroidManifest.xml
        -I ${ANDROID_JAR}
        -F lovr.unaligned.apk
        -A ${ANDROID_ASSETS}
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
      get_target_property(TARGET_TYPE ${target} TYPE)
      if(${TARGET_TYPE} STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET move_files POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${target}>
            raw/lib/${ANDROID_ABI}/$<TARGET_SONAME_FILE_NAME:${target}>
        )
      elseif(${TARGET_TYPE} STREQUAL "MODULE_LIBRARY")
        add_custom_command(TARGET move_files POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${target}>
            raw/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:${target}>
        )
      endif()
    endforeach()
    add_dependencies(buildAPK move_files)
  endif()
elseif(UNIX)
  target_sources(lovr PRIVATE src/core/os_linux.c)
  if(LOVR_USE_GLFW)
    target_link_libraries(lovr X11 xcb X11-xcb)
  else()
    target_link_libraries(lovr xcb xcb-xinput xcb-xkb xkbcommon xkbcommon-x11)
  endif()
  set_target_properties(lovr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "\$ORIGIN"
    ENABLE_EXPORTS ON
  )
  function(move_lib)
    if(TARGET ${ARGV0})
      get_target_property(TARGET_TYPE ${ARGV0} TYPE)
      if(${TARGET_TYPE} STREQUAL "MODULE_LIBRARY")
        add_custom_command(TARGET move_files POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:${ARGV0}>
          ${CMAKE_BINARY_DIR}/bin/$<TARGET_FILE_NAME:${ARGV0}>
        )
      elseif(${TARGET_TYPE} STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET move_files POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_SONAME_FILE:${ARGV0}>
          ${CMAKE_BINARY_DIR}/bin/$<TARGET_SONAME_FILE_NAME:${ARGV0}>
        )
      endif()
    endif()
  endfunction()
  move_lib(${LOVR_GLFW})
  move_lib(${LOVR_LUA})
  move_lib(${LOVR_JOLT})
  move_lib(${LOVR_MSDF})
  move_lib(${LOVR_OPENXR})
  move_lib(${LOVR_OCULUS_AUDIO})
  foreach(target ${ALL_PLUGIN_TARGETS})
    move_lib(${target})
  endforeach()
  if(LOVR_BUILD_BUNDLE)
    move_resource("lovr.desktop")
    move_resource("AppRun")
    move_resource("logo.svg")
  endif()
endif()
