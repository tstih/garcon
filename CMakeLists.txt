# Top-level CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" GARCON_PROJECT_VERSION)
string(STRIP "${GARCON_PROJECT_VERSION}" GARCON_PROJECT_VERSION)

project(garcon VERSION "${GARCON_PROJECT_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# Force static libraries for everything
set(BUILD_SHARED_LIBS OFF)

# We want a debug build by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(GARCON_ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(GARCON_ENABLE_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)
option(GARCON_ENABLE_TSAN "Build with ThreadSanitizer" OFF)

if(GARCON_ENABLE_ASAN AND GARCON_ENABLE_TSAN)
  message(FATAL_ERROR "ASan and TSan cannot be enabled together")
endif()

add_library(garcon_sanitizers INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  if(GARCON_ENABLE_ASAN)
    target_compile_options(garcon_sanitizers INTERFACE
      -fsanitize=address
      -fno-omit-frame-pointer
    )
    target_link_options(garcon_sanitizers INTERFACE
      -fsanitize=address
    )
  endif()

  if(GARCON_ENABLE_UBSAN)
    target_compile_options(garcon_sanitizers INTERFACE
      -fsanitize=undefined
      -fno-omit-frame-pointer
    )
    target_link_options(garcon_sanitizers INTERFACE
      -fsanitize=undefined
    )
  endif()

  if(GARCON_ENABLE_TSAN)
    target_compile_options(garcon_sanitizers INTERFACE
      -fsanitize=thread
      -fno-omit-frame-pointer
    )
    target_link_options(garcon_sanitizers INTERFACE
      -fsanitize=thread
    )
  endif()
endif()

# --- Add subdirectories ---
add_subdirectory(lib)
add_subdirectory(src)

add_dependencies(garcon
    garcon_cors_module
    garcon_host_guard_module
    garcon_router_module
    garcon_header_guard_module
    garcon_jwt_auth_module
    garcon_route_table_module
    garcon_rate_limit_module
    garcon_static_files_module
)

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
