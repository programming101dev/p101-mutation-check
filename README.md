# p101-mutation-check

`p101-mutation-check` asks a narrower question than coverage: would the tests
fail if an important decision in the C code were wrong?

It obtains exact mutation locations from `lib_c_facts`' native libclang
analysis, copies the project for every mutant, changes one expression, and
runs the test command directly without a shell. The original working tree is
never edited, and candidate discovery no longer launches a Python policy tool.

## Usage

    ./build-clang/p101-mutation-check \
        --compile-db build-clang/compile_commands.json \
        --max-mutants 50 \
        . -- ./test.sh

List candidates without running tests:

    ./build-clang/p101-mutation-check --compile-db build-clang/compile_commands.json --list .

Combine `--list --json` to emit the reusable
`p101-mutation-candidates-v2` candidate document instead of text.

Use `--json` for the common finding envelope. Exit status is `0` when the
baseline and every selected mutant are killed, `1` when one or more mutants
survive, and `2` for parser, baseline, timeout, or tool trouble.

## Contract

Admitted input:

- active translation units in one `compile_commands.json`;
- Clang-located candidates produced directly through `lib_c_facts`;
- a test command supplied as an argument vector after `--`.

The operators are intentionally focused and reviewable:

- comparison-boundary changes such as `<` to `<=`;
- logical-connective changes between `&&` and `||`;
- arithmetic changes between `+` and `-`;
- inversion of `p101_error_has_error` and `p101_error_has_no_error`;
- replacement of a p101 cleanup operation with a no-op. Cleanup includes
  close, free, unmap, destroy, release, unlock, join, detach, and error-reset
  operations.

Each surviving mutant is `P101-MUTATION-001` and includes its operator, source
location, and replacement. The JSON summary records killed, survived,
inconclusive, and selected counts.

## Blind spots

This is focused mutation testing, not proof of test correctness. It does not
mutate every expression, reason about equivalent mutants, explore concurrency
schedules, or inspect third-party code. A killed mutant only proves that this
test command rejected this one edit. Candidate discovery is limited to active
translation units that Clang parsed successfully.

The native implementation depends on libclang. The workspace setup installs
the matching development package on Linux and the LLVM package on macOS and
FreeBSD.

## Evidence

    ./test.sh
    ./check.sh
