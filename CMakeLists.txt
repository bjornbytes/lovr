cmake_minimum_required(VERSION 3.1.0)
project(lovr)

# Setup
if(EMSCRIPTEN)
  string(CONCAT LOVR_EMSCRIPTEN_FLAGS
    "-O3 "
    "-s WASM=1 "
    "-s USE_GLFW=3 "
    "-s USE_WEBGL2=1 "
    "-s GL_PREINITIALIZED_CONTEXT=1 "
    "-s USE_ZLIB=1 "
    "-s FULL_ES3=1 "
    "-s FORCE_FILESYSTEM=1 "
    "-s ALLOW_MEMORY_GROWTH=1 "
    "-s \"EXPORTED_FUNCTIONS=[ "
      "'_main','_lovrRun','_lovrQuit','_lovrDestroy',"
      "'_mat4_identity','_mat4_invert','_mat4_multiply','_mat4_rotateQuat','_mat4_transform','_mat4_transformDirection','_mat4_translate',"
      "'_quat_fromMat4','_quat_getAngleAxis'"
    "]\" "
    "-s \"EXTRA_EXPORTED_RUNTIME_METHODS=['getValue','setValue']\" "
    "--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lovr.js\" "
    "--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lovr.html\""
  )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
endif()

# PhysicsFS
set(PHYSFS_BUILD_STATIC OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_7Z OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_GRP OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_WAD OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_HOG OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_MVL OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_QPAK OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_SLB OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_ISO9660 OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_VDF OFF CACHE BOOL "")
set(PHYSFS_BUILD_TEST OFF CACHE BOOL "")
set(PHYSFS_BUILD_WX_TEST FALSE CACHE BOOL "")
if(EMSCRIPTEN)
  add_subdirectory(deps/physfs physfs)
  include_directories(deps/physfs/src)
  set(LOVR_PHYSFS physfs)
elseif(WIN32)
  add_subdirectory(deps/physfs physfs)
  include_directories(deps/physfs/src)
  set(LOVR_PHYSFS physfs)
else()
  find_package(PhysFS REQUIRED)
  include_directories(${PHYSFS_INCLUDE_DIR})
  set(LOVR_PHYSFS ${PHYSFS_LIBRARY})
endif()

# Assimp
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
if(EMSCRIPTEN)
  set(ZLIB_FOUND 1)
  set(ZLIB_LIBRARIES "-s USE_ZLIB=1")
  set(ZLIB_INCLUDE_DIR "assimp/contrib/zlib")
  add_subdirectory(deps/assimp assimp)
  include_directories(deps/assimp/include ${CMAKE_BINARY_DIR}/assimp/include)
  set(LOVR_ASSIMP assimp)
elseif(WIN32)
  add_subdirectory(deps/assimp assimp)
  include_directories(deps/assimp/include ${CMAKE_BINARY_DIR}/assimp/include)
  set(LOVR_ASSIMP assimp)
else()
  pkg_search_module(ASSIMP REQUIRED assimp)
  include_directories(${ASSIMP_INCLUDE_DIRS})
  set(LOVR_ASSIMP ${ASSIMP_LIBRARIES})
endif()

# enet
if(EMSCRIPTEN)
  set(HAVE_HAS_SOCKLEN_T TRUE CACHE BOOL "")
  add_definitions(-D__APPLE__)
  add_subdirectory(deps/enet enet)
  include_directories(deps/enet/include)
  set(LOVR_ENET enet)
  remove_definitions(-D__APPLE__)
else()
  add_subdirectory(deps/enet enet)
  include_directories(deps/enet/include)
  set(LOVR_ENET enet)
  if(WIN32)
    set(LOVR_ENET ${LOVR_ENET} ws2_32 winmm)
  endif()
endif()

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
if(WIN32)
  add_subdirectory(deps/glfw glfw)
  include_directories(deps/glfw/include)
  set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
elseif(NOT EMSCRIPTEN)
  pkg_search_module(GLFW REQUIRED glfw3)
  include_directories(${GLFW_INCLUDE_DIRS})
  set(LOVR_GLFW ${GLFW_LIBRARIES})
endif()
unset(LIB_SUFFIX CACHE)

# Lua
if(EMSCRIPTEN)
  option(LUA_USE_RELATIVE_LOADLIB OFF)
  option(LUA_USE_ULONGJMP OFF)
  option(LUA_USE_POPEN OFF)
  add_subdirectory(deps/lua lua)
  set_target_properties(lua luac liblua liblua_static PROPERTIES EXCLUDE_FROM_ALL 1)
  include_directories(deps/lua/src ${CMAKE_BINARY_DIR}/lua)
  set(LOVR_LUA liblua_static)
