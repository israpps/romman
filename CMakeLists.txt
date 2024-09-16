cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(romman)

include_directories(include)
add_executable(romman
  src/main.cpp
  src/rom.cpp
  src/utils.cpp
)
