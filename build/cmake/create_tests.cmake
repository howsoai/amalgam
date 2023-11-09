#
# Tests
#

enable_testing()
#find_package(GTest REQUIRED)
#include(GoogleTest)

# CTest args:
set(CMAKE_CTEST_ARGUMENTS "-j" "--schedule-random" "--output-on-failure" "--output-log" "${CMAKE_SOURCE_DIR}/out/test/all_tests.log")

# Not all tests can be run on all platforms:
if(IS_MACOS)
    if(IS_ARM64)
        # Can't run cross compiled arm64 binaries on macos amd64 hosts
        list(PREPEND CMAKE_CTEST_ARGUMENTS "-LE" ".*")
    else()
        # Can't run advanced intrinsic builds on macos amd64 build machines
        list(PREPEND CMAKE_CTEST_ARGUMENTS "-LE" "advanced_intrinsics")
    endif()
elseif(IS_WASM)
    # No tests to run for WASM
    list(PREPEND CMAKE_CTEST_ARGUMENTS "-LE" ".*")
else()
    list(PREPEND CMAKE_CTEST_ARGUMENTS "-L" "smoke_test")
endif()

# Test runner for platforms that need it:
set(TEST_RUNNER)
if(IS_LINUX AND IS_ARM64)
    set(TEST_RUNNER "qemu-aarch64" "-L" "${ARM64_LIB_DIR}")
endif()

# Create tests for every app target:
set(ALL_TEST_TARGETS)
set(TEST_OUTPUT_LOG_BASE "${CMAKE_INSTALL_PREFIX}/../../test")
foreach(TEST_TARGET ${ALL_APP_TARGETS})

    # No args test:
    set(TEST_NAME "App.NoArgs.${TEST_TARGET}_noargs")
    set(TEST_OUTPUT_LOG "${TEST_OUTPUT_LOG_BASE}/out.${TEST_NAME}.txt")
    add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_TARGET}>"
    )
    list(APPEND ALL_TEST_TARGETS ${TEST_NAME})

    # Version test:
    set(TEST_NAME "App.Version.${TEST_TARGET}_getversion")
    set(TEST_OUTPUT_LOG "${TEST_OUTPUT_LOG_BASE}/out.${TEST_NAME}.txt")
    add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_TARGET}>" --version
    )
    set_tests_properties(${TEST_NAME} PROPERTIES PASS_REGULAR_EXPRESSION "${AMALGAM_VERSION_FULL_ESCAPED}")
    list(APPEND ALL_TEST_TARGETS ${TEST_NAME})

    # Full test:
    set(TEST_NAME "App.FullTest.${TEST_TARGET}_fulltests")
    set(TEST_OUTPUT_LOG "${TEST_OUTPUT_LOG_BASE}/out.${TEST_NAME}.txt")
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_TARGET}>" -l ${TEST_OUTPUT_LOG} amlg_code/full_test.amlg
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/Amalgam
    )
    set_tests_properties(${TEST_NAME} PROPERTIES PASS_REGULAR_EXPRESSION "--total execution time--")
    list(APPEND ALL_TEST_TARGETS ${TEST_NAME})

    # Unit tests (using REPL)
    if(NOT IS_WASM)
        set(TEST_EXE_NAME "${TEST_TARGET}-tester")
        set(TEST_SOURCES "test/interpreter_unit_tests/main.cpp" "test/3rd_party/subprocess_h/subprocess.h")
        source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${TEST_SOURCES})
        add_executable(${TEST_EXE_NAME} ${TEST_SOURCES})
        set_target_properties(${TEST_EXE_NAME} PROPERTIES FOLDER "Testing")
        target_include_directories(${TEST_EXE_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/test/3rd_party")

        set(TEST_NAME "App.UnitTests.${TEST_EXE_NAME}")
        set(TEST_OUTPUT_LOG "${TEST_OUTPUT_LOG_BASE}/out.${TEST_NAME}.txt")
        add_test(
            NAME ${TEST_NAME}
            COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_TARGET}>" -l ${TEST_OUTPUT_LOG} "$<TARGET_FILE_NAME:${TEST_TARGET}>"
        )
        list(APPEND ALL_TEST_TARGETS ${TEST_NAME})
    endif()

endforeach()

# Create tests for every lib target:
foreach(TEST_TARGET ${ALL_SHAREDLIB_TARGETS})

    # Create test exe:
    set(TEST_EXE_NAME "${TEST_TARGET}-tester")
    set(TEST_SOURCES "test/lib_smoke_test/main.cpp" "test/lib_smoke_test/test.amlg")
    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${TEST_SOURCES})
    add_executable(${TEST_EXE_NAME} ${TEST_SOURCES})
    set_target_properties(${TEST_EXE_NAME} PROPERTIES FOLDER "Testing")
    target_link_libraries(${TEST_EXE_NAME} ${TEST_TARGET})

    # Test for test exe:
    set(TEST_NAME "Lib.SmokeTest.${TEST_EXE_NAME}")
    add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_EXE_NAME}>" test.amlg
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test/lib_smoke_test
    )
    set_tests_properties(${TEST_NAME} PROPERTIES PASS_REGULAR_EXPRESSION "${AMALGAM_VERSION_FULL_ESCAPED}")
    list(APPEND ALL_TEST_TARGETS ${TEST_NAME})

endforeach()

# Add common test labels:
foreach(TEST_TARGET ${ALL_TEST_TARGETS})
    set(TEST_LABELS smoke_test)
    if(IS_AMD64 AND NOT "${TEST_TARGET}" MATCHES "${NO_ADVANCED_INTRINSICS_AMD64_SUFFIX}")
        list(APPEND TEST_LABELS "advanced_intrinsics")
    endif()
    set_tests_properties(${TEST_TARGET} PROPERTIES LABELS "${TEST_LABELS}")
endforeach()