elseif(WIN32)
  add_subdirectory(deps/luajit luajit)
  include_directories(deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
  set(LOVR_LUA liblua)
else()
  if (APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pagezero_size 10000 -image_base 100000000")
  endif()
  pkg_search_module(LUAJIT REQUIRED luajit)
  include_directories(${LUAJIT_INCLUDE_DIRS})
  set(LOVR_LUA ${LUAJIT_LIBRARIES})
endif()

# MSDF
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(deps/msdfgen lib_msdfgen)
set(BUILD_SHARED_LIBS ON)
include_directories(deps/msdfgen)
set(LOVR_MSDF lib_msdfgen)

# ODE
if(EMSCRIPTEN)
  set(ODE_BUILD_SHARED OFF CACHE BOOL "")
  add_subdirectory(deps/ode ode)
  set_target_properties(ode PROPERTIES COMPILE_FLAGS "-Wno-unused-volatile-lvalue -Wno-array-bounds -Wno-undefined-var-template")
  include_directories(deps/ode/include "${CMAKE_CURRENT_BINARY_DIR}/ode/include")
  set(LOVR_ODE ode)
elseif(WIN32)
  set(ODE_BUILD_SHARED ON CACHE BOOL "")
  add_subdirectory(deps/ode ode)
  include_directories(deps/ode/include "${CMAKE_CURRENT_BINARY_DIR}/ode/include")
  set(LOVR_ODE ode)
else()
  pkg_search_module(ODE REQUIRED ode)
  include_directories(${ODE_INCLUDE_DIRS})
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")
  if(APPLE)
    set(LOVR_ODE ode ccd)
  else()
    set(LOVR_ODE ode)
  endif()
endif()

# OpenAL
set(USE_OPENAL ON CACHE BOOL "If set to OFF, OpenAL is excluded and the behavior of lovr.audio is undefined")
set(ALSOFT_UTILS OFF CACHE BOOL "")
set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
set(ALSOFT_TESTS OFF CACHE BOOL "")

if(USE_OPENAL)
  add_definitions(-DUSE_OPENAL)
  set(LOVR_AUDIO_SRC src/audio/audio.c src/audio/source.c src/audio/microphone.c)

  if(EMSCRIPTEN)
    include_directories(deps/openal-soft/include)
  elseif(WIN32)
    add_subdirectory(deps/openal-soft openal)
    include_directories(deps/openal-soft/include)
    set_target_properties(OpenAL PROPERTIES COMPILE_FLAGS "/wd4005 /wd4098")
    set(LOVR_OPENAL OpenAL)
  else()
    pkg_search_module(OPENAL openal-soft)
    if (NOT OPENAL_FOUND)
      pkg_search_module(OPENAL openal)
      if (NOT OPENAL_FOUND)
        message(FATAL_ERROR "OpenAL not found.")
      endif()
    endif()
    include_directories(${OPENAL_INCLUDE_DIRS})
    string(REPLACE ";" " " OPENAL_LDFLAGS_STR "${OPENAL_LDFLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPENAL_LDFLAGS_STR}")
    set(LOVR_OPENAL ${OPENAL_LIBRARIES})
  endif()
else()
  set(LOVR_AUDIO_SRC src/audio/null.c)
endif()

# OpenGL
if(NOT WIN32 AND NOT EMSCRIPTEN)
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_OPENGL ${OPENGL_LIBRARIES})
endif()

# OpenVR
if(NOT EMSCRIPTEN)
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

# Oculus SDK
if (NOT EMSCRIPTEN AND LOVR_OCULUS_PATH)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(OCULUS_BUILD_TYPE "Release")
  else()
    set(OCULUS_BUILD_TYPE "Debug")
  endif()

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OCULUS_ARCH "x64")
  else()
    set(OCULUS_ARCH "Win32")
  endif()

  add_definitions(-DUSE_OCULUS)
  include_directories("${LOVR_OCULUS_PATH}/LibOVR/Include")
  link_directories("${LOVR_OCULUS_PATH}/LibOVR/Lib/Windows/${OCULUS_ARCH}/${OCULUS_BUILD_TYPE}/VS2017")
  set(LOVR_OCULUS LibOVR)
endif()

# pthreads
if(NOT WIN32)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set(LOVR_PTHREADS Threads::Threads)
endif()

