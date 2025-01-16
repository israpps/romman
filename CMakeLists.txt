if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wno-format)

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

if(NOT APPLE)
  target_link_libraries(romman -static -static-libgcc -static-libstdc++)
else()
  message(WARNING "Static linking is not supported on macOS. Skipping -static flags.")
endif()
