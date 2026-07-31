set(PROJECT_NAME "p101-mutation-check")
set(PROJECT_VERSION "2.0.0")
set(PROJECT_DESCRIPTION "Programming 101 native C mutation test checker")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)

set(DARWIN_STANDARD_FLAGS
        -D_DARWIN_C_SOURCE
)

set(LINUX_STANDARD_FLAGS)
set(BSD_STANDARD_FLAGS)

set(EXECUTABLE_TARGETS main)
set(LIBRARY_TARGETS "")
set(main_OUTPUT_NAME p101-mutation-check)

set(main_SOURCES
        src/candidates.c
        src/cli.c
        src/execution.c
        src/files.c
        src/main.c
        src/output.c
)

set(main_HEADERS
        include/mutation_check.h
)

set(main_LINK_LIBRARIES
        p101_error
        p101_env
        p101_tool_event
        p101_c
        p101_c_facts
        p101_filesystem
        p101_io
        p101_process
        p101_time
        m
)
