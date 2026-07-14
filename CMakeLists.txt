cmake_minimum_required(VERSION 3.20)

project(GameEngine1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)

add_executable(GameEngine1
    main.cpp
    glad.c
)

target_include_directories(GameEngine1 PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/include
)

target_link_libraries(GameEngine1 PRIVATE glfw)