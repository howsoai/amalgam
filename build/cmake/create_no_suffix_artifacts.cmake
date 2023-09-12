#
# No suffix default targets
#

# Don't create defaut targets for WASM
if(NOT IS_WASM)
    # No suffix app target:
    set(NO_SUFFIX_APP_TARGET "${PROJECT_NAME}-mt-app")
    if(IS_ARM64_8A)
        # No mt target on arm64 8-a so default is based off st.
        set(NO_SUFFIX_APP_TARGET "${PROJECT_NAME}-st-app")
    endif()
    set(NO_SUFFIX_APP_NAME "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    add_custom_target("create-no-suffix-app" ALL
        COMMENT "Creating default named app"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${NO_SUFFIX_APP_TARGET}>" "${NO_SUFFIX_APP_NAME}"
        BYPRODUCTS ${NO_SUFFIX_APP_NAME}
    )
    set_target_properties("create-no-suffix-app" PROPERTIES FOLDER "OtherBuildTargets")
    install(FILES ${NO_SUFFIX_APP_NAME} DESTINATION bin PERMISSIONS ${DEFAULT_INSTALL_PERMISSIONS})

    # No suffix sharedlib target:
    set(NO_SUFFIX_LIB_TARGET "${PROJECT_NAME}-mt-sharedlib")
    if(IS_ARM64_8A)
        # No mt target on arm64 8-a so default is based off st.
        set(NO_SUFFIX_LIB_TARGET "${PROJECT_NAME}-st-sharedlib")
    endif()
    set(NO_SUFFIX_LIB_NAME "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    add_custom_target("create-no-suffix-sharedlib" ALL
        COMMENT "Creating default named lib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${NO_SUFFIX_LIB_TARGET}>" "${NO_SUFFIX_LIB_NAME}"
        BYPRODUCTS ${NO_SUFFIX_LIB_NAME}
    )
    set_target_properties("create-no-suffix-sharedlib" PROPERTIES FOLDER "OtherBuildTargets")
    install(FILES ${NO_SUFFIX_LIB_NAME} DESTINATION lib PERMISSIONS ${DEFAULT_INSTALL_PERMISSIONS})
    if(IS_WINDOWS)
        set(NO_SUFFIX_LINKER_LIB_NAME "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}amalgam${CMAKE_STATIC_LIBRARY_SUFFIX}")
        add_custom_target("create-no-suffix-linkinglib" ALL
            COMMENT "Creating default named linking lib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_LINKER_FILE:${NO_SUFFIX_LIB_TARGET}>" "${NO_SUFFIX_LINKER_LIB_NAME}"
            BYPRODUCTS ${NO_SUFFIX_LINKER_LIB_NAME}
        )
        set_target_properties("create-no-suffix-linkinglib" PROPERTIES FOLDER "OtherBuildTargets")
        install(FILES ${NO_SUFFIX_LINKER_LIB_NAME} DESTINATION lib PERMISSIONS ${DEFAULT_INSTALL_PERMISSIONS})
    endif()
endif()
