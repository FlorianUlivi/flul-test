# LLVM source-based coverage instrumentation.
# Activated when FLUL_COVERAGE=ON.

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "Coverage requires a Clang compiler. "
        "Current compiler: ${CMAKE_CXX_COMPILER_ID}"
    )
endif()

function(enable_coverage_for_target target_name)
    target_compile_options(${target_name} PRIVATE
        -fprofile-instr-generate
        -fcoverage-mapping
    )
    target_link_options(${target_name} PRIVATE
        -fprofile-instr-generate
    )
endfunction()
