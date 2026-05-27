# Top-level CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(garcon VERSION 0.0.5 LANGUAGES CXX)

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

enable_testing()
add_subdirectory(tests)

include("${CMAKE_SOURCE_DIR}/pkg/linux/deb/CPackDeb.cmake")
include(CPack)

add_custom_target(dist
    COMMAND ${CMAKE_CPACK_COMMAND} --config ${CMAKE_BINARY_DIR}/CPackConfig.cmake
    COMMAND ${CMAKE_COMMAND}
            -DCPACK_OUTPUT_DIR=${CMAKE_BINARY_DIR}/pkg
            -DBIN_OUTPUT_DIR=${CMAKE_SOURCE_DIR}/bin
            -P ${CMAKE_SOURCE_DIR}/pkg/linux/deb/FinalizeDeb.cmake
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    DEPENDS garcon
    VERBATIM
)
