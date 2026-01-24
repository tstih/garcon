# Top-level CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(garcon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# Force static libraries for everything
set(BUILD_SHARED_LIBS OFF)

# We want a debug build by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Add subdirectories ---
add_subdirectory(src)