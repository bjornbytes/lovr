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

# GLEW / OpenGL
if(WIN32)
  add_subdirectory(deps/glew glew)
  include_directories(deps/glew/include)
  set(LOVR_GL glew)
else()
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_GL ${OPENGL_LIBRARIES})
endif()

# GLFW
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
option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
option(ASSIMP_BUILD_TESTS OFF)
if(WIN32)
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
else()
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
