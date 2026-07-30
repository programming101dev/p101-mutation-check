# Commands

| Command | What it does |
| --- | --- |
| `./p101-mutation-check --list <project>` | List exact source mutations without running tests. |
| `./p101-mutation-check <project> -- <test command>` | Run the baseline, then require the test command to kill every selected mutant. |
| `./p101-mutation-check --json <project> -- <test command>` | Emit the same result as deterministic JSON. |
| `./build.sh` | Syntax-check the Python entry point. |
| `./test.sh` | Run the isolated regression fixture. |
| `./check.sh` | Run both build and regression receipts. |

Use `--max`, `--kind`, and `--timeout` to bound an exercise. The test command
is passed as an argument vector and is never interpreted by a shell.
