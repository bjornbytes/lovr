cmake_minimum_required(VERSION 3.0.0)
project(lovr)

# Setup
if(EMSCRIPTEN)
  add_definitions(-DLOVR_WEB=1)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(LOVR_EMSCRIPTEN_FLAGS "-O3 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_FREETYPE=1 -s USE_GLFW=3 -s USE_ZLIB=1 -s ASSERTIONS=1 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s NO_EXIT_RUNTIME=1 -s TOTAL_MEMORY=134217728 -s ELIMINATE_DUPLICATE_FUNCTIONS=1")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  include_directories("${EMSCRIPTEN_ROOT_PATH}/tests/zlib")
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
endif()

# Assimp
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_NO_EXPORT ON OFF CACHE BOOL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
if(EMSCRIPTEN)
  set(ZLIB_FOUND 1)
  set(ZLIB_LIBRARIES "-s USE_ZLIB=1")
  set(ZLIB_INCLUDE_DIR "${EMSCRIPTEN_ROOT_PATH}/tests/zlib")
  add_subdirectory(deps/assimp assimp)
  include_directories(deps/assimp/include)
  set(LOVR_ASSIMP assimp)
elseif(WIN32)
  add_subdirectory(deps/assimp assimp)
  include_directories(deps/assimp/include)
  set(LOVR_ASSIMP assimp)
else()
  pkg_search_module(ASSIMP REQUIRED assimp)
  include_directories(${ASSIMP_INCLUDE_DIRS})
  set(LOVR_ASSIMP ${ASSIMP_LIBRARIES})
endif()

# FreeType
if (EMSCRIPTEN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_FREETYPE=1")
elseif (WIN32)
  set(FREETYPE_NO_DIST TRUE CACHE BOOL "")
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory(deps/freetype freetype)
  set(BUILD_SHARED_LIBS ON)
  include_directories(deps/freetype/include)
  set(LOVR_FREETYPE freetype)
else()
  pkg_search_module(FREETYPE REQUIRED freetype2)
  include_directories(${FREETYPE_INCLUDE_DIRS})
  set(LOVR_FREETYPE ${FREETYPE_LIBRARIES})
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

# Lua
if(EMSCRIPTEN)
  option(LUA_USE_RELATIVE_LOADLIB OFF)
  option(LUA_USE_ULONGJMP OFF)
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
add_subdirectory(deps/msdfgen lib_msdfgen)
include_directories(deps/msdfgen)
set(LOVR_MSDF lib_msdfgen)

# ODE
if(EMSCRIPTEN)
  set(ODE_BUILD_SHARED OFF CACHE BOOL "")
  add_subdirectory(deps/ode ode)
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
  set(LOVR_ODE ode)
endif()

# OpenAL
set(ALSOFT_UTILS OFF CACHE BOOL "")
set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
set(ALSOFT_TESTS OFF CACHE BOOL "")
if (WIN32)
  add_subdirectory(deps/openal-soft openal)
  include_directories(deps/openal-soft/include)
  set(LOVR_OPENAL OpenAL32)
elseif(NOT EMSCRIPTEN)
  pkg_search_module(OPENAL REQUIRED openal)
  include_directories(${OPENAL_INCLUDE_DIRS})
  string(REPLACE ";" " " OPENAL_LDFLAGS_STR "${OPENAL_LDFLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPENAL_LDFLAGS_STR}")
  set(LOVR_OPENAL ${OPENAL_LIBRARIES})
endif()

# OpenGL
if(NOT WIN32)
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_OPENGL ${OPENGL_LIBRARIES})
endif()

