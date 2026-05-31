add_library(garcon_cors_module SHARED
    module.cpp
    cors_module.cpp
    ../../src/http/response.cpp
)

target_compile_features(garcon_cors_module PRIVATE cxx_std_23)

target_include_directories(garcon_cors_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

set_target_properties(garcon_cors_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
