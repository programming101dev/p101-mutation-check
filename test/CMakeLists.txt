# Standalone Unity test tree for a p101-linked C program. Kept separate from the
# project's strict analysis build. Links the same p101 libraries the main target
# links (see ../config.cmake main_LINK_LIBRARIES), resolved from the usual
# install prefixes; add to _P101_LIB_DIRS/_P101_INC_DIRS if yours differ.
cmake_minimum_required(VERSION 3.14)
project(p101_tests C)
enable_testing()

add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/unity")
target_compile_options(unity PRIVATE -w)

option(P101_TEST_COVERAGE "Instrument the code under test" OFF)
set(_COV "")
if (P101_TEST_COVERAGE)
    set(_COV --coverage)
endif ()

set(P101_LIBS p101_error p101_env p101_c p101_posix p101_unix p101_fsm p101_convert)
set(_P101_INC_DIRS /usr/local/include /opt/homebrew/include /opt/local/include)
set(_P101_LIB_DIRS /usr/local/lib /usr/local/lib64 /opt/homebrew/lib /opt/local/lib)

set(_P101_RESOLVED "")
foreach (_l IN LISTS P101_LIBS)
    find_library(_P101_LIB_${_l} NAMES ${_l} PATHS ${_P101_LIB_DIRS})
    if (_P101_LIB_${_l})
        list(APPEND _P101_RESOLVED "${_P101_LIB_${_l}}")
    else ()
        message(WARNING "p101 library '${_l}' not found in ${_P101_LIB_DIRS} — install the p101 libs (setup.sh) or add the path.")
    endif ()
endforeach ()

# p101_add_test(<name> <sources...>) — pass the test source plus any of the
# project's own sources it exercises (NOT main.c, which defines main()).
function(p101_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${_P101_INC_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    target_link_libraries(${name} PRIVATE unity ${_P101_RESOLVED} m)
    target_compile_options(${name} PRIVATE ${_COV})
    target_link_options(${name} PRIVATE ${_COV})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Tests THIS project's own code: compiles src/fsm.c and exercises run_fsm().
p101_add_test(test_fsm test_fsm.c "${CMAKE_CURRENT_SOURCE_DIR}/../src/fsm.c")

# Example of testing a p101 wrapper this project links (no project code needed).
p101_add_test(test_p101_string test_p101_string.c)
