#!/usr/bin/env bash
set -euo pipefail
CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-${TMPDIR:-/tmp}/p101-mutation-check-pycache}"
trap 'find . -maxdepth 1 -name ".coverage*" -delete' EXIT
find . -maxdepth 1 -name '.coverage*' -delete
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage run --parallel-mode test/test_mutation_check.py
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage run --parallel-mode test/test_mutation_check_unit.py
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage combine
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage report --include="$PWD/p101-mutation-check"
