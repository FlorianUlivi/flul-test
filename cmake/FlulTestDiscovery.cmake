execute_process(
    COMMAND "${TEST_EXECUTABLE}" --list
    OUTPUT_VARIABLE output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "Test discovery failed for '${TEST_EXECUTABLE}' (exit code ${result})")
endif()

if(output STREQUAL "")
    file(WRITE "${CTEST_FILE}" "")
    return()
endif()

string(REPLACE "\n" ";" test_list "${output}")

file(WRITE "${CTEST_FILE}" "")
foreach(test IN LISTS test_list)
    if(NOT test STREQUAL "")
        file(APPEND "${CTEST_FILE}"
            "add_test(\"${test}\" \"${TEST_EXECUTABLE}\" \"--filter\" \"${test}\")\n"
        )
    endif()
endforeach()
