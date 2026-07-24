# Standalone libFuzzer tree. clang-only (needs the fuzzer runtime) -- fuzz.sh
# selects a fuzzer-capable clang and passes it as CMAKE_C_COMPILER. Separate
# from the strict analysis build.
#
# TARGET: the template's OWN argument parser (src/main.c: parse_arguments()).
# fuzz_parse.c #includes ../src/main.c so the static parser is visible AND gets
# compiled with -fsanitize=fuzzer (=> coverage-guided). We rename the program's
# main() aside and redirect exit() into a longjmp (usage() is _Noreturn) via the
# two -D defines below, so nothing in src/ has to change. src/fsm.c is compiled
# in only so the (unused) renamed main() resolves run_fsm() at link time.
cmake_minimum_required(VERSION 3.14)
project(p101_fuzz C)

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzing needs clang with libFuzzer (-fsanitize=fuzzer); got '${CMAKE_C_COMPILER_ID}'. Use ./fuzz.sh, which picks a fuzzer-capable clang.")
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
        message(WARNING "p101 library '${_l}' not found in ${_P101_LIB_DIRS} -- install the p101 libs (setup.sh) or add the path.")
    endif ()
endforeach ()

# -fsanitize=fuzzer pulls in libFuzzer (and its own main); +address,undefined so
# the fuzzer's inputs are checked for memory errors and UB.
set(_FUZZ_FLAGS -fsanitize=fuzzer,address,undefined -g -O1)

add_executable(fuzz_parse fuzz_parse.c "${CMAKE_CURRENT_SOURCE_DIR}/../src/fsm.c")

# Reach the static parser without editing src/: rename the program's main() out
# of libFuzzer's way, and redirect exit() (usage() is _Noreturn) so the -h path
# is a longjmp, not a fake crash.
target_compile_definitions(fuzz_parse PRIVATE main=p101_unused_main exit=p101_fuzz_exit)

target_include_directories(fuzz_parse PRIVATE ${_P101_INC_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_compile_options(fuzz_parse PRIVATE ${_FUZZ_FLAGS})
target_link_options(fuzz_parse PRIVATE ${_FUZZ_FLAGS})
target_link_libraries(fuzz_parse PRIVATE ${_P101_RESOLVED} m)
