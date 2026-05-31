find_package(OpenSSL REQUIRED)
include(GNUInstallDirs)

file(GLOB_RECURSE GARCON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

list(REMOVE_ITEM GARCON_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/static_files.cpp"
)

add_executable(garcon ${GARCON_SOURCES})

target_compile_features(garcon PRIVATE cxx_std_23)

target_include_directories(garcon PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(garcon PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
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
