# Finalize Debian package output.
#
# This script copies the generated `.deb` artifacts from CPack's build-local
# output directory into the repository `bin/` folder so the final artifact is
# easy to find without leaving CPack's temporary directories there.
#
# Copyright 2025 Tomaz Stih. All rights reserved.
# MIT License.

if(NOT DEFINED CPACK_OUTPUT_DIR OR NOT DEFINED BIN_OUTPUT_DIR)
    message(FATAL_ERROR "CPACK_OUTPUT_DIR and BIN_OUTPUT_DIR must be set")
endif()

file(MAKE_DIRECTORY "${BIN_OUTPUT_DIR}")
file(GLOB deb_files "${CPACK_OUTPUT_DIR}/*.deb")

if(NOT deb_files)
    message(FATAL_ERROR "No Debian package was generated in ${CPACK_OUTPUT_DIR}")
endif()

foreach(deb_file IN LISTS deb_files)
    file(COPY "${deb_file}" DESTINATION "${BIN_OUTPUT_DIR}")
endforeach()
