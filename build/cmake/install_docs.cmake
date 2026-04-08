#
# Install docs
#

# Install language reference:
install(
    FILES
        "${CMAKE_SOURCE_DIR}/docs/index.md"
    DESTINATION "docs/language_reference"
)

# Install version.json:
install(FILES "${CMAKE_BINARY_DIR}/version.json" DESTINATION "docs")
