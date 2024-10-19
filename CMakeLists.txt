#
# Amalgam Language Interpreter - CMake build
#

cmake_minimum_required(VERSION 3.26)
project(amalgam LANGUAGES CXX DESCRIPTION "Amalgam Language Interpreter")

# Options:
set(USE_OBJECT_LIBS ON CACHE BOOL "Build using object libs")
set(TRY_GIT_TAG_FOR_UNKNOWN_VERSION ON CACHE BOOL
    "If env var AMALGAM_BUILD_VERSION not found, try to use latest git tag for version")

# Project CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/build/cmake")
include(global_settings)
include(version)
include(global_compiler_flags)

# Enforce correct CMAKE_BUILD_TYPE value
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build: ${CMAKE_CXX_FLAGS_RELEASE}")
    if(CMAKE_CXX_FLAGS_RELEASE MATCHES "-g")
        message(FATAL_ERROR "Release build should not include debug symbols (-g)")
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build: ${CMAKE_CXX_FLAGS_DEBUG}")
else()
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# Print useful info from global settings/flags/env
message(STATUS "Amalgam version (orig)       : '${AMALGAM_VERSION_ORIG}'")
message(STATUS "Amalgam version (clean)      : '${AMALGAM_VERSION}'")
message(STATUS "Amalgam version base         : '${AMALGAM_VERSION_BASE}'")
message(STATUS "Amalgam version prelease     : '${AMALGAM_VERSION_PRERELEASE}'")
message(STATUS "Amalgam version metadata     : '${AMALGAM_VERSION_METADATA}'")
message(STATUS "Amalgam version full         : '${AMALGAM_VERSION_FULL}'")
message(STATUS "Amalgam version full escaped : '${AMALGAM_VERSION_FULL_ESCAPED}'")
message(STATUS "Use object libs              : ${USE_OBJECT_LIBS}")
message(STATUS "System name                  : ${CMAKE_SYSTEM_NAME}")
message(STATUS "OS                           : ${OS}")
message(STATUS "Target system version        : ${CMAKE_SYSTEM_VERSION}")
if(IS_VISUALSTUDIO)
message(STATUS "Target system version - VS   : ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
endif()
message(STATUS "Target system processor      : ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Architecture                 : ${ARCH}")
message(STATUS "Architecture version         : ${ARCH_VERSION}")
if(IS_AMD64)
message(STATUS "Advanced intrinsics          : ${ADVANCED_INTRINSICS_AMD64}")
endif()
message(STATUS "Host system processor        : ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "Number logical cores         : ${NUMBER_OF_LOGICAL_CORES}")
message(STATUS "Number physical cores        : ${NUMBER_OF_PHYSICAL_CORES}")
message(STATUS "Total virtual memory         : ${TOTAL_VIRTUAL_MEMORY} MiB")
message(STATUS "Available virtual memory     : ${AVAILABLE_VIRTUAL_MEMORY} MiB")
message(STATUS "Total physical memory        : ${TOTAL_PHYSICAL_MEMORY} MiB")
message(STATUS "Available physical memory    : ${AVAILABLE_PHYSICAL_MEMORY} MiB")
message(STATUS "Build type                   : '${CMAKE_BUILD_TYPE}'")
if(IS_VISUALSTUDIO)
message(STATUS "Build types (multi-configs)  : '${CMAKE_CONFIGURATION_TYPES}'")
endif()
message(STATUS "CMake version                : ${CMAKE_VERSION}")
message(STATUS "Generator                    : ${CMAKE_GENERATOR}")
message(STATUS "Compiler                     : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version             : ${CMAKE_CXX_COMPILER_VERSION}")
if(IS_MSVC)
message(STATUS "MSVC version                 : ${MSVC_VERSION}")
message(STATUS "MSVC toolset                 : ${MSVC_TOOLSET_VERSION}")
endif()
message(STATUS "Compiler C++ flags           : ${CMAKE_CXX_FLAGS}")
message(STATUS "Compiler C++ debug flags     : ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Compiler C++ release flags   : ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Linker C++ app flags         : ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "Linker C++ app debug flags   : ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
message(STATUS "Linker C++ app release flags : ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker C++ lib flags         : ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "Linker C++ lib debug flags   : ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
message(STATUS "Linker C++ lib release flags : ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")


#
# Source files
#

# Common source between apps & libs
# Note: do not change to globbing, that is a CMake anti-pattern
set(COMMON_SOURCE
    src/3rd_party/date/date.h
    src/3rd_party/date/ios.h
    src/3rd_party/date/tz.cpp
    src/3rd_party/date/tz.h
    src/3rd_party/date/tz_private.h
    src/3rd_party/fast_log/src/exp_table.h
    src/3rd_party/murmurhash3/MurmurHash3.cpp
    src/3rd_party/murmurhash3/MurmurHash3.h
    src/3rd_party/simdjson/simdjson.cpp
    src/3rd_party/simdjson/simdjson.h
    src/3rd_party/rapidyaml/rapidyaml-0.7.2.hpp
    src/3rd_party/skarupke_maps/bytell_hash_map.hpp
    src/3rd_party/skarupke_maps/flat_hash_map.hpp
    src/3rd_party/swiftdtoa/SwiftDtoa.cpp
    src/3rd_party/swiftdtoa/SwiftDtoa.h
    src/3rd_party/tweetnacl/tweetnacl.cpp
    src/3rd_party/tweetnacl/tweetnacl.h
    src/Amalgam/Amalgam.h
    src/Amalgam/AmalgamAPI.cpp
    src/Amalgam/AmalgamVersion.h
    src/Amalgam/AssetManager.cpp
    src/Amalgam/AssetManager.h
    src/Amalgam/BinaryPacking.cpp
    src/Amalgam/BinaryPacking.h
    src/Amalgam/Conviction.h
    src/Amalgam/ConvictionUtil.h
    src/Amalgam/Cryptography.cpp
    src/Amalgam/Cryptography.h
    src/Amalgam/DateTimeFormat.cpp
    src/Amalgam/DateTimeFormat.h
    src/Amalgam/DistanceReferencePair.h
    src/Amalgam/entity/Entity.cpp
    src/Amalgam/entity/Entity.h
    src/Amalgam/entity/EntityExternalInterface.cpp
    src/Amalgam/entity/EntityExternalInterface.h
    src/Amalgam/entity/EntityManipulation.cpp
    src/Amalgam/entity/EntityManipulation.h
    src/Amalgam/entity/EntityQueries.cpp
    src/Amalgam/entity/EntityQueries.h
    src/Amalgam/entity/EntityQueriesStatistics.h
    src/Amalgam/entity/EntityQueryBuilder.h
    src/Amalgam/entity/EntityQueryCaches.cpp
    src/Amalgam/entity/EntityQueryCaches.h
    src/Amalgam/entity/EntityWriteListener.cpp
    src/Amalgam/entity/EntityWriteListener.h
    src/Amalgam/evaluablenode/EvaluableNode.cpp
    src/Amalgam/evaluablenode/EvaluableNode.h
    src/Amalgam/evaluablenode/EvaluableNodeManagement.cpp
    src/Amalgam/evaluablenode/EvaluableNodeManagement.h
    src/Amalgam/evaluablenode/EvaluableNodeTreeDifference.cpp
    src/Amalgam/evaluablenode/EvaluableNodeTreeDifference.h
    src/Amalgam/evaluablenode/EvaluableNodeTreeFunctions.cpp
    src/Amalgam/evaluablenode/EvaluableNodeTreeFunctions.h
    src/Amalgam/evaluablenode/EvaluableNodeTreeManipulation.cpp
    src/Amalgam/evaluablenode/EvaluableNodeTreeManipulation.h
    src/Amalgam/FastEMath.h
    src/Amalgam/FastMath.h
    src/Amalgam/FilenameEscapeProcessor.h
    src/Amalgam/GeneralizedDistance.h
    src/Amalgam/HashMaps.h
    src/Amalgam/importexport/FileSupportCAML.cpp
    src/Amalgam/importexport/FileSupportCAML.h
    src/Amalgam/importexport/FileSupportCSV.cpp
    src/Amalgam/importexport/FileSupportCSV.h
    src/Amalgam/importexport/FileSupportJSON.cpp
    src/Amalgam/importexport/FileSupportJSON.h
    src/Amalgam/importexport/FileSupportYAML.cpp
    src/Amalgam/importexport/FileSupportYAML.h
    src/Amalgam/IntegerSet.h
    src/Amalgam/interpreter/Interpreter.cpp
    src/Amalgam/interpreter/Interpreter.h
    src/Amalgam/interpreter/InterpreterDebugger.cpp
    src/Amalgam/interpreter/InterpreterOpcodesBase.cpp
    src/Amalgam/interpreter/InterpreterOpcodesCodeMixing.cpp
    src/Amalgam/interpreter/InterpreterOpcodesDataTypes.cpp
    src/Amalgam/interpreter/InterpreterOpcodesEntityAccess.cpp
    src/Amalgam/interpreter/InterpreterOpcodesEntityControl.cpp
    src/Amalgam/interpreter/InterpreterOpcodesListManipulation.cpp
    src/Amalgam/interpreter/InterpreterOpcodesLogic.cpp
    src/Amalgam/interpreter/InterpreterOpcodesMath.cpp
    src/Amalgam/interpreter/InterpreterOpcodesTransformations.cpp
    src/Amalgam/KnnCache.h
    src/Amalgam/Merger.h
    src/Amalgam/Opcodes.cpp
    src/Amalgam/Opcodes.h
    src/Amalgam/Parser.cpp
    src/Amalgam/Parser.h
    src/Amalgam/PartialSum.h
    src/Amalgam/PerformanceProfiler.cpp
    src/Amalgam/PerformanceProfiler.h
    src/Amalgam/PlatformSpecific.cpp
    src/Amalgam/PlatformSpecific.h
    src/Amalgam/PrintListener.cpp
    src/Amalgam/PrintListener.h
    src/Amalgam/rand/RandomStream.cpp
    src/Amalgam/rand/RandomStream.h
    src/Amalgam/rand/WeightedDiscreteRandomStream.h
    src/Amalgam/SBFDSColumnData.h
    src/Amalgam/SeparableBoxFilterDataStore.cpp
    src/Amalgam/SeparableBoxFilterDataStore.h
    src/Amalgam/string/StringInternPool.h
    src/Amalgam/string/StringManipulation.cpp
    src/Amalgam/string/StringManipulation.h
)
set(COMMON_SOURCE_THREADS
    ${COMMON_SOURCE}
    src/Amalgam/Concurrency.cpp
    src/Amalgam/Concurrency.h
    src/Amalgam/ThreadPool.cpp
    src/Amalgam/ThreadPool.h
)
set(RESOURCE_SOURCE)
if(IS_MSVC)
    set(RESOURCE_SOURCE
        docs/icon/amalgam.ico
        src/Amalgam/resource.h
        src/Amalgam/Resource.rc
    )
endif()
set(AMALGAM_APP_ONLY_SOURCE
    src/Amalgam/AmalgamMain.cpp
    src/Amalgam/AmalgamTrace.cpp
    ${RESOURCE_SOURCE}
)
set(AMALGAM_LIB_ONLY_SOURCE ${RESOURCE_SOURCE})
set(AMALGAM_ALL_SOURCE ${AMALGAM_APP_ONLY_SOURCE} ${COMMON_SOURCE_THREADS})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${AMALGAM_ALL_SOURCE})

# Get all files we care about for easy access in projects (mainly IDEs)
file(GLOB_RECURSE ALL_FILES "${CMAKE_SOURCE_DIR}/*")
list(FILTER ALL_FILES EXCLUDE REGEX "(.*/.git/.*|.*/out/.*|.*/src/.*|.*/test/.*|.*Amalgam\.sln.*)")
file(GLOB_RECURSE ADDITIONAL_FILES "${CMAKE_SOURCE_DIR}/src/Amalgam/out.txt" "${CMAKE_SOURCE_DIR}/src/Amalgam/amlg_code/*")
list(APPEND ALL_FILES ${ADDITIONAL_FILES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${ALL_FILES})
list(APPEND AMALGAM_APP_ONLY_SOURCE ${ALL_FILES})


#
# Include dirs
#

include_directories(
    "${CMAKE_SOURCE_DIR}/src/3rd_party"
    "${CMAKE_SOURCE_DIR}/src/Amalgam"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/entity"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/evaluablenode"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/importexport"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/interpreter"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/rand"
    "${CMAKE_SOURCE_DIR}/src/Amalgam/string"
)


#
# Compiled targets
#

include(custom_add_target)

# Multi-threaded targets (amalgam-mt):
add_compiled_target(AUTO_NAME TYPE "objlib" USE_ADVANCED_ARCH_INTRINSICS USE_THREADS
    SOURCE ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
add_compiled_target(AUTO_NAME TYPE "app" USE_ADVANCED_ARCH_INTRINSICS USE_THREADS
    SOURCE ${COMMON_SOURCE_THREADS} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE})
add_compiled_target(AUTO_NAME TYPE "sharedlib" USE_ADVANCED_ARCH_INTRINSICS USE_THREADS
    SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT amalgam-mt-app)

# Multi-threaded w/ no arch intrinsics targets  (amalgam-mt-noavx):
#
# Note: On amd64, an app/lib combo is built without advanced intrinsics to be run on
#       VMs/emulators/etc that do not have support for them. On arm64 archs, all targets don't
#       use advanced intrinsics so this target is redundant.
if(IS_AMD64 AND NOT IS_WASM)
    add_compiled_target(NAME "${PROJECT_NAME}-mt-${NO_ADVANCED_INTRINSICS_AMD64_SUFFIX}-objlib" TYPE "objlib" USE_THREADS
        SOURCE ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
    add_compiled_target(NAME "${PROJECT_NAME}-mt-${NO_ADVANCED_INTRINSICS_AMD64_SUFFIX}-app" TYPE "app" USE_THREADS
        SOURCE ${COMMON_SOURCE_THREADS} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE} IDE_FOLDER "OtherBuildTargets")
    add_compiled_target(NAME "${PROJECT_NAME}-mt-${NO_ADVANCED_INTRINSICS_AMD64_SUFFIX}-sharedlib" TYPE "sharedlib" USE_THREADS
        SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
endif()

# OpenMP targets  (amalgam-omp):
add_compiled_target(AUTO_NAME TYPE "objlib" USE_ADVANCED_ARCH_INTRINSICS USE_OPENMP
    SOURCE ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
add_compiled_target(AUTO_NAME TYPE "app" USE_ADVANCED_ARCH_INTRINSICS USE_OPENMP
    SOURCE ${COMMON_SOURCE_THREADS} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE} IDE_FOLDER "OtherBuildTargets")
add_compiled_target(AUTO_NAME TYPE "sharedlib" USE_ADVANCED_ARCH_INTRINSICS USE_OPENMP
    SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")

# Single-threaded targets (amalgam-st):
add_compiled_target(AUTO_NAME TYPE "objlib" USE_ADVANCED_ARCH_INTRINSICS
    SOURCE ${COMMON_SOURCE} IDE_FOLDER "OtherBuildTargets")
add_compiled_target(AUTO_NAME TYPE "app" USE_ADVANCED_ARCH_INTRINSICS
    SOURCE ${COMMON_SOURCE} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE})
add_compiled_target(AUTO_NAME TYPE "sharedlib" USE_ADVANCED_ARCH_INTRINSICS
    SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE} IDE_FOLDER "OtherBuildTargets")

# Linux-only/amd64 debug targets
if(IS_LINUX AND IS_AMD64)
    # Single-threaded PGC targets (amalgam-st-pgc):
    add_compiled_target(AUTO_NAME TYPE "objlib" USE_ADVANCED_ARCH_INTRINSICS USE_PGC NO_INSTALL
        SOURCE ${COMMON_SOURCE} IDE_FOLDER "OtherBuildTargets")
    add_compiled_target(AUTO_NAME TYPE "app" USE_ADVANCED_ARCH_INTRINSICS USE_PGC NO_INSTALL
        SOURCE ${COMMON_SOURCE} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE} IDE_FOLDER "OtherBuildTargets")
    add_compiled_target(AUTO_NAME TYPE "sharedlib" USE_ADVANCED_ARCH_INTRINSICS USE_PGC NO_INSTALL
        SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE} IDE_FOLDER "OtherBuildTargets")
    # Single-threaded AFMI targets (amalgam-st-afmi)
    add_compiled_target(AUTO_NAME TYPE "objlib" USE_ADVANCED_ARCH_INTRINSICS USE_AFMI_ST
        SOURCE ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
    add_compiled_target(AUTO_NAME TYPE "app" USE_ADVANCED_ARCH_INTRINSICS USE_AFMI_ST
        SOURCE ${COMMON_SOURCE_THREADS} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE})
    add_compiled_target(AUTO_NAME TYPE "sharedlib" USE_ADVANCED_ARCH_INTRINSICS USE_AFMI_ST
        SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
    # Multi-threaded AFMI targets (amalgam-mt-afmi)
    add_compiled_target(AUTO_NAME TYPE "objlib" USE_ADVANCED_ARCH_INTRINSICS USE_AFMI_MT
        SOURCE ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
    add_compiled_target(AUTO_NAME TYPE "app" USE_ADVANCED_ARCH_INTRINSICS USE_AFMI_MT
        SOURCE ${COMMON_SOURCE_THREADS} APP_ONLY_SOURCE ${AMALGAM_APP_ONLY_SOURCE})
    add_compiled_target(AUTO_NAME TYPE "sharedlib" USE_ADVANCED_ARCH_INTRINSICS USE_AFMI_MT
        SOURCE ${AMALGAM_LIB_ONLY_SOURCE} ${COMMON_SOURCE_THREADS} IDE_FOLDER "OtherBuildTargets")
endif()

# Print the current build type
message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")

#
# Additional artifacts/test/etc
#

include(create_tests)
include(create_package)
include(install_docs)
