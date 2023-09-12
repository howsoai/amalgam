#
# Custom read-only targets with other files from source tree for convenience
#

# Build files:
file(GLOB_RECURSE CONFIG_FILES "build/*")
list(APPEND CONFIG_FILES
    .gitignore
    CMakeLists.txt
    CMakePresets.json
    open-in-vs.bat
)
add_custom_target("${PROJECT_NAME}-build" SOURCES ${CONFIG_FILES})
set_target_properties("${PROJECT_NAME}-build" PROPERTIES FOLDER "Utilities" EXCLUDE_FROM_ALL True)

# Scripts:
file(GLOB_RECURSE SCRIPT_FILES "Amalgam/amlg_code/*")
add_custom_target("${PROJECT_NAME}-scripts" SOURCES ${SCRIPT_FILES})
set_target_properties("${PROJECT_NAME}-scripts" PROPERTIES FOLDER "Utilities" EXCLUDE_FROM_ALL True)

# Docs:
file(GLOB_RECURSE DOC_FILES "docs/*")
list(APPEND DOC_FILES README.md LICENSE-3RD-PARTY.txt)
add_custom_target("${PROJECT_NAME}-docs" SOURCES ${DOC_FILES})
set_target_properties("${PROJECT_NAME}-docs" PROPERTIES FOLDER "Utilities" EXCLUDE_FROM_ALL True)

# Examples:
file(GLOB_RECURSE EXAMPLE_FILES "examples/*")
add_custom_target("${PROJECT_NAME}-examples" SOURCES ${EXAMPLE_FILES})
set_target_properties("${PROJECT_NAME}-examples" PROPERTIES FOLDER "Utilities" EXCLUDE_FROM_ALL True)

# Set source group for all static projects:
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${CONFIG_FILES} ${SCRIPT_FILES} ${DOC_FILES} ${EXAMPLE_FILES})
