cmake_minimum_required(VERSION 3.20)

project(VoxelEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(VoxelEngine src/main.cpp src/core/debug.cpp)

target_link_libraries(VoxelEngine PRIVATE
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
)

target_include_directories(VoxelEngine PRIVATE src)