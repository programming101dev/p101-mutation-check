#!/usr/bin/env bash
# coverage-report.sh — run the built target(s) and produce + open an HTML code
# coverage report (gcovr), using the gcov that matches the configured compiler.
# Platforms: macOS, Linux, FreeBSD.
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

usage() {
  cat <<'USAGE'
Usage: ./coverage-report.sh [--report-only] [-- <program args>]
  Runs the executable target(s) from the last coverage build to generate the
  .gcda counters, then builds coverage-<compiler>/index.html with gcovr (line +
  decision coverage) and opens it.

  --report-only (-R)  don't run; just report the .gcda already accumulated
                      (e.g. after driving the program yourself several times).

  Requires a coverage build first:
    ./change-compiler.sh -c <cc> --coverage && ./build.sh
  Pass runtime arguments to your program after --, e.g.:
    ./coverage-report.sh -- -v -d 1
  .gcda counters accumulate across runs until the next ./build.sh, so:
    ./build-<cc>/main --mode a
    ./build-<cc>/main --mode b
    ./coverage-report.sh --report-only     # union of both runs

  gcovr is required:  brew install gcovr  (macOS)  |  pipx install gcovr
USAGE
}
case " $* " in *" --help "*|*" -h "*) usage; exit 0 ;; esac

# options, then program args after a literal --
report_only=0
prog_args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --report-only|-R) report_only=1; shift ;;
    --)               shift; prog_args=("$@"); break ;;
    *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
  esac
done

build_dir="build"
[[ -f .last-build-dir ]] && build_dir="$(cat .last-build-dir)"
[[ -d "$build_dir" && -f "$build_dir/CMakeCache.txt" ]] || {
  echo "No configured build dir ('$build_dir'). Run:" >&2
  echo "  ./change-compiler.sh -c <cc> --coverage && ./build.sh" >&2
  exit 1
}

# was this actually a coverage build? (wc consumes all output -> no SIGPIPE)
if [[ "$(find "$build_dir" -name '*.gcno' 2>/dev/null | wc -l | tr -d '[:space:]')" == "0" ]]; then
  echo "No .gcno found in '$build_dir' — configure with --coverage and rebuild:" >&2
  echo "  ./change-compiler.sh -c <cc> --coverage && ./build.sh" >&2
  exit 1
fi

command -v gcovr >/dev/null 2>&1 || {
  echo "gcovr not found. Install it:  brew install gcovr  (macOS)  |  pipx install gcovr" >&2
  exit 1
}

# derive the C compiler that configured this build, then the matching gcov tool
cc="$(sed -n 's/^CMAKE_C_COMPILER:[^=]*=//p' "$build_dir/CMakeCache.txt" | head -1)"
ccbase="$(basename "${cc:-cc}")"
case "$ccbase" in
  gcc-*)  gcov_tool="gcov-${ccbase#gcc-}" ;;    # gcc-16  -> gcov-16
  *gcc*)  gcov_tool="gcov" ;;
  clang*) gcov_tool="llvm-cov gcov" ;;          # clang   -> llvm-cov gcov
  *)      gcov_tool="gcov" ;;
esac
# on macOS the llvm tools usually live behind xcrun
if [[ "$gcov_tool" == "llvm-cov gcov" ]] && ! command -v llvm-cov >/dev/null 2>&1 \
   && command -v xcrun >/dev/null 2>&1; then
  gcov_tool="xcrun llvm-cov gcov"
fi
if ! command -v "${gcov_tool%% *}" >/dev/null 2>&1; then
  echo "warning: '${gcov_tool}' not found; falling back to plain 'gcov'" >&2
  gcov_tool="gcov"
fi

# executable targets from config.cmake (fallback: main)
exes=()
if [[ -f config.cmake ]]; then
  raw="$(tr '\n' ' ' < config.cmake | sed -n 's/.*set(EXECUTABLE_TARGETS\([^)]*\)).*/\1/p' | tr -d '"')"
  for t in $raw; do exes+=("$t"); done
fi
[[ ${#exes[@]} -gt 0 ]] || exes=(main)

if [[ $report_only -eq 1 ]]; then
  echo ">> --report-only: not running; reporting accumulated .gcda counters"
else
  echo ">> running target(s) to generate coverage data: ${exes[*]}"
  for t in "${exes[@]}"; do
    bin=""
    [[ -x "$build_dir/$t" ]] && bin="$build_dir/$t"
    if [[ -z "$bin" ]]; then
      while IFS= read -r f; do [[ -x "$f" ]] && { bin="$f"; break; }; done \
        < <(find "$build_dir" -type f -name "$t" 2>/dev/null)
    fi
    [[ -n "$bin" && -x "$bin" ]] || { echo "   (skip: no executable for '$t')"; continue; }
    echo "   $bin ${prog_args[*]-}"
    "$bin" ${prog_args[@]+"${prog_args[@]}"} || echo "   ($t exited non-zero; coverage still recorded)"
  done
fi

# parallel output dir next to the build dir:  build-gcc-16 -> coverage-gcc-16
case "$build_dir" in build-*) _sfx="${build_dir#build-}" ;; *) _sfx="" ;; esac
cov_dir="coverage${_sfx:+-$_sfx}"
rm -rf "$cov_dir"; mkdir -p "$cov_dir"

echo ">> gcovr (gcov tool: $gcov_tool) -> $cov_dir/"
gcovr --gcov-executable "$gcov_tool" -r . "$build_dir" --decisions --html-details -o "$cov_dir/index.html"
gcovr --gcov-executable "$gcov_tool" -r . "$build_dir" --decisions > "$cov_dir/summary.txt" 2>/dev/null || true
echo "---- summary ----"; cat "$cov_dir/summary.txt" 2>/dev/null || true

# open it
if   command -v open     >/dev/null 2>&1; then open "$cov_dir/index.html" >/dev/null 2>&1 || true
elif command -v xdg-open >/dev/null 2>&1; then xdg-open "$cov_dir/index.html" >/dev/null 2>&1 || true
fi
echo "Report: $(pwd)/$cov_dir/index.html"
