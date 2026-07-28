#!/usr/bin/env bash
# copy-template.sh
# Portable template copier with controlled symlinks.
# Platforms: macOS, FreeBSD 13+, Linux.

set -euo pipefail
CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

# ---------- configuration -----------------------------------------------

# These must be symlinks in the destination.
LINK_ITEMS=(
  "sanitizers.txt"
  ".flags"
  "supported_c_compilers.txt"
)

# These must be copied in the destination.
COPY_ITEMS=(
  ".clang-format"
  ".gitignore"
  "build.sh"
  "check.sh"
  "build-all.sh"
  "change-compiler.sh"
  "clean.sh"
  "check-compilers.sh"
  "check-env.sh"
  "doctor.sh"
  "create-links.sh"
  "CMakeLists.txt"
  "config.cmake"
  "coverage.txt"
  "profile.txt"
  "p101-doctor-args.txt"
  "coverage-report.sh"
  "profile-report.sh"
  "report.sh"
  "README.md"
  "commands.md"
  "test.sh"
  "test-all.sh"
  "test"
  "fuzz.sh"
  "debug.sh"
  "fuzz"
  "src"
  "include"
)

# ---------- options ------------------------------------------------------

FORCE=0   # overwrite conflicts
DRYRUN=0  # print actions only
QUIET=0   # reduce chatter

usage() {
  echo "Usage: $0 [-f] [-n] [-q] <destination_directory>"
  echo "  -f  force overwrite of existing paths"
  echo "  -n  dry run, print actions without changes"
  echo "  -q  quiet mode"
  exit 2
}

# --help / -h -> usage, exit 0 (P101 uniform CLI help)
case " $* " in *" --help "*|*" -h "*) ( usage ) || true; exit 0 ;; esac

while getopts ":fnq" opt; do
  case "$opt" in
    f) FORCE=1 ;;
    n) DRYRUN=1 ;;
    q) QUIET=1 ;;
    *) usage ;;
  esac
