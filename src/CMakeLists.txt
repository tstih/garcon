add_executable(garcon
    main.cpp
    net_socket.cpp
    net_listener.cpp
    http_buffer.cpp
    http_framing.cpp
    http_request.cpp
    http_response.cpp
    static_files.cpp
    server.cpp
)

target_compile_features(garcon PRIVATE cxx_std_20)

add_custom_command(
    TARGET garcon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:garcon>/www"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/www"
            "$<TARGET_FILE_DIR:garcon>/www"
    VERBATIM
)