# OpenVR
if(NOT EMSCRIPTEN)
  if(NOT DEFINED OPENVR_DIR AND IS_DIRECTORY "${PROJECT_SOURCE_DIR}/deps/openvr")
    SET(OPENVR_DIR deps/openvr)
  else()
    message(FATAL_ERROR "Please specify the location of OpenVR (-DOPENVR_DIR=/path/to/openvr)")
  endif()
  if(APPLE)
    add_definitions(-DOSX -DPOSIX)
  elseif(UNIX)
    add_definitions(-DLINUX -DPOSIX)
  endif()
  set(BUILD_SHARED ON CACHE BOOL "")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -fvisibility=hidden")
  add_definitions(-DVR_API_PUBLIC)
  include_directories(${OPENVR_DIR}/headers)
  include_directories(${OPENVR_DIR}/src)
  include_directories(${OPENVR_DIR}/src/vrcommon)
  add_subdirectory(${OPENVR_DIR}/src openvr_api)
  set(LOVR_OPENVR openvr_api)
endif()

# PhysicsFS
set(PHYSFS_BUILD_STATIC OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_7Z OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_GRP OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_WAD OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_HOG OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_MVL OFF CACHE BOOL "")
set(PHYSFS_ARCHIVE_QPAK OFF CACHE BOOL "")
set(PHYSFS_BUILD_TEST OFF CACHE BOOL "")
set(PHYSFS_BUILD_WX_TEST FALSE CACHE BOOL "")
if(EMSCRIPTEN)
  option(PHYSFS_ARCHIVE_ZIP OFF)
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

# LÖVR
set(LOVR_SRC
  src/api/audio.c
  src/api/event.c
  src/api/filesystem.c
  src/api/graphics.c
  src/api/headset.c
  src/api/math.c
  src/api/physics.c
  src/api/timer.c
  src/api/types/blob.c
  src/api/types/collider.c
  src/api/types/controller.c
  src/api/types/font.c
  src/api/types/joints.c
  src/api/types/mesh.c
  src/api/types/model.c
  src/api/types/shader.c
  src/api/types/shapes.c
  src/api/types/skybox.c
  src/api/types/source.c
  src/api/types/texture.c
  src/api/types/transform.c
  src/api/types/world.c
  src/audio/audio.c
  src/audio/source.c
  src/event/event.c
  src/filesystem/blob.c
  src/filesystem/filesystem.c
  src/graphics/font.c
  src/graphics/graphics.c
  src/graphics/mesh.c
  src/graphics/model.c
  src/graphics/shader.c
  src/graphics/skybox.c
  src/graphics/texture.c
  src/headset/headset.c
  src/lib/glad/glad.c
  src/lib/lua-cjson/fpconv.c
  src/lib/lua-cjson/lua_cjson.c
  src/lib/lua-cjson/strbuf.c
  src/lib/map/map.c
  src/lib/stb/stb_image.c
  src/lib/stb/stb_vorbis.c
  src/lib/vec/vec.c
  src/loaders/font.c
  src/loaders/model.c
  src/loaders/source.c
  src/loaders/texture.c
  src/lovr.c
  src/luax.c
  src/main.c
  src/math/mat4.c
  src/math/quat.c
  src/math/transform.c
  src/math/vec3.c
  src/physics/physics.c
  src/timer/timer.c
  src/util.c
)

if(EMSCRIPTEN)
  set(LOVR_HEADSET src/headset/webvr.c)
else()
  set(LOVR_HEADSET src/headset/openvr.c)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(lovr ${LOVR_SRC} ${LOVR_HEADSET})
target_link_libraries(lovr
  ${LOVR_ASSIMP}
  ${LOVR_FREETYPE}
  ${LOVR_GLFW}
  ${LOVR_LUA}
  ${LOVR_MSDF}
  ${LOVR_ODE}
  ${LOVR_OPENAL}
  ${LOVR_OPENGL}
  ${LOVR_OPENVR}
  ${LOVR_PHYSFS}

  ${LOVR_EMSCRIPTEN_FLAGS}
)

if(WIN32)
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
  move_dll(${LOVR_OPENAL})
  move_dll(${LOVR_OPENVR})
  move_dll(${LOVR_PHYSFS})
endif()
