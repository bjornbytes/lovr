cmake_minimum_required(VERSION 3.0.0)

function(move_dll ARG_TARGET)
  add_custom_command(TARGET lovr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${ARG_TARGET}>
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/$<TARGET_FILE_NAME:${ARG_TARGET}>
  )
endfunction()

if(UNIX)
  find_package(PkgConfig REQUIRED)
endif()

project(lovr)
set(LOVR_SRC
  src/main.c
  src/glfw.c
  src/lovr.c
  src/matrix.c
  src/util.c
  src/loaders/texture.c
  src/loaders/model.c
  src/event/event.c
  src/filesystem/filesystem.c
  src/graphics/graphics.c
  src/graphics/buffer.c
  src/graphics/model.c
  src/graphics/shader.c
  src/graphics/skybox.c
  src/graphics/texture.c
  src/headset/headset.c
  src/timer/timer.c
  src/vendor/map/map.c
  src/vendor/vec/vec.c
  src/lovr/event.c
  src/lovr/filesystem.c
  src/lovr/graphics.c
  src/lovr/headset.c
  src/lovr/timer.c
  src/lovr/types/buffer.c
  src/lovr/types/controller.c
  src/lovr/types/model.c
  src/lovr/types/shader.c
  src/lovr/types/skybox.c
  src/lovr/types/texture.c
)

if (EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  add_definitions(-s USE_ZLIB=1)
  set(ZLIB_FOUND 1)
  set(ZLIB_LIBRARIES "-s USE_ZLIB=1")
  set(ZLIB_INCLUDE_DIR .)
  set(LOVR_SRC ${LOVR_SRC} src/headset/webvr.c)
else()
  set(LOVR_SRC ${LOVR_SRC} src/headset/vive.c)
endif()

add_executable(lovr ${LOVR_SRC})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Lua
if(EMSCRIPTEN)
  option(LUA_USE_RELATIVE_LOADLIB OFF)
  option(LUA_USE_ULONGJMP OFF)
  add_subdirectory(deps/lua lua)
  include_directories(deps/lua/src ${CMAKE_BINARY_DIR}/lua)
  set(LOVR_LUA liblua)
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

# GLEW / OpenGL
if (EMSCRIPTEN)
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIR})
  set(LOVR_GL ${OPENGL_LIBRARIES})
elseif(WIN32)
  add_subdirectory(deps/glew glew)
  include_directories(deps/glew/include)
  set(LOVR_GL glew)
else()
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_GL ${OPENGL_LIBRARIES})
endif()

# GLFW
if(EMSCRIPTEN)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3")
elseif(WIN32)
  add_subdirectory(deps/glfw glfw)
  include_directories(deps/glfw/include)
  set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
else()
  pkg_search_module(GLFW REQUIRED glfw3)
  include_directories(${GLFW_INCLUDE_DIRS})
  set(LOVR_GLFW ${GLFW_LIBRARIES})
endif()

# PhysicsFS
if(EMSCRIPTEN)
  option(PHYSFS_ARCHIVE_ZIP OFF) # Ran into zlib duplicate symbol errors when this was enabled
  add_subdirectory(deps/physfs physfs)
  include_directories(deps/physfs)
  set(LOVR_PHYSFS physfs)
elseif(WIN32)
  add_subdirectory(deps/physfs physfs)
  include_directories(deps/physfs)
  set(LOVR_PHYSFS physfs)
else()
  find_package(PhysFS REQUIRED)
  include_directories(${PHYSFS_INCLUDE_DIR})
  set(LOVR_PHYSFS ${PHYSFS_LIBRARY})
endif()

# assimp
option(ASSIMP_NO_EXPORT ON)
option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
option(ASSIMP_BUILD_TESTS OFF)
option(ASSIMP_BUILD_ZLIB OFF)
if(WIN32 OR EMSCRIPTEN)
  add_subdirectory(deps/assimp assimp)
  include_directories(deps/assimp/include)
  set(LOVR_ASSIMP assimp)
else()
  pkg_search_module(ASSIMP REQUIRED assimp)
  include_directories(${ASSIMP_INCLUDE_DIRS})
  set(LOVR_ASSIMP ${ASSIMP_LIBRARIES})
endif()

# openvr
if(WIN32)
  include_directories(deps/openvr/headers)
  target_link_libraries(lovr ${CMAKE_SOURCE_DIR}/deps/openvr/lib/win32/openvr_api.lib)
  add_custom_command(TARGET lovr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/deps/openvr/bin/win32/openvr_api.dll"
    $<TARGET_FILE_DIR:lovr>
  )
elseif(NOT EMSCRIPTEN)
  if(DEFINED OPENVR_DIR)
    get_filename_component(FULL_OPENVR_DIR ${OPENVR_DIR} ABSOLUTE BASE_DIR ${CMAKE_BINARY_DIR})
    message(STATUS "Setting OpenVR root directory to ${FULL_OPENVR_DIR}")
    include_directories(${FULL_OPENVR_DIR}/headers)
    if(APPLE)
      set(LOVR_OPENVR ${FULL_OPENVR_DIR}/lib/osx32/libopenvr_api.dylib)
    else()
      set(LOVR_OPENVR ${FULL_OPENVR_DIR}/lib/linux32/libopenvr_api.so)
    endif()
  else()
    message(FATAL_ERROR "Please specify the location of OpenVR (-DOPENVR_DIR=/path/to/openvr)")
  endif()
endif()

set(LOVR_LIB
  ${LOVR_LUA}
  ${LOVR_GL}
  ${LOVR_GLFW}
  ${LOVR_PHYSFS}
  ${LOVR_ASSIMP}
  ${LOVR_OPENVR}
)

target_link_libraries(lovr ${LOVR_LIB})

if(WIN32)
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_GL})
  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_PHYSFS})
  move_dll(${LOVR_ASSIMP})
endif()
