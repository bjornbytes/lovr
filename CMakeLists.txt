cmake_minimum_required(VERSION 3.0.0)
project(lovr)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
file(GLOB SOURCES src/*.c)
if(APPLE)
  set(LUAJIT_OSX_MAGIC "-pagezero_size 10000 -image_base 100000000")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LUAJIT_OSX_MAGIC}")
endif()

add_executable(lovr ${SOURCES})

# LuaJIT
find_package(LuaJIT REQUIRED)
if(LuaJIT_FOUND)
  include_directories(${LUA_INCLUDE_DIR})
  target_link_libraries(lovr ${LUA_LIBRARIES})
else()
  message(STATUS "LuaJIT not found")
endif()

# Assimp
find_package(assimp REQUIRED)
if(assimp_FOUND)
  include_directories(${assimp_INCLUDE_DIRS})
  target_link_libraries(lovr ${assimp_LIBRARIES})
else()
  message(STATUS "assimp not found")
endif()

# GLFW
find_package(GLFW REQUIRED)
if(GLFW_FOUND)
  include_directories(${GLFW_INCLUDE_DIR})
  target_link_libraries(lovr ${GLFW_LIBRARIES})
else()
  message(STATUS "GLFW not found")
endif()

# OSVR FIXME
find_library(OSVR_CLIENTKIT_LIBRARY NAMES osvrClientKit)
find_library(OSVR_CLIENT_LIBRARY NAMES osvrClient)
find_path(OSVR_INCLUDE_DIR NAMES osvr/ClientKit/ClientKit.h)

if(OSVR_INCLUDE_DIR AND OSVR_CLIENTKIT_LIBRARY AND OSVR_CLIENT_LIBRARY)
  include_directories(${OSVR_INCLUDE_DIR})
  target_link_libraries(lovr ${OSVR_CLIENT_LIBRARY})
  target_link_libraries(lovr ${OSVR_CLIENTKIT_LIBRARY})
endif()

# OpenGL
find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
  include_directories(${OPENGL_INCLUDE_DIR})
  target_link_libraries(lovr ${OPENGL_LIBRARIES})
endif()
