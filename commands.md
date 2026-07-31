# Commands

| Command | What it does |
| --- | --- |
| `./build-clang/p101-mutation-check --compile-db <db> --list <project>` | List exact source mutations without running tests. |
| `./build-clang/p101-mutation-check --compile-db <db> <project> -- <test command>` | Run the baseline, then require the test command to kill every selected mutant. |
| `./build-clang/p101-mutation-check --json --compile-db <db> <project> -- <test command>` | Emit the same result as deterministic JSON. |
| `./build.sh` | Build the native C executable through the strict analysis pipeline. |
| `./test.sh` | Run the isolated regression fixture. |
| `./check.sh` | Run both build and regression receipts. |

Use `--max-mutants`, `--operator`, and `--timeout` to bound an exercise. The test command
is passed as an argument vector and is never interpreted by a shell.
