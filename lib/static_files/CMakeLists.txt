add_library(garcon_static_files_service STATIC
    static_files.cpp
)

target_compile_features(garcon_static_files_service PRIVATE cxx_std_23)
set_target_properties(garcon_static_files_service PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(garcon_static_files_service PUBLIC
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(garcon_static_files_service PRIVATE
    garcon_sanitizers
)

add_library(garcon_static_files_module SHARED
    module.cpp
    static_files_module.cpp
    ../../src/http/response.cpp
)

target_compile_features(garcon_static_files_module PRIVATE cxx_std_23)

target_include_directories(garcon_static_files_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(garcon_static_files_module PRIVATE
    garcon_sanitizers
    garcon_static_files_service
)

set_target_properties(garcon_static_files_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
