# template-c-program

`template-c-program` is a small C program driven by a finite-state machine — it validates a required `-d <delay>` argument, then runs a simple FSM. Like every Programming 101 template it ships with the full
quality toolchain already wired in — a strict analysis build, the sanitizers,
unit tests, a fuzzer, and coverage — so any project you start from it is
correct-by-construction from the first commit. `commands.md` is the one-line
reference for every script; this file is the walkthrough.

## Quick start

Configure a compiler once, then run the gate:

    ./change-compiler.sh -c clang     # pick the compiler and configure the build
    ./check.sh                       # format + strict build + tests + fuzz smoke -> one PASS/FAIL

`./change-compiler.sh --help` lists the compilers detected on this machine.

## The workflow

1. **Configure** — `./change-compiler.sh -c clang` picks the compiler and
   configures the build. Run it again any time to switch compilers (e.g.
   `./change-compiler.sh -c gcc`).
2. **Build** — `./build.sh` compiles through the strict analysis pipeline:
   clang-format check, clang-tidy, cppcheck, the Clang static analyzer,
   hundreds of warnings under `-Werror`, and the sanitizers baked in. Add `-q`
   to hide the per-file command dump.
3. **Test** — `./test.sh` builds and runs the Unity test suite; `./test-all.sh`
   runs it across every supported compiler.
4. **Check** — `./check.sh` is the one command to run before you submit: it does
   the format check, the strict build, the tests, and a short fuzz smoke run,
   then prints a single PASS/FAIL and exits non-zero on any failure. Add
   `--cov <pct>` to also fail when test coverage is below a threshold.
5. **Fuzz** — `./fuzz.sh` runs the libFuzzer target (coverage-guided, sanitizers
   on) and prints PASS/FAIL. Here it fuzzes the program's own argument parser (`parse_arguments`), so it exercises real code you can break.
6. **Coverage** — `./coverage-report.sh` builds an HTML coverage report; add
   `--min <pct>` to fail below a threshold.
7. **Diagnose** — when something looks wrong, `./doctor.sh` reports what actually
   works on this machine for this project.

## Formatting

    ./build.sh -f     # apply clang-tidy --fix + clang-format, in place
    ./build.sh -C     # check formatting only, no build (non-zero if unclean)

## Adding or removing files

This template uses a fixed strict `CMakeLists.txt` driven by `config.cmake` —
there is no generated makefile to edit. When you add or remove a source or
header, edit the lists in `config.cmake` (`main_SOURCES`, `main_HEADERS`, and
`main_LINK_LIBRARIES` for libraries), then re-configure and build:

    ./change-compiler.sh -c clang
    ./build.sh

## Start a new project from this template

    ./copy-template.sh <destination-directory>

This copies everything you need — the sources, the build system, and every
script — into a fresh project directory.

## Prerequisites

The Programming 101 setup scripts install everything these tools need. If a
script reports a missing tool, run `./doctor.sh` to see exactly what this
project can and can't do on your machine, and re-run the setup if needed.
