cmake_minimum_required(VERSION 3.10)
project(romman)

include_directories(include)
add_executable(romman
  src/main.cpp
  src/rom.cpp
  src/utils.cpp
)
