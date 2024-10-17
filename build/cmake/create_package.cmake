#
# Package target
#

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${AMALGAM_VERSION_FULL}")
if(NOT IS_WASM)
    # Non-WASM builds
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${OSv2}-${ARCH}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # WASM debug builds
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARCH}-debug")
else()
    # WASM release builds
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${ARCH}")
endif()
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_INSTALL_PREFIX}/../../package")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY False)
# Release builds are never compiled with -g, so no need to strip files during packaging
set(CPACK_STRIP_FILES False)
set(CPACK_THREADS 0)
set(CPACK_ARCHIVE_THREADS 0)
include(CPack)
