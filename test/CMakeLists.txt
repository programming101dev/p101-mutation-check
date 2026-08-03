cmake_minimum_required(VERSION 3.14)
project(p101_mutation_check_tests C)
enable_testing()

option(P101_TEST_COVERAGE "Instrument the code under test" OFF)
set(_COV "")
if(P101_TEST_COVERAGE)
    set(_COV --coverage)
endif()

set(P101_PUBLIC_INCLUDE_DIRS "" CACHE STRING "Extra p101 include dirs")
set(P101_PUBLIC_LINK_DIRS "" CACHE STRING "Extra p101 link dirs")
separate_arguments(P101_PUBLIC_INCLUDE_DIRS_LIST NATIVE_COMMAND "${P101_PUBLIC_INCLUDE_DIRS}")
separate_arguments(P101_PUBLIC_LINK_DIRS_LIST NATIVE_COMMAND "${P101_PUBLIC_LINK_DIRS}")
set(_P101_INC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../include" ${P101_PUBLIC_INCLUDE_DIRS_LIST} /usr/local/include /opt/homebrew/include /opt/local/include)
set(_P101_LIB_DIRS ${P101_PUBLIC_LINK_DIRS_LIST} /usr/local/lib /usr/local/lib64 /opt/homebrew/lib /opt/local/lib)

set(P101_LIBS
        p101_error p101_env p101_record p101_tool_event p101_c p101_c_facts
        p101_filesystem p101_io p101_process p101_time)
set(_P101_RESOLVED "")
foreach(_library IN LISTS P101_LIBS)
    unset(_P101_LIBRARY CACHE)
    unset(_P101_LIBRARY)
    find_library(_P101_LIBRARY NAMES ${_library} PATHS ${_P101_LIB_DIRS} NO_DEFAULT_PATH)
    if(NOT _P101_LIBRARY)
        find_library(_P101_LIBRARY NAMES ${_library})
    endif()
    if(NOT _P101_LIBRARY)
        message(FATAL_ERROR "Required p101 library not found: ${_library}")
    endif()
    list(APPEND _P101_RESOLVED "${_P101_LIBRARY}")
endforeach()

add_executable(p101_mutation_check_test_cli
        ../src/candidates.c
        ../src/cli.c
        ../src/execution.c
        ../src/files.c
        ../src/main.c
        ../src/output.c)
target_include_directories(p101_mutation_check_test_cli PRIVATE ${_P101_INC_DIRS})
target_link_libraries(p101_mutation_check_test_cli PRIVATE ${_P101_RESOLVED} m)
target_compile_options(p101_mutation_check_test_cli PRIVATE ${_COV})
target_link_options(p101_mutation_check_test_cli PRIVATE ${_COV})

add_test(
        NAME mutation_check_cli
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.sh"
                "$<TARGET_FILE:p101_mutation_check_test_cli>"
                "${CMAKE_C_COMPILER}")
