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
