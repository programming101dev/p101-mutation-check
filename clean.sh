#!/usr/bin/env bash
# clean.sh — remove this project's build / coverage / profile output for a
# clean slate. Never touches source (src/, include/, config.cmake, *.sh).
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

usage() {
  cat <<'USAGE'
Usage: ./clean.sh [-n|--dry-run]
  Removes generated output only:
    build-<cc>/ coverage-<cc>/ profile-<cc>/ cmake-build-*/  (and plain build/)
    .last-build-dir
    stray *.gcda *.gcno *.gcov *.su *.ci gmon.out perf.data coverage.html
  Source and scripts are never touched. Safe to re-run.
  -n, --dry-run   list what would be removed; delete nothing.
USAGE
}
case " $* " in *" --help "*|*" -h "*) usage; exit 0 ;; esac

dry=0
if [[ "${1-}" == "-n" || "${1-}" == "--dry-run" ]]; then dry=1; fi

shopt -s nullglob

dirs=()
for d in build build-* coverage coverage-* profile profile-* cmake-build-*; do
  if [[ -d "$d" ]]; then dirs+=("$d"); fi
done
files=()
for f in .last-build-dir gmon.out perf.data coverage.html *.gcda *.gcno *.gcov *.su *.ci; do
  if [[ -f "$f" ]]; then files+=("$f"); fi
done

targets=( ${dirs[@]+"${dirs[@]}"} ${files[@]+"${files[@]}"} )
if [[ ${#targets[@]} -eq 0 ]]; then
  echo "Already clean."
  exit 0
fi

echo "Removing:"
printf '  %s\n' "${targets[@]}"
if [[ $dry -eq 1 ]]; then
  echo "(dry-run — nothing deleted)"
  exit 0
fi
rm -rf -- "${targets[@]}"
echo "Clean."
