cmake_minimum_required(VERSION 3.0.0)
project(lovr)

option(OSVR_PATH "The path to the osvr snapshot")

add_executable(lovr
  src/glfw.c
  src/util.c
  src/shader.c
  src/event.c
  src/graphics.c
  src/model.c
  src/device.c
  src/interface.c
  src/lovr.c
  src/main.c
)

function(move_dll ARG_TARGET)
  add_custom_command(TARGET lovr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${ARG_TARGET}>
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/$<TARGET_FILE_NAME:${ARG_TARGET}>
  )
endfunction()

# LuaJIT
add_subdirectory(deps/luajit luajit)
include_directories(deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
set(LOVR_LUA liblua)

# GLEW
add_subdirectory(deps/glew/build/cmake glew)
include_directories(deps/glew/include)
set(LOVR_GLEW glew)

# GLFW
add_subdirectory(deps/glfw glfw)
include_directories(deps/glfw/include)
set(LOVR_GLFW glfw ${GLFW_LIBRARIES})

# assimp
add_subdirectory(deps/assimp assimp)
include_directories(deps/assimp/include)
set(LOVR_ASSIMP assimp)

# osvr (todo)
include(${OSVR_PATH}/lib/cmake/osvr/osvrConfig.cmake)
include_directories(${OSVR_PATH}/include)
target_link_libraries(lovr osvr::osvrClient)
target_link_libraries(lovr osvr::osvrClientKit)
move_dll(osvr::osvrCommon)
move_dll(osvr::osvrUtil)
move_dll(osvr::osvrClient)
move_dll(osvr::osvrClientKit)

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
