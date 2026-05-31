add_library(garcon_host_guard_module SHARED
    module.cpp
    host_guard_module.cpp
    ../../src/http/response.cpp
)

target_compile_features(garcon_host_guard_module PRIVATE cxx_std_23)

target_include_directories(garcon_host_guard_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

set_target_properties(garcon_host_guard_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
