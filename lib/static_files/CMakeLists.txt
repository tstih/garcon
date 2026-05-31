add_library(garcon_static_files_module SHARED
    module.cpp
    static_files_module.cpp
    ../../src/http/response.cpp
    ../../src/static_files.cpp
)

target_compile_features(garcon_static_files_module PRIVATE cxx_std_23)

target_include_directories(garcon_static_files_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

set_target_properties(garcon_static_files_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
