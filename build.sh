#!/usr/bin/env bash
# build.sh — build the last configured CMake build directory
# Supports: macOS, modern Linux, modern FreeBSD
set -euo pipefail

# QoL: -q / --quiet hides the per-file compile-command dump.
_P101_VERBOSE=1
_p101_bq=()
for _p101_bqa in "$@"; do
  case "$_p101_bqa" in
    -q|--quiet) export P101_QUIET=1; _P101_VERBOSE= ;;
    *) _p101_bq+=("$_p101_bqa") ;;
  esac
done
if ((${#_p101_bq[@]})); then set -- "${_p101_bq[@]}"; else set --; fi
unset _p101_bq _p101_bqa


# ----------------- defaults -----------------
jobs="${JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-}}"
target=""
build_dir=""

usage() {
  echo "Usage: $0 [-j N] [-t <target>] [-q]"
  echo "  -j N        Parallel build with N jobs (or set JOBS / CMAKE_BUILD_PARALLEL_LEVEL)"
  echo "  -t target   Build a specific target (e.g. -t main)"
  echo "  -q          Quiet: hide the per-file compile-command dump"
  exit 1
}

# --help / -h -> usage, exit 0 (P101 uniform CLI help)
case " $* " in *" --help "*|*" -h "*) ( usage ) || true; exit 0 ;; esac

# ----------------- parse options -----------------
while getopts ":j:t:h" opt; do
  case "$opt" in
    j) jobs="$OPTARG" ;;
    t) target="$OPTARG" ;;
    h|*) usage ;;
  esac
done

# ----------------- determine build dir -----------------
# Preferred: read the last configured build dir written by change-compiler.sh
if [[ -f ".last-build-dir" ]]; then
  build_dir="$(< .last-build-dir)"
else
  # Fallback for legacy/manual setups
  build_dir="build"
fi

# ----------------- sanity checks -----------------
if [[ ! -d "$build_dir" || ! -f "$build_dir/CMakeCache.txt" ]]; then
  echo "Error: build directory '$build_dir' is not configured." >&2
  echo "Run ./change-compiler.sh first." >&2
  exit 1
fi

# ----------------- assemble build command -----------------
cmd=(cmake --build "$build_dir" --clean-first ${_P101_VERBOSE:+--verbose})
[[ -n "$target" ]] && cmd+=(--target "$target")
[[ -n "$jobs" ]] && cmd+=(--parallel "$jobs")

# ----------------- run -----------------
echo "Using build directory: $build_dir"
echo "Running: ${cmd[*]}"
exec "${cmd[@]}"