# LÖVR source
set(LOVR_SRC
  src/api/audio.c
  src/api/data.c
  src/api/event.c
  src/api/filesystem.c
  src/api/graphics.c
  src/api/headset.c
  src/api/lovr.c
  src/api/math.c
  src/api/physics.c
  src/api/thread.c
  src/api/timer.c
  src/api/types/animator.c
  src/api/types/audioStream.c
  src/api/types/blob.c
  src/api/types/canvas.c
  src/api/types/channel.c
  src/api/types/collider.c
  src/api/types/controller.c
  src/api/types/font.c
  src/api/types/joints.c
  src/api/types/material.c
  src/api/types/mesh.c
  src/api/types/microphone.c
  src/api/types/model.c
  src/api/types/modelData.c
  src/api/types/randomGenerator.c
  src/api/types/rasterizer.c
  src/api/types/shader.c
  src/api/types/shaderBlock.c
  src/api/types/shapes.c
  src/api/types/soundData.c
  src/api/types/source.c
  src/api/types/texture.c
  src/api/types/textureData.c
  src/api/types/thread.c
  src/api/types/transform.c
  src/api/types/vertexData.c
  src/api/types/world.c
  src/data/audioStream.c
  src/data/blob.c
  src/data/modelData.c
  src/data/rasterizer.c
  src/data/soundData.c
  src/data/textureData.c
  src/data/vertexData.c
  src/event/event.c
  src/filesystem/file.c
  src/filesystem/filesystem.c
  src/graphics/animator.c
  src/graphics/font.c
  src/graphics/graphics.c
  src/graphics/material.c
  src/graphics/model.c
  src/graphics/opengl.c
  src/headset/fake.c
  src/headset/headset.c
  src/lib/glad/glad.c
  src/lib/lua-cjson/fpconv.c
  src/lib/lua-cjson/lua_cjson.c
  src/lib/lua-cjson/strbuf.c
  src/lib/lua-enet/enet.c
  src/lib/map/map.c
  src/lib/noise1234/noise1234.c
  src/lib/stb/stb_image.c
  src/lib/stb/stb_image_write.c
  src/lib/stb/stb_truetype.c
  src/lib/stb/stb_vorbis.c
  src/lib/tinycthread/tinycthread.c
  src/lib/vec/vec.c
  src/lovr.c
  src/luax.c
  src/main.c
  src/math/mat4.c
  src/math/math.c
  src/math/quat.c
  src/math/randomGenerator.c
  src/math/transform.c
  src/math/vec3.c
  src/physics/physics.c
  src/resources/shaders.c
  src/thread/channel.c
  src/thread/thread.c
  src/timer/timer.c
  src/util.c
  ${LOVR_AUDIO_SRC}
)

# Conditional sources
if(EMSCRIPTEN)
  set(LOVR_SRC ${LOVR_SRC} src/headset/webvr.c)
else()
  set(LOVR_SRC ${LOVR_SRC} src/headset/openvr.c)

  if(LOVR_OCULUS_PATH)
    set(LOVR_SRC ${LOVR_SRC} src/headset/oculus.c)
  endif()
endif()

# LÖVR
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(lovr ${LOVR_SRC})
set_property(TARGET lovr PROPERTY C_STANDARD 99)
target_link_libraries(lovr
  ${LOVR_ASSIMP}
  ${LOVR_ENET}
  ${LOVR_GLFW}
  ${LOVR_LUA}
  ${LOVR_MSDF}
  ${LOVR_ODE}
  ${LOVR_OPENAL}
  ${LOVR_OPENGL}
  ${LOVR_OPENVR}
  ${LOVR_OCULUS}
  ${LOVR_PHYSFS}
  ${LOVR_PTHREADS}
  ${LOVR_EMSCRIPTEN_FLAGS}
)

# Yay Windows
if(WIN32)
  set_target_properties(lovr PROPERTIES COMPILE_FLAGS "/wd4244")
  set_target_properties(lovr PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
  set_target_properties(lovr PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

  if(MSVC_VERSION VERSION_LESS 1900)
    target_compile_definitions(lovr PUBLIC -Dinline=__inline -Dsnprintf=_snprintf)
  endif()

  function(move_dll ARG_TARGET)
    add_custom_command(TARGET lovr POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
      $<TARGET_FILE:${ARG_TARGET}>
      ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/$<TARGET_FILE_NAME:${ARG_TARGET}>
    )
  endfunction()

  move_dll(${LOVR_ASSIMP})
  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_ODE})
  if (LOVR_OPENAL)
    move_dll(${LOVR_OPENAL})
  endif()
  move_dll(${LOVR_OPENVR})
  move_dll(${LOVR_PHYSFS})
endif()
