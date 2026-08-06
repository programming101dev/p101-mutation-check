#!/usr/bin/env bash
set -euo pipefail

tool=$1
compiler=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-mutation-check-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$tool" --help >/dev/null

cat >"$work/boundary.c" <<'SOURCE'
struct p101_env;
static void p101_close(const struct p101_env *env, void *err, int fd)
{
    (void)env;
    (void)err;
    (void)fd;
}

int below_limit(int value)
{
    return value < 7;
}

int combine(int left, int right, int enabled)
{
    int total = left + right;
    return enabled && total > 0;
}

void cleanup(const struct p101_env *env, void *err, int fd)
{
    p101_close(env, err, fd);
}
SOURCE

cat >"$work/driver.c" <<'SOURCE'
int below_limit(int value);
int main(void)
{
    return below_limit(7) == 0 ? 0 : 1;
}
SOURCE

cat >"$work/run-test.sh" <<SCRIPT
#!/usr/bin/env bash
set -euo pipefail
root=\$(CDPATH='' cd -- "\$(dirname -- "\${BASH_SOURCE[0]}")" && pwd)
"$compiler" "\$root/boundary.c" "\$root/driver.c" -o "\$root/boundary-test"
"\$root/boundary-test"
SCRIPT
chmod +x "$work/run-test.sh"

escaped_work=${work//\\/\\\\}
escaped_work=${escaped_work//\"/\\\"}
cat >"$work/compile_commands.json" <<JSON
[
  {
    "directory": "$escaped_work",
    "file": "$escaped_work/boundary.c",
    "arguments": ["$compiler", "-c", "$escaped_work/boundary.c"]
  }
]
JSON

"$tool" --compile-db "$work/compile_commands.json" \
    --operator comparison-boundary --list "$work" |
    grep -q 'comparison-boundary'
"$tool" --compile-db "$work/compile_commands.json" \
    --operator logical-connective --list "$work" |
    grep -q 'logical-connective'
"$tool" --compile-db "$work/compile_commands.json" \
    --operator arithmetic-operator --list "$work" |
    grep -q 'arithmetic-operator'
"$tool" --compile-db "$work/compile_commands.json" \
    --operator skip-call --list "$work" |
    grep -q 'skip-call'

output=$("$tool" --compile-db "$work/compile_commands.json" \
    --operator comparison-boundary --max-mutants 1 "$work" -- bash "$work/run-test.sh")
grep -q 'selected=1 killed=1 survived=0' <<<"$output"

set +e
"$tool" --compile-db "$work/compile_commands.json" \
    --operator error-predicate "$work" -- bash "$work/run-test.sh" \
    >"$work/no-mutants.out" 2>"$work/no-mutants.err"
status=$?
set -e
[ "$status" -eq 2 ]
grep -q 'no mutants' "$work/no-mutants.err"
