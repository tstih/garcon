add_library(garcon_rate_limit_module SHARED
    module.cpp
    rate_limit_module.cpp
    ../../src/http/response.cpp
)

target_compile_features(garcon_rate_limit_module PRIVATE cxx_std_23)

target_include_directories(garcon_rate_limit_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(garcon_rate_limit_module PRIVATE
    garcon_sanitizers
)

set_target_properties(garcon_rate_limit_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
