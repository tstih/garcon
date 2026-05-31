find_package(OpenSSL REQUIRED)
include(GNUInstallDirs)

set(GARCON_SOURCES
    app/access_log.cpp
    app/connection_handler.cpp
    app/command_line.cpp
    app/default_stream_factory.cpp
    app/dynamic_module.cpp
    app/ip_connection_limiter.cpp
    app/module_config.cpp
    app/request_pipeline.cpp
    app/runtime_events.cpp
    app/work_queue.cpp
    app/worker_pool.cpp
    http/buffer.cpp
    http/framing.cpp
    http/request.cpp
    http/response.cpp
    main.cpp
    net/listener.cpp
    net/plain_stream.cpp
    net/socket.cpp
    server.cpp
    tls/context.cpp
    tls/stream.cpp
)

add_executable(garcon ${GARCON_SOURCES})

target_compile_features(garcon PRIVATE cxx_std_23)
target_compile_definitions(garcon PRIVATE GARCON_VERSION="${PROJECT_VERSION}")

target_include_directories(garcon PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(garcon PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    garcon_sanitizers
    ${CMAKE_DL_LIBS}
)

install(TARGETS garcon
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/www/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/garcon/www"
)

install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/CHANGELOG.md"
    "${CMAKE_SOURCE_DIR}/docs/ARCHITECTURE.md"
    "${CMAKE_SOURCE_DIR}/docs/MODULE-DEVELOPMENT-TUTORIAL.md"
    "${CMAKE_SOURCE_DIR}/docs/TUTORIAL.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

add_custom_target(
    garcon_runtime_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:garcon>/www"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:garcon>/modules.d"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/www"
            "$<TARGET_FILE_DIR:garcon>/www"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/modules.d"
            "$<TARGET_FILE_DIR:garcon>/modules.d"
    DEPENDS garcon
    VERBATIM
)
