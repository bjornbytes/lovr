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
file(GLOB_RECURSE LOVR_SRC src/*.c)
add_executable(lovr ${LOVR_SRC})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# LuaJIT
if(WIN32)
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

# OpenGL
if(NOT WIN32)
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_GL ${OPENGL_LIBRARIES})
endif()

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
if(WIN32)
  add_subdirectory(deps/glfw glfw)
  include_directories(deps/glfw/include)
  set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
else()
  pkg_search_module(GLFW REQUIRED glfw3)
  include_directories(${GLFW_INCLUDE_DIRS})
  set(LOVR_GLFW ${GLFW_LIBRARIES})
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

if(WIN32)
  add_subdirectory(deps/physfs physfs)
  include_directories(deps/physfs)
  set(LOVR_PHYSFS physfs)
else()
  find_package(PhysFS REQUIRED)
  include_directories(${PHYSFS_INCLUDE_DIR})
  set(LOVR_PHYSFS ${PHYSFS_LIBRARY})
endif()

# assimp
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_NO_EXPORT ON OFF CACHE BOOL "")

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")

if(WIN32)
  add_subdirectory(deps/assimp assimp)
  include_directories(deps/assimp/include)
  set(LOVR_ASSIMP assimp)
else()
  pkg_search_module(ASSIMP REQUIRED assimp)
  include_directories(${ASSIMP_INCLUDE_DIRS})
  set(LOVR_ASSIMP ${ASSIMP_LIBRARIES})
endif()

# OpenAL
set(ALSOFT_UTILS OFF CACHE BOOL "")
set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
set(ALSOFT_TESTS OFF CACHE BOOL "")
if (WIN32)
  add_subdirectory(deps/openal-soft openal)
  include_directories(deps/openal-soft/include)
  set(LOVR_OPENAL OpenAL32)
else()
  find_package(OpenAL REQUIRED)
  include_directories(${OPENAL_INCLUDE_DIR})
  set(LOVR_OPENAL ${OPENAL_LIBRARY})
endif()

# FreeType
if (WIN32)
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory(deps/freetype freetype)
  include_directories(deps/freetype/include)
  set(LOVR_FREETYPE freetype)
else()
  pkg_search_module(FREETYPE REQUIRED freetype2)
  include_directories(${FREETYPE_INCLUDE_DIRS})
  set(LOVR_FREETYPE ${FREETYPE_LIBRARIES})
endif()

# openvr
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

set(LOVR_LIB
  ${LOVR_LUA}
  ${LOVR_GL}
  ${LOVR_GLFW}
  ${LOVR_PHYSFS}
  ${LOVR_ASSIMP}
  ${LOVR_OPENAL}
  ${LOVR_OPENVR}
  ${LOVR_FREETYPE}
)

target_link_libraries(lovr ${LOVR_LIB})

if(WIN32)
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_PHYSFS})
  move_dll(${LOVR_ASSIMP})
  move_dll(${LOVR_OPENAL})
  move_dll(${LOVR_OPENVR})
  move_dll(${LOVR_FREETYPE})
endif()
