# Set comprehensive compiler warnings for the project

function(set_project_warnings target_name)
    set(COMMON_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
    )

    set(GCC_WARNINGS
        ${COMMON_WARNINGS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wnull-dereference
        -Wuseless-cast
    )

    set(CLANG_WARNINGS
        ${COMMON_WARNINGS}
        -Wshadow-all
        -Wimplicit-fallthrough
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS ${GCC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(PROJECT_WARNINGS ${CLANG_WARNINGS})
    else()
        set(PROJECT_WARNINGS ${COMMON_WARNINGS})
    endif()

    target_compile_options(${target_name} PRIVATE ${PROJECT_WARNINGS})
endfunction()

# Apply warnings globally to all targets created after this point
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wnull-dereference
        -Wuseless-cast
        -Wdouble-promotion
        -Wformat=2
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wshadow-all
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wimplicit-fallthrough
        -Wdouble-promotion
        -Wformat=2
    )
endif()
