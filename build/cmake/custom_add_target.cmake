#
# Function for creating custom compiled targets
#
# Notes:
#   1) PGC = Pedantic Garbage Collection
#   2) NO_INSTALL implies don't package, when given the target will be tested but not installed/packaged
#

set(ALL_OBJLIB_TARGETS)
set(ALL_SHAREDLIB_TARGETS)
set(ALL_APP_TARGETS)
function(add_compiled_target)
    set(options AUTO_NAME USE_THREADS USE_OPENMP USE_PGC USE_ADVANCED_ARCH_INTRINSICS NO_INSTALL)
    set(oneValueArgs NAME TYPE OUTPUT_NAME_BASE IDE_FOLDER)
    set(multiValueArgs INCLUDE_DIRS COMPILER_DEFINES LINK_LIBRARIES SOURCE APP_ONLY_SOURCE)
    cmake_parse_arguments(args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Error if unknown args passed in:
    if(NOT "${args_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unparsed args: ${args_UNPARSED_ARGUMENTS}")
    elseif(NOT "${args_KEYWORDS_MISSING_VALUES}" STREQUAL "")
        message(FATAL_ERROR "Args missing values: ${args_KEYWORDS_MISSING_VALUES}")
    endif()

    # Set local scope optional args:
    set(AUTO_NAME ${args_AUTO_NAME})
    set(USE_THREADS ${args_USE_THREADS})
    set(USE_OPENMP ${args_USE_OPENMP})
    set(USE_PGC ${args_USE_PGC})
    set(USE_ADVANCED_ARCH_INTRINSICS ${args_USE_ADVANCED_ARCH_INTRINSICS})
    set(NO_INSTALL ${args_NO_INSTALL})

    # For armv8-a targets, we do not build any threading binaries:
    if(IS_ARM64_8A AND (USE_THREADS OR USE_OPENMP))
        return()
    endif()

    # Validate naming combination:
    if(AUTO_NAME AND NOT "${args_NAME}" STREQUAL "")
        message(FATAL_ERROR "AUTO_NAME and NAME cannot both be set")
    endif()

    # Validate target type:
    set(TARGET_TYPES app sharedlib objlib)
    if("${args_TYPE}" STREQUAL "")
        message(FATAL_ERROR "Must supply target type: TYPE {${TARGET_TYPES}}")
    elseif(NOT "${args_TYPE}" IN_LIST TARGET_TYPES)
        message(FATAL_ERROR "Unknown target type: ${args_TYPE}")
    endif()
    set(TARGET_TYPE "${args_TYPE}")

    # Set vars based on target type:
    set(IS_OBJLIB False)
    set(IS_SHAREDLIB False)
    set(IS_APP False)
    if("${TARGET_TYPE}" STREQUAL "objlib")
        set(IS_OBJLIB True)
    elseif("${TARGET_TYPE}" STREQUAL "sharedlib")
        set(IS_SHAREDLIB True)
    elseif("${TARGET_TYPE}" STREQUAL "app")
        set(IS_APP True)
    endif()

    # Validate threads vs openmp:
    if(USE_THREADS AND USE_OPENMP)
        message(FATAL_ERROR "Threads + OpenMP builds are not supported")
    endif()

    # Validate PGC vs threads/openmp:
    if(USE_PGC AND (USE_THREADS OR USE_OPENMP))
        message(FATAL_ERROR "Pedantic garbage collection (PGC) + Threads/OpenMP builds are not supported")
    endif()

    # Construct target name:
    # Note: autonames target if asked to or accepts a hardcoded name from caller
    set(TARGET_NAME_BASE)
    set(TARGET_NAME)
    if(NOT "${args_NAME}" STREQUAL "")
        set(TARGET_NAME_BASE "${args_NAME}")
        set(TARGET_NAME "${TARGET_NAME_BASE}")
    elseif(AUTO_NAME)
        set(TARGET_NAME_BASE "${PROJECT_NAME}")
        if(USE_THREADS)
            string(APPEND TARGET_NAME_BASE "-mt")
        elseif(USE_OPENMP)
            string(APPEND TARGET_NAME_BASE "-omp")
        elseif(USE_PGC)
            string(APPEND TARGET_NAME_BASE "-st-pgc")
        else()
            string(APPEND TARGET_NAME_BASE "-st")
        endif()
        set(TARGET_NAME "${TARGET_NAME_BASE}-${TARGET_TYPE}")
    endif()

    # Construct output name:
    # Note: accepts a hardcoded name from caller, otherwise
    #       it's the same as the target name (minus type).
    set(OUTPUT_NAME_BASE)
    if(NOT "${args_OUTPUT_NAME_BASE}" STREQUAL "")
        set(OUTPUT_NAME_BASE "${args_OUTPUT_NAME_BASE}")
    else()
        string(REPLACE "-${TARGET_TYPE}" "" OUTPUT_NAME_BASE "${TARGET_NAME}")
    endif()

    # For variants not supported, skip them:
    if(IS_WASM AND (IS_SHAREDLIB OR USE_THREADS OR USE_OPENMP))
        return()
    elseif(IS_OBJLIB AND NOT USE_OBJECT_LIBS)
        return()
    endif()

    # Create target:
    set(INSTALL_DIR)
    if(IS_OBJLIB)
        list(APPEND ALL_OBJLIB_TARGETS ${TARGET_NAME})
        set(ALL_OBJLIB_TARGETS ${ALL_OBJLIB_TARGETS} PARENT_SCOPE)

        add_library(${TARGET_NAME} OBJECT ${args_SOURCE})
    elseif(IS_SHAREDLIB)
        list(APPEND ALL_SHAREDLIB_TARGETS ${TARGET_NAME})
        set(ALL_SHAREDLIB_TARGETS ${ALL_SHAREDLIB_TARGETS} PARENT_SCOPE)

        if(USE_OBJECT_LIBS)
            string(REPLACE "-sharedlib" "-objlib" OBJ_LIB_TARGET_NAME ${TARGET_NAME})
            add_library(${TARGET_NAME} SHARED $<TARGET_OBJECTS:${OBJ_LIB_TARGET_NAME}>)
        else()
            add_library(${TARGET_NAME} SHARED ${args_SOURCE})
        endif()

        # On windows, add a hard dependency on the app of same type. This is needed since
        # the builds are parallelized and with the artifacts being named the same between apps
        # and shared libs (amalgam.exe vs amalgam.dll), there are intermediate files that get created
        # for both that can get clobbered or file system read/write errors (amalgam.exp, for example). So,
        # make sure they never run at the same time.
        if(IS_WINDOWS)
            string(REPLACE "-sharedlib" "-app" APP_TARGET_NAME ${TARGET_NAME})
            add_dependencies(${TARGET_NAME} ${APP_TARGET_NAME})
        endif()

        # Set install dir:
        set(INSTALL_DIR "lib")
    elseif(IS_APP)
        list(APPEND ALL_APP_TARGETS ${TARGET_NAME})
        set(ALL_APP_TARGETS ${ALL_APP_TARGETS} PARENT_SCOPE)

        if(USE_OBJECT_LIBS)
            string(REPLACE "-app" "-objlib" OBJ_LIB_TARGET_NAME ${TARGET_NAME})
            add_executable(${TARGET_NAME} ${args_APP_ONLY_SOURCE} $<TARGET_OBJECTS:${OBJ_LIB_TARGET_NAME}>)
        else()
            add_executable(${TARGET_NAME} ${args_APP_ONLY_SOURCE} ${args_SOURCE})
        endif()

        # Set install dir:
        set(INSTALL_DIR "bin")
    endif()


    #
    # Add options, properties, etc to targets:
    #

    # Include dirs:
    if(NOT "${args_INCLUDE_DIRS}" STREQUAL "")
        target_include_directories(${TARGET_NAME} PUBLIC ${args_INCLUDE_DIRS})
    endif()

    # Compiler defines:
    if(NOT "${args_COMPILER_DEFINES}" STREQUAL "")
        target_compile_definitions(${TARGET_NAME} PUBLIC ${args_COMPILER_DEFINES})
    endif()

    # Link libraries:
    if(NOT "${args_LINK_LIBRARIES}" STREQUAL "")
        target_link_libraries(${TARGET_NAME} PUBLIC ${args_LINK_LIBRARIES})
    endif()

    # Library define symbol:
    if(IS_SHAREDLIB)
        set_target_properties(${TARGET_NAME} PROPERTIES DEFINE_SYMBOL "AMALGAM_LIB_EXPORTS")
    endif()

    # Add file name define for resource files that use it:
    # Note: objlibs don't have output targets don't add them there
    if(NOT IS_OBJLIB)
        target_compile_definitions(${TARGET_NAME} PUBLIC AMALGAM_FILE_NAME="$<TARGET_FILE_NAME:${TARGET_NAME}>")
    endif()

    # Output name base:
    # Note: objlibs don't have output names
    if(NOT IS_OBJLIB)
        set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${OUTPUT_NAME_BASE}")
    endif()

    # Threads:
    if(USE_THREADS)
        target_compile_definitions(${TARGET_NAME} PUBLIC MULTITHREAD_SUPPORT)
    endif()

    # Unfortunately, this is needed for all Unix targets, not just multithreaded ones,
    # see: https://howardhinnant.github.io/date/tz.html#Installation
    # TODO 15993: Maybe when moving to C++20 and the official verision of the date library, this won't be needed
    #       for single threaded targets
    # TODO 15993: move to using official CMake Threads include when it works
    if(IS_UNIX AND NOT IS_WASM)
        target_compile_options(${TARGET_NAME} PUBLIC -pthread)
        target_link_libraries(${TARGET_NAME} PUBLIC pthread)
    endif()

    # OpenMP:
    # TODO 15993: move to using official CMake OpenMP include when it works
    if(USE_OPENMP)
        if(IS_MSVC)
            target_compile_options(${TARGET_NAME} PUBLIC /openmp)
        elseif(IS_GCC_FLAG_COMPAT_COMPILER)
            target_link_libraries(${TARGET_NAME} PUBLIC -lgomp)
        elseif(IS_APPLECLANG)
            # Look for libomp installed via Homebrew
            find_library(OMP_LIB libomp.a PATHS /usr/local/opt/libomp /opt/homebrew/opt/libomp PATH_SUFFIXES /lib DOC "Location of libomp" REQUIRED)
            find_path(OMP_INCLUDE omp.h PATHS /usr/local/opt/libomp /opt/homebrew/opt/libomp PATH_SUFFIXES /include REQUIRED)
            target_compile_options(${TARGET_NAME} PUBLIC -Xpreprocessor -fopenmp)
            target_include_directories(${TARGET_NAME} PUBLIC ${OMP_INCLUDE})
            target_link_libraries(${TARGET_NAME} PUBLIC ${OMP_LIB})
        endif()
    endif()

    # PGC:
    if(USE_PGC)
        target_compile_definitions(${TARGET_NAME} PUBLIC PEDANTIC_GARBAGE_COLLECTION)
    endif()

    # Advanced arch intrinsics:
    if(USE_ADVANCED_ARCH_INTRINSICS)
        if (IS_AMD64)
            set(INTRINSICS_FLAGS)
            if(IS_MSVC)
                set(INTRINSICS_FLAGS "/arch:${ADVANCED_INTRINSICS_AMD64}")
            elseif(IS_GCC_FLAG_COMPAT_COMPILER)
                set(INTRINSICS_FLAGS "-m${ADVANCED_INTRINSICS_AMD64}")
            elseif(IS_APPLECLANG)
                set(INTRINSICS_FLAGS "-march=core-${ADVANCED_INTRINSICS_AMD64}")
            endif()
            target_compile_options(${TARGET_NAME} PUBLIC ${INTRINSICS_FLAGS})
        endif()
    endif()

    # IDE folder:
    if(NOT "${args_IDE_FOLDER}" STREQUAL "")
        set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "${args_IDE_FOLDER}")
    endif()

    # Install:
    # Notes:
    #  1) objlibs can't be installed
    #  2) if not installing target, mark it as TEST_ONLY to be used for testing
    if(NOT IS_OBJLIB)
        if(NOT NO_INSTALL)
            install(TARGETS ${TARGET_NAME} DESTINATION "${INSTALL_DIR}" PERMISSIONS ${DEFAULT_INSTALL_PERMISSIONS})

            # Extra files to install for WASM
            if(IS_WASM)
                install(
                    FILES
                        "$<TARGET_FILE_DIR:${TARGET_NAME}>/$<TARGET_FILE_BASE_NAME:${TARGET_NAME}>.data"
                        "$<TARGET_FILE_DIR:${TARGET_NAME}>/$<TARGET_FILE_BASE_NAME:${TARGET_NAME}>.wasm"
                    DESTINATION "${INSTALL_DIR}"
                    PERMISSIONS ${DEFAULT_INSTALL_PERMISSIONS}
                )
                file(MAKE_DIRECTORY "out/config")
                set(WASM_DECLARATION_FILE "out/config/${TARGET_NAME_BASE}.d.cts")
                file(COPY_FILE "build/wasm/amalgam-wasm.d.cts" "${WASM_DECLARATION_FILE}" ONLY_IF_DIFFERENT)
                install(FILES "${WASM_DECLARATION_FILE}" DESTINATION "${INSTALL_DIR}" PERMISSIONS ${DEFAULT_INSTALL_PERMISSIONS})
            endif()
        endif()
    endif()

endfunction()
