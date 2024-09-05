#
# Version parsing/setting:
#

# Unknown version vars:
set(AMALGAM_VERSION_MAJOR_UNKNOWN 0)
set(AMALGAM_VERSION_MINOR_UNKNOWN 0)
set(AMALGAM_VERSION_PATCH_UNKNOWN 0)
set(AMALGAM_VERSION_PRERELEASE_UNKNOWN "alpha")
set(AMALGAM_VERSION_METADATA_UNKNOWN "local.dev")
set(AMALGAM_VERSION_UNKNOWN
    "${AMALGAM_VERSION_MAJOR_UNKNOWN}.${AMALGAM_VERSION_MINOR_UNKNOWN}.${AMALGAM_VERSION_PATCH_UNKNOWN}-${AMALGAM_VERSION_PRERELEASE_UNKNOWN}+${AMALGAM_VERSION_METADATA_UNKNOWN}")

# Version:
set(AMALGAM_VERSION_ORIG "${AMALGAM_VERSION_UNKNOWN}")
set(AMALGAM_VERSION)
set(IS_VERSION_FROM_GIT_TAG False)
if(DEFINED ENV{AMALGAM_BUILD_VERSION} AND NOT "$ENV{AMALGAM_BUILD_VERSION}" STREQUAL "")

    message(STATUS "Reading version from env var 'AMALGAM_BUILD_VERSION'")
    set(AMALGAM_VERSION_ORIG "$ENV{AMALGAM_BUILD_VERSION}")
    set(AMALGAM_VERSION "${AMALGAM_VERSION_ORIG}")

else()

    if(TRY_GIT_TAG_FOR_UNKNOWN_VERSION)

        message(STATUS "Reading version from latest git tag")

        # Get latest git tag
        find_package(Git)
        if(Git_NOT_FOUND)
            message(WARNING "Git not found, cannot get release tags. Defaulting to unknown version")
        else()
            execute_process(COMMAND
                "${GIT_EXECUTABLE}" describe --abbrev=0 --tags
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE GIT_RETURN_CODE
                OUTPUT_VARIABLE GIT_TAG_LATEST
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(GIT_RETURN_CODE AND NOT GIT_RETURN_CODE EQUAL 0)
                message(WARNING "Git command failed, cannot get release tags. Defaulting to unknown version")
            else()
                set(AMALGAM_VERSION_ORIG ${GIT_TAG_LATEST})
                string(REPLACE "version-" "" GIT_TAG_LATEST "${GIT_TAG_LATEST}")
                string(REGEX REPLACE "^v" "" GIT_TAG_LATEST "${GIT_TAG_LATEST}")
                set(AMALGAM_VERSION ${GIT_TAG_LATEST})
                set(IS_VERSION_FROM_GIT_TAG True)
            endif()
        endif()

    else()
        message(STATUS "No version given. Defaulting to unknown version")
    endif()

endif()

# Parse version number:
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\-*([^\\+]*)\\+*(.*)" VERSION_MATCH "${AMALGAM_VERSION}")
set(AMALGAM_VERSION_MAJOR ${CMAKE_MATCH_1})
set(AMALGAM_VERSION_MINOR ${CMAKE_MATCH_2})
set(AMALGAM_VERSION_PATCH ${CMAKE_MATCH_3})
set(AMALGAM_VERSION_PRERELEASE ${CMAKE_MATCH_4})
set(AMALGAM_VERSION_METADATA ${CMAKE_MATCH_5})

# If version couldn't be parsed, warn and set to unknown:
if("${AMALGAM_VERSION_MAJOR}" STREQUAL "" OR "${AMALGAM_VERSION_MINOR}" STREQUAL "" OR "${AMALGAM_VERSION_PATCH}" STREQUAL "")
    message(WARNING "Version number could not be parsed. Defaulting to unknown version")
    set(AMALGAM_VERSION "${AMALGAM_VERSION_UNKNOWN}")
    set(AMALGAM_VERSION_MAJOR ${AMALGAM_VERSION_MAJOR_UNKNOWN})
    set(AMALGAM_VERSION_MINOR ${AMALGAM_VERSION_MINOR_UNKNOWN})
    set(AMALGAM_VERSION_PATCH ${AMALGAM_VERSION_PATCH_UNKNOWN})
    set(AMALGAM_VERSION_PRERELEASE "${AMALGAM_VERSION_PRERELEASE_UNKNOWN}")
    set(AMALGAM_VERSION_METADATA "${AMALGAM_VERSION_METADATA_UNKNOWN}")
else()

    # Alter version slightly if we read it from a git tag:
    # Note: this is so the semver is always consistent, even between local and automated
    #       builds. If the version is not given to us in the env, we use the last
    #       annotated git tag, increment its patch, and assign new prerelease
    #       and metadata as if it was an unknown version (lowest precedence).
    if(IS_VERSION_FROM_GIT_TAG)
        math(EXPR AMALGAM_VERSION_PATCH "${AMALGAM_VERSION_PATCH}+1")
        set(AMALGAM_VERSION_PRERELEASE "${AMALGAM_VERSION_PRERELEASE_UNKNOWN}")
        set(AMALGAM_VERSION_METADATA "${AMALGAM_VERSION_METADATA_UNKNOWN}")
    endif()

endif()

set(AMALGAM_VERSION_SUFFIX)
if(NOT "${AMALGAM_VERSION_PRERELEASE}" STREQUAL "")
    string(APPEND AMALGAM_VERSION_SUFFIX "-${AMALGAM_VERSION_PRERELEASE}")
endif()
if(NOT "${AMALGAM_VERSION_METADATA}" STREQUAL "")
    string(APPEND AMALGAM_VERSION_SUFFIX "+${AMALGAM_VERSION_METADATA}")
endif()
set(AMALGAM_VERSION_BASE "${AMALGAM_VERSION_MAJOR}.${AMALGAM_VERSION_MINOR}.${AMALGAM_VERSION_PATCH}")
set(AMALGAM_VERSION_FULL "${AMALGAM_VERSION_BASE}${AMALGAM_VERSION_SUFFIX}")
string(REPLACE "+" "\\+" AMALGAM_VERSION_FULL_ESCAPED ${AMALGAM_VERSION_FULL})

# Write version header:
configure_file(
    "${CMAKE_SOURCE_DIR}/build/cmake/configure_files/AmalgamVersion.h.in"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/AmalgamVersion.h"
    NEWLINE_STYLE ${NEWLINE_STYLE}
    @ONLY
)

# Write version.json:
file(WRITE "${CMAKE_BINARY_DIR}/version.json" "{\n  \"version\": \"${AMALGAM_VERSION_FULL}\"\n}")
