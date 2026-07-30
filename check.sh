#!/usr/bin/env bash
set -euo pipefail
CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
./build.sh
./test.sh
echo "p101-mutation-check check passed"
