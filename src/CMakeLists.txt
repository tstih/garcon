find_package(OpenSSL REQUIRED)
include(GNUInstallDirs)

file(GLOB_RECURSE GARCON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(garcon ${GARCON_SOURCES})

target_compile_features(garcon PRIVATE cxx_std_23)

target_include_directories(garcon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(garcon PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
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
    "${CMAKE_SOURCE_DIR}/docs/TUTORIAL.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
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
