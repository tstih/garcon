add_library(garcon_jwt_auth_module SHARED
    module.cpp
    jwt_auth_module.cpp
    ../../src/http/response.cpp
)

target_compile_features(garcon_jwt_auth_module PRIVATE cxx_std_23)

target_include_directories(garcon_jwt_auth_module PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

find_package(OpenSSL REQUIRED)

target_link_libraries(garcon_jwt_auth_module PRIVATE
    garcon_sanitizers
    OpenSSL::Crypto
)

set_target_properties(garcon_jwt_auth_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/modules"
)
