cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 17)
project(metaltoy)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(metal-cmake)
add_subdirectory(src)
