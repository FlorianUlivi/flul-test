function(flul_test_discover TARGET)
    set(ctest_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_tests.cmake")

    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        BYPRODUCTS "${ctest_file}"
        COMMAND "${CMAKE_COMMAND}"
            -D "TEST_EXECUTABLE=$<TARGET_FILE:${TARGET}>"
            -D "CTEST_FILE=${ctest_file}"
            -P "${PROJECT_SOURCE_DIR}/cmake/FlulTestDiscovery.cmake"
        VERBATIM
    )

    set_property(DIRECTORY
        APPEND PROPERTY TEST_INCLUDE_FILES "${ctest_file}"
    )
endfunction()
