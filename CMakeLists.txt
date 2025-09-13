cmake_minimum_required(VERSION 4.0)
project(chip8)

set(CMAKE_CXX_STANDARD 26)

find_package(SDL3 REQUIRED)
include_directories(${SDL3_INCLUDE_DIRS})

add_executable(chip8 main.cpp)
target_link_libraries(chip8 ${SDL3_LIBRARIES})
