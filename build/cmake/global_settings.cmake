#
# Global settings across all projects
#

# For IDEs that support it, turn on folders:
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Remove library prefix for compatibility with callers who don't expect a lib prefix:
# TODO 15993: eventually update callers to understand libs on platforms that typically have prefix
#       Example: libamalgam.so/libamalgam.dylib/amalgam.dll
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Generator:
set(IS_NINJA False)
set(IS_VISUALSTUDIO False)
if("${CMAKE_GENERATOR}" MATCHES "[Nn]inja")
    set(IS_NINJA True)
elseif("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
    set(IS_VISUALSTUDIO True)
else()
    message(WARNING "Unknown generator '${CMAKE_GENERATOR}', usage is not officially supported")
endif()

# OS:
set(OS "unknown")
set(OSv2 "unknown")
set(IS_WINDOWS False)
set(IS_UNIX False)
set(IS_LINUX False)
set(IS_MACOS False)
set(NEWLINE_STYLE "unknown")
if(WIN32)
    set(OS "windows")
    set(OSv2 "${OS}")
    set(IS_WINDOWS True)
    set(NEWLINE_STYLE "WIN32")
elseif(UNIX AND NOT APPLE)
    set(OS "linux")
    set(OSv2 "${OS}")
    set(IS_UNIX True)
    set(IS_LINUX True)
    set(NEWLINE_STYLE "UNIX")
elseif(UNIX)
    set(OS "macos")
    set(OSv2 "darwin")
    set(IS_UNIX True)
    set(IS_MACOS True)
    set(NEWLINE_STYLE "UNIX")
else()
    message(WARNING "Unknown OS, usage is not officially supported")
endif()

# Arch:
set(IS_AMD64 False)
set(IS_ARM64 False)
set(IS_ARM64_8A False)
set(IS_WASM False)
if("${ARCH}" MATCHES "^arm64")
    set(IS_ARM64 True)
    if("${ARCH}" STREQUAL "arm64_8a")
        set(IS_ARM64_8A True)
    endif()
elseif("${ARCH}" STREQUAL "wasm64")
    set(IS_WASM True)
else() # default arch
    set(IS_AMD64 True)
    set(ARCH "amd64")
endif()

# System info:
cmake_host_system_information(RESULT NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT TOTAL_VIRTUAL_MEMORY QUERY TOTAL_VIRTUAL_MEMORY)
cmake_host_system_information(RESULT AVAILABLE_VIRTUAL_MEMORY QUERY AVAILABLE_VIRTUAL_MEMORY)
cmake_host_system_information(RESULT TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
cmake_host_system_information(RESULT AVAILABLE_PHYSICAL_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)

# Install permissions:
set(DEFAULT_INSTALL_PERMISSIONS)
if(IS_WASM)
    set(DEFAULT_INSTALL_PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ
    )
else()
    set(DEFAULT_INSTALL_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ             GROUP_EXECUTE
        WORLD_READ             WORLD_EXECUTE
    )
endif()

# RPATH:
if(IS_UNIX)
    set(CMAKE_SKIP_INSTALL_RPATH True)

    if(IS_WASM)
        set(CMAKE_BUILD_WITH_INSTALL_RPATH True)
    endif()
endif()

# Exe ext for WASM:
if(IS_WASM)
    set(CMAKE_EXECUTABLE_SUFFIX ".cjs")
endif()
