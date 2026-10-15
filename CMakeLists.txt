cmake_minimum_required(VERSION 3.20)

project(VoxelEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glm CONFIG REQUIRED)

function(voxel_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endfunction()

add_library(VoxelCore
    src/core/camera.cpp
    src/core/options.cpp
    src/voxel/chunk.cpp
    src/voxel/mesh.cpp
    src/voxel/terrain.cpp
    src/voxel/world.cpp
    src/voxel/world_codec.cpp
    src/voxel/world_io.cpp
    src/voxel/raycast.cpp
    src/voxel/editor.cpp
    src/voxel/edit_history.cpp
)
target_include_directories(VoxelCore PUBLIC src)
target_link_libraries(VoxelCore PUBLIC glm::glm)
voxel_warnings(VoxelCore)

option(VOXEL_BUILD_APP "Build the OpenGL terrain viewer" ON)
if(VOXEL_BUILD_APP)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    add_executable(VoxelEngine
        src/main.cpp
        src/core/window.cpp
        src/core/input.cpp
        src/render/shader.cpp
        src/render/gpu_mesh.cpp
        src/render/world_renderer.cpp
        src/render/overlay.cpp
        src/render/diagnostics.cpp
    )
    target_link_libraries(VoxelEngine PRIVATE VoxelCore OpenGL::GL glfw GLEW::GLEW)
    voxel_warnings(VoxelEngine)
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(test_name IN ITEMS chunk mesh terrain camera world raycast editor options world_codec edit_history world_io greedy_mesh)
        add_executable(${test_name}_tests tests/${test_name}_tests.cpp)
        target_link_libraries(${test_name}_tests PRIVATE VoxelCore)
        voxel_warnings(${test_name}_tests)
        add_test(NAME ${test_name} COMMAND ${test_name}_tests)
    endforeach()
endif()
