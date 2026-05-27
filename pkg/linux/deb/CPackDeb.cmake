# Debian packaging configuration for Garcon.
#
# This file centralizes the CPack settings used to build Debian packages for
# Linux. It is included from the top-level CMakeLists.txt so `make dist` in a
# configured build tree emits a `.deb` package into the source `bin/` folder.
#
# Copyright 2025 Tomaz Stih. All rights reserved.
# MIT License.

file(READ "${CMAKE_SOURCE_DIR}/pkg/linux/deb/description.txt"
     GARCON_DEBIAN_DESCRIPTION)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "garcon")
set(CPACK_PACKAGE_VENDOR "Tomaz Stih")
set(CPACK_PACKAGE_CONTACT "Tomaz Stih")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Lean and minimal HTTP/HTTPS server written in modern C++")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/pkg")

set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${GARCON_DEBIAN_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "web")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
