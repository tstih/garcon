add_executable(garcon_unit_http
    unit_http.cpp
    ../src/http/buffer.cpp
    ../src/http/framing.cpp
    ../src/http/request.cpp
    ../src/http/response.cpp
)

target_compile_features(garcon_unit_http PRIVATE cxx_std_23)
target_include_directories(garcon_unit_http PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(garcon_unit_http PRIVATE
    garcon_sanitizers
)

add_test(
    NAME unit_http
    COMMAND $<TARGET_FILE:garcon_unit_http>
)

add_test(
    NAME cors_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/cors_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME host_guard_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/host_guard_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME ip_limit_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/ip_limit_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME header_guard_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/header_guard_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME jwt_auth_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/jwt_auth_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME keep_alive_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/keep_alive_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME route_table_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/route_table_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME router_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/router_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME rate_limit_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/rate_limit_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME security_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/security_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME https_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/https_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME concurrency_smoke
    COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/concurrency_smoke.sh"
            "$<TARGET_FILE:garcon>"
            "${CMAKE_SOURCE_DIR}"
)