done
shift $((OPTIND-1))
[ $# -eq 1 ] || usage

dest_dir=$1

say()  { [ "$QUIET" -eq 1 ] || echo "$@"; }
warn() { printf 'WARN: %s\n' "$*" >&2; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 4; }

# ---------- portable path helpers ---------------------------------------

# Absolute path without relying on realpath or readlink -f. Missing final
# components are retained, but ".." components are rejected by the caller.
abspath() {
  local p=$1 suffix="" base
  case "$p" in /*) ;; *) p="$PWD/$p" ;; esac
  while [ ! -d "$p" ]; do
    base=$(basename -- "$p")
    suffix="/$base$suffix"
    p=$(dirname -- "$p")
  done
  ( CDPATH='' cd -- "$p" && printf '%s%s\n' "$(pwd -P)" "$suffix" )
}

# Compute relative path from $2's directory to $1. Falls back to absolute.
relpath() {
  local target=$1 from=$2
  if command -v python3 >/dev/null 2>&1; then
    python3 - "$target" "$from" <<'PY'
import os,sys
t,f=sys.argv[1],sys.argv[2]
print(os.path.relpath(t, start=os.path.dirname(f)))
PY
  else
    # Fallback, acceptable on systems without Python 3.
    echo "$target"
  fi
}

mkd() {
  local d=$1
  if [ ! -d "$d" ]; then
    if [ "$DRYRUN" -eq 1 ]; then say "mkdir -p $d"; else mkdir -p "$d"; fi
  fi
}

clobber_if_needed() {
  local p=$1
  if [ -e "$p" ] || [ -L "$p" ]; then
    if [ "$FORCE" -eq 1 ]; then
      if [ "$DRYRUN" -eq 1 ]; then say "rm -rf $p"; else rm -rf -- "$p"; fi
    else
      warn "$p exists, use -f to overwrite"
      return 1
    fi
  fi
  return 0
}

copy_item() {
  # Directories use cp -a, files preserve perms and times.
  local src=$1 dst=$2
  if [ -d "$src" ] && [ ! -L "$src" ]; then
    if [ "$DRYRUN" -eq 1 ]; then
      say "cp -a \"$src\" \"$dst\""
    else
      cp -a "$src" "$dst"
    fi
  else
    if [ "$DRYRUN" -eq 1 ]; then
      say "install -p \"$src\" \"$dst\""
    else
      if ! install -p "$src" "$dst" 2>/dev/null; then
        cp -p "$src" "$dst"
      fi
    fi
  fi
}

link_item() {
  local src=$1 dst=$2
  local rel
  rel=$(relpath "$src" "$dst")
  if [ "$DRYRUN" -eq 1 ]; then
    say "ln -s \"$rel\" \"$dst\""
  else
    ln -s "$rel" "$dst"
  fi
}

# ---------- main ---------------------------------------------------------

case "/$dest_dir/" in
  */../*|*/./*) die "destination must not contain '.' or '..' path components" ;;
esac

src_root=$(abspath ".")
dest_dir=$(abspath "$dest_dir")
src_parent=$(abspath "..")
workspace_root=$(abspath "../..")
home_dir=""
[ -n "${HOME:-}" ] && [ -d "$HOME" ] && home_dir=$(abspath "$HOME")
case "$dest_dir" in
  /) die "destination must not be the filesystem root" ;;
  "$src_parent"|"$workspace_root") die "destination must not be a workspace container directory" ;;
  "$home_dir") [ -z "$home_dir" ] || die "destination must not be the home directory" ;;
  "$src_root") die "destination is the template source directory" ;;
  "$src_root"/*) die "destination must not be inside the template source directory" ;;
esac
[ "$(dirname -- "$dest_dir")" != "/" ] || die "destination must not be a top-level system directory"
mkd "$dest_dir"

# A successful copy promises a buildable standalone destination. Missing or
# dangling required sources therefore make the operation fail, not degrade.
for i in "${LINK_ITEMS[@]}"; do
  [ -e "$src_root/$i" ] || die "missing or dangling link source: $i"
done
for i in "${COPY_ITEMS[@]}"; do
  [ -e "$src_root/$i" ] || die "missing or dangling copy source: $i"
done
[ -e "$src_root/cmake/FailIfCppcheckDiagnostics.cmake" ] ||
  die "cmake helper source is missing or dangling"

# Create required links.
for i in "${LINK_ITEMS[@]}"; do
  src="$src_root/$i"; dst="$dest_dir/$i"
  if ! clobber_if_needed "$dst"; then exit 3; fi
  mkd "$(dirname -- "$dst")"
  link_item "$src" "$dst"
  say "linked: $i"
done

# Copy required items.
for i in "${COPY_ITEMS[@]}"; do
  src="$src_root/$i"; dst="$dest_dir/$i"
  if [ -d "$src" ] && [ ! -L "$src" ]; then
    if [ -e "$dst" ] || [ -L "$dst" ]; then
      if [ "$FORCE" -eq 1 ]; then
        [ "$DRYRUN" -eq 1 ] && say "rm -rf \"$dst\"" || rm -rf -- "$dst"
      else
        warn "$dst exists, use -f to overwrite"; exit 3
      fi
    fi
    mkd "$(dirname -- "$dst")"
    copy_item "$src" "$dst"
  else
    mkd "$(dirname -- "$dst")"
    if ! clobber_if_needed "$dst"; then exit 3; fi
    copy_item "$src" "$dst"
  fi
  say "copied: $i"
done

# Initialize per destination if .flags is not present.
# You have .flags in LINK_ITEMS. If you want per-project .flags, move it
# from LINK_ITEMS to COPY_ITEMS and keep this block.
# The shared CMakeLists sources helper scripts from cmake/. In the workspace
# that is a symlink to scripts/cmake/, but a STANDALONE project has no scripts/
# sibling to link to, so copy the helpers in as REAL files (dereference the
# symlink). Without this CMake aborts: "p101 helper scripts not found".
if [ ! -e "$dest_dir/cmake/FailIfCppcheckDiagnostics.cmake" ]; then
  if [ "$DRYRUN" -eq 1 ]; then
    say "cp -RL cmake/ helpers -> $dest_dir/cmake"
  else
    mkdir -p "$dest_dir/cmake"
    cp -RL "$src_root/cmake/." "$dest_dir/cmake/" 2>/dev/null || cp -R "$src_root/cmake/." "$dest_dir/cmake/"
    say "materialized cmake/ helper scripts"
  fi
fi

if [ "$DRYRUN" -eq 1 ]; then
  # Dry run never created $dest_dir, so do not try to enter it.
  say "./check-compilers.sh (initialize local compiler lists if .flags is absent)"
else
  pushd "$dest_dir" >/dev/null
  if [ ! -e ".flags" ] && [ ! -L ".flags" ]; then
    say "initializing compiler flags"
    ./check-compilers.sh
  else
    say ".flags present, skip initialization"
  fi
  popd >/dev/null
fi

say "done."
