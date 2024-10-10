#
# Global compiler defines & flags:
#

# TODO 15993: do we need this? Can it be smaller? How do we set it on all platforms?
set(DEFAULT_STACK_SIZE_WIN 67108864)

set(DEFAULT_STACK_SIZE_MACOS 0x4000000)

set(IS_MSVC False)
set(IS_GCC False)
set(IS_CLANG False)
set(IS_GNU_FLAG_COMPAT_COMPILER False)
set(IS_APPLECLANG False)
if(MSVC)

    set(IS_MSVC True)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.30)
        message(WARNING "MSVC version '${CMAKE_CXX_COMPILER_VERSION}' < 19.30, usage is not officially supported")
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Common flags:
    string(APPEND CMAKE_CXX_FLAGS " /nologo /W3 /WX /MP /GS /TP /FC /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /analyze-")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " /STACK:${DEFAULT_STACK_SIZE_WIN}")

    # Debug flags:
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " /JMC")
    if(IS_VISUALSTUDIO)
        # EditAndContinue only works with VS generator:
        string(APPEND CMAKE_CXX_FLAGS_DEBUG " /ZI")
    else()
        string(APPEND CMAKE_CXX_FLAGS_DEBUG " /Zi")
    endif()

    # Release flags:
    string(REPLACE "/O2" "/Ox" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi /Oi /Ot /Oy /GF /GL /GT /Gy /Gd")
    set(ALL_LINKER_FLAGS_RELEASE CMAKE_EXE_LINKER_FLAGS_RELEASE CMAKE_SHARED_LINKER_FLAGS_RELEASE)
    foreach(FLAGS_NAME in ${ALL_LINKER_FLAGS_RELEASE})
        string(APPEND ${FLAGS_NAME} " /LTCG:incremental /OPT:REF /OPT:ICF /DEBUG:FASTLINK")
    endforeach()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    # Note: GCC and Clang are mostly flag-compatible so we set flags the same but allow for special handling if needed
    #       through IS_* vars.
    set(IS_GCC_FLAG_COMPAT_COMPILER True)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(IS_GCC True)
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
            message(WARNING "GCC version '${CMAKE_CXX_COMPILER_VERSION}' < 10, usage is not officially supported")
        endif()
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(IS_CLANG True)
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
            message(WARNING "Clang version '${CMAKE_CXX_COMPILER_VERSION}' < 17, usage is not officially supported")
        endif()
    endif()

    string(APPEND CMAKE_CXX_FLAGS " -fPIC -fno-strict-aliasing -Wall -Wno-unknown-pragmas -Werror")
    # TODO 1599: Additional warnings that are fairly strict, not enabled right now
    #string(APPEND CMAKE_CXX_FLAGS " -Wpedantic -Wextra -Wabi")

    if(IS_ARM64)
        # See for discussion why set: https://stackoverflow.com/questions/52020305/what-exactly-does-gccs-wpsabi-option-do-what-are-the-implications-of-supressi
        string(APPEND CMAKE_CXX_FLAGS " -Wno-psabi")

        if(IS_ARM64_8A)
            add_compile_definitions(NO_REENTRANCY_LOCKS)
        endif()
    endif()

    # TODO 1599: WASM support is experimental, these flags will be cleaned up and auto-generated where possible
    if(IS_WASM)
        string(APPEND CMAKE_CXX_FLAGS " -sMEMORY64=2 -Wno-experimental -DSIMDJSON_NO_PORTABILITY_WARNING")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -sINVOKE_RUN=0 -sALLOW_MEMORY_GROWTH=1 -sMEMORY_GROWTH_GEOMETRIC_STEP=0.50 -sMODULARIZE=1 -sEXPORT_NAME=AmalgamRuntime -sENVIRONMENT=worker,node,web -sEXPORTED_RUNTIME_METHODS=cwrap,ccall,FS,setValue,getValue,UTF8ToString -sEXPORTED_FUNCTIONS=_malloc,_free,_LoadEntity,_CloneEntity,_VerifyEntity,_StoreEntity,_ExecuteEntity,_ExecuteEntityJsonPtr,_DestroyEntity,_GetEntities,_SetRandomSeed,_SetJSONToLabel,_GetJSONPtrFromLabel,_SetSBFDataStoreEnabled,_IsSBFDataStoreEnabled,_GetVersionString,_SetMaxNumThreads,_GetMaxNumThreads,_GetConcurrencyTypeString,_DeleteString --preload-file /wasm/tzdata@/tzdata --preload-file /wasm/etc@/etc")
        # Set memory arguments
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            string(APPEND CMAKE_EXE_LINKER_FLAGS " -sINITIAL_HEAP=65536000 -sSTACK_SIZE=33554432")
        else()
            string(APPEND CMAKE_EXE_LINKER_FLAGS " -sINITIAL_HEAP=16777216 -sSTACK_SIZE=8388608")
        endif()
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")

    set(IS_APPLECLANG True)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
        message(WARNING "AppleClang version '${CMAKE_CXX_COMPILER_VERSION}' < 14, usage is not officially supported")
    endif()

    string(APPEND CMAKE_CXX_FLAGS " -fPIC -fno-strict-aliasing -Wall -Wno-unknown-pragmas -Werror")
    # TODO 1599: Additional warnings that are fairly strict, not enabled right now
    #string(APPEND CMAKE_CXX_FLAGS " -Wpedantic -Wextra -Wabi")

else()

    message(WARNING "Unknown generator '${CMAKE_CXX_COMPILER_ID}', usage is not officially supported")

endif()

# Unix only:
set(ARCH_VERSION "amd64")
set(ARM64_LIB_DIR) # used by arm emulator for testing
if(IS_UNIX)
    if(IS_WASM)
        add_compile_definitions(USE_OS_TZDB=0 HAS_REMOTE_API=0 INSTALL=/)
    else()
        add_compile_definitions(USE_OS_TZDB=1)
    endif()

    # Arch flag:
    if(IS_AMD64)
        set(ARCH_VERSION "x86-64")
    elseif(IS_ARM64)
        if(IS_MACOS)
            set(ARCH_VERSION "armv8.5-a+simd")
        elseif(IS_LINUX)
            set(ARM64_LIB_DIR "/usr/aarch64-linux-gnu")
            if(IS_ARM64_8A)
                set(ARCH_VERSION "armv8-a+simd")
            else()
                set(ARCH_VERSION "armv8.2-a+simd+rcpc")
            endif()
        endif()
    endif()
    if(NOT IS_WASM)
        string(APPEND CMAKE_CXX_FLAGS " -march=${ARCH_VERSION}")
    endif()
endif()

# Set stack size for macOS
if(IS_MACOS)
    if(IS_WASM)
        set(CMAKE_CXX_LINK_FLAGS "")
    else()
        string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-stack_size,${DEFAULT_STACK_SIZE_MACOS}")
    endif()
endif()

# MSVC only:
if(IS_MSVC)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# Ensure that debug WASM builds include additional WASM-specific linker flags 
if (IS_WASM AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -sASSERTIONS=2")
    # Remove the below flags as they are incompatible with debugging WASM
    string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-Wlimited-postlink-optimizations" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(APPEND CMAKE_CXX_FLAGS " -O0 -gseparate-dwarf")
endif()

# amd64 advanced intrinsics:
# Note: allowed values - avx avx2 avx512
set(ADVANCED_INTRINSICS_AMD64 "avx2")
if(IS_WINDOWS)
    string(TOUPPER ${ADVANCED_INTRINSICS_AMD64} ADVANCED_INTRINSICS_AMD64)
endif()
# Used for naming built binaries without AVX:
set(NO_ADVANCED_INTRINSICS_AMD64_SUFFIX "noavx")