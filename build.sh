#!/usr/bin/env bash
set -euo pipefail
CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-${TMPDIR:-/tmp}/p101-mutation-check-pycache}"
python3 -m py_compile p101-mutation-check
echo "p101-mutation-check build passed"
