#
# Tests
#

enable_testing()

# CTest args:
set(CMAKE_CTEST_ARGUMENTS "-j 1" "--schedule-random" "--output-on-failure" "--extra-verbose" "--output-log" "${CMAKE_SOURCE_DIR}/out/test/all_tests.log")

# Not all tests can be run on all platforms:
# Note: macOS builds arm64 by setting CMAKE_OSX_ARCHITECTURES rather than by changing the
# system name, so CMAKE_CROSSCOMPILING is not set for it; compare against the host
# processor instead to tell a native arm64 build from one cross compiled on an amd64 host.
if(IS_WASM)
    # No tests to run for WASM
    list(PREPEND CMAKE_CTEST_ARGUMENTS "-LE" ".*")
elseif(IS_MACOS AND IS_ARM64 AND NOT "${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "arm64")
    # Can't run cross compiled arm64 binaries on macos amd64 hosts
    list(PREPEND CMAKE_CTEST_ARGUMENTS "-LE" ".*")
elseif(IS_MACOS AND NOT IS_ARM64)
    # Can't run advanced intrinsic builds on macos amd64 build machines
    list(PREPEND CMAKE_CTEST_ARGUMENTS "-LE" "advanced_intrinsics")
else()
    list(PREPEND CMAKE_CTEST_ARGUMENTS "-L" "smoke_test")
endif()

# Test runner for platforms that need it:
# Note: arm64 Linux is built and tested natively on arm64 runners, so no emulator is needed.
set(TEST_RUNNER)

# Create tests for every app target:
set(ALL_TEST_TARGETS)
set(TEST_OUTPUT_LOG_BASE "${CMAKE_INSTALL_PREFIX}/../../test")
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_workspace)
foreach(TEST_TARGET ${ALL_APP_TARGETS})

    message(STATUS "GLIBC version: ${GLIBC_VERSION}")
    message(STATUS "TEST_TARGET: ${TEST_TARGET}")

    # GLIBC 2.28 version doesn't need to test all targets
    if("${GLIBC_VERSION}" STREQUAL "2.28")
        if(NOT "${TEST_TARGET}" STREQUAL "amalgam-mt-app" AND NOT "${TEST_TARGET}" STREQUAL "amalgam-st-app") 
            continue()
        endif()
    endif()

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
        COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_TARGET}>" --validate-amalgam
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test_workspace
    )
    list(APPEND ALL_TEST_TARGETS ${TEST_NAME})

endforeach()

# Create tests for every lib target:
foreach(TEST_TARGET ${ALL_SHAREDLIB_TARGETS})

    # Create test exe:
    set(TEST_EXE_NAME "${TEST_TARGET}-tester")
    set(TEST_SOURCES "test/lib_smoke_test/main.cpp" "test/lib_smoke_test/test.amlg" "test/lib_smoke_test/counter.amlg" "test/lib_smoke_test/cluster.amlg" "test/unit_test/clustering_test.cpp")
    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${TEST_SOURCES})
    add_executable(${TEST_EXE_NAME} ${TEST_SOURCES})
    set_target_properties(${TEST_EXE_NAME} PROPERTIES FOLDER "Testing")
    target_include_directories(${TEST_EXE_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/test/unit_test")
    target_link_libraries(${TEST_EXE_NAME} ${TEST_TARGET})

    # Test for test exe:
    set(TEST_NAME "Lib.SmokeTest.${TEST_EXE_NAME}")
    add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TEST_EXE_NAME}>"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test/lib_smoke_test
    )
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

# Scheduler probes use production Concurrency.cpp without rebuilding the Interpreter.
# Keep smoke_test so the existing native CI test filter includes them.
if(TARGET amalgam-mt-app AND NOT IS_WASM)
    add_executable(amalgam-concurrency-test
        test/unit_test/concurrency_test.cpp src/Amalgam/Concurrency.cpp)
    target_compile_definitions(amalgam-concurrency-test PRIVATE MULTITHREAD_SUPPORT)
    target_link_libraries(amalgam-concurrency-test PRIVATE amalgam-taskflow)
    set_target_properties(amalgam-concurrency-test PROPERTIES FOLDER "Testing")
    add_test(NAME Concurrency.Taskflow COMMAND amalgam-concurrency-test)
    set_tests_properties(Concurrency.Taskflow PROPERTIES LABELS "smoke_test;concurrency" TIMEOUT 120)
    set(INTERPRETER_CONCURRENCY_LABELS "smoke_test;concurrency")
    if(IS_AMD64)
        list(APPEND INTERPRETER_CONCURRENCY_LABELS "advanced_intrinsics")
    endif()
    foreach(WORKERS 1 2 4)
        add_test(NAME Concurrency.Interpreter.${WORKERS}
            COMMAND $<TARGET_FILE:amalgam-mt-app> --numthreads ${WORKERS}
                ${CMAKE_SOURCE_DIR}/test/concurrency/nested.amlg
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test_workspace)
        set_tests_properties(Concurrency.Interpreter.${WORKERS} PROPERTIES
            LABELS "${INTERPRETER_CONCURRENCY_LABELS}" TIMEOUT 120
            PASS_REGULAR_EXPRESSION "^[.]true" FAIL_REGULAR_EXPRESSION "false")
        add_test(NAME Concurrency.Convictions.${WORKERS}
            COMMAND $<TARGET_FILE:amalgam-mt-app> --numthreads ${WORKERS}
                ${CMAKE_SOURCE_DIR}/test/concurrency/convictions.amlg
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test_workspace)
        set_tests_properties(Concurrency.Convictions.${WORKERS} PROPERTIES
            LABELS "${INTERPRETER_CONCURRENCY_LABELS}" TIMEOUT 120
            PASS_REGULAR_EXPRESSION "^[.]true" FAIL_REGULAR_EXPRESSION "false")
    endforeach()
    add_custom_target(concurrency-stress
        COMMAND $<TARGET_FILE:amalgam-concurrency-test> --stress
        DEPENDS amalgam-concurrency-test USES_TERMINAL)
endif()
