cmake_minimum_required(VERSION 3.20)

project(GameEngine1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(GameEngine1 src/main.cpp)

target_link_libraries(GameEngine1 PRIVATE
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
)