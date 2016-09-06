cmake_minimum_required(VERSION 3.0.0)

function(move_dll ARG_TARGET)
  add_custom_command(TARGET lovr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${ARG_TARGET}>
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/$<TARGET_FILE_NAME:${ARG_TARGET}>
  )
endfunction()

project(lovr)
file(GLOB_RECURSE LOVR_SRC src/*.c)
add_executable(lovr ${LOVR_SRC})

# LuaJIT
add_subdirectory(deps/luajit luajit)
include_directories(deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
set(LOVR_LUA liblua)

# GLEW
add_subdirectory(deps/glew glew)
include_directories(deps/glew/include)
set(LOVR_GLEW glew)

# GLFW
add_subdirectory(deps/glfw glfw)
include_directories(deps/glfw/include)
set(LOVR_GLFW glfw ${GLFW_LIBRARIES})

# assimp
option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
option(ASSIMP_BUILD_TESTS OFF)
add_subdirectory(deps/assimp assimp)
include_directories(deps/assimp/include)
set(LOVR_ASSIMP assimp)

# openvr (todo)
include_directories(deps/openvr/headers)
target_link_libraries(lovr ${CMAKE_SOURCE_DIR}/deps/openvr/lib/win32/openvr_api.lib)
add_custom_command(TARGET lovr POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${CMAKE_SOURCE_DIR}/deps/openvr/bin/win32/openvr_api.dll"
  $<TARGET_FILE_DIR:lovr>
)

set(LOVR_LIB
  ${LOVR_LUA}
  ${LOVR_GLEW}
  ${LOVR_GLFW}
  ${LOVR_ASSIMP}
)

target_link_libraries(lovr ${LOVR_LIB})

move_dll(${LOVR_LUA})
move_dll(${LOVR_GLEW})
move_dll(${LOVR_GLFW})
move_dll(${LOVR_ASSIMP})
