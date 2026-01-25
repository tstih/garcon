file(GLOB_RECURSE GARCON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(garcon ${GARCON_SOURCES})

target_compile_features(garcon PRIVATE cxx_std_20)

target_include_directories(garcon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(
    TARGET garcon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:garcon>/www"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/www"
            "$<TARGET_FILE_DIR:garcon>/www"
    VERBATIM
)
