#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "p101-mutation-check"
AUDIT = ROOT.parent / "p101-wrapper-audit" / "p101-wrapper-audit"


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="p101-mutation-check-test-") as temp:
        project = Path(temp)
        source = project / "boundary.c"
        test_program = project / "test_boundary.py"
        database = project / "compile_commands.json"
        source.write_text("int below_limit(int value) { return value < 7; }\n", encoding="utf-8")
        test_program.write_text(
            """
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parent
exe = root / "boundary-test"
source = root / "boundary.c"
driver = root / "driver.c"
driver.write_text("int below_limit(int); int main(void) { return below_limit(7) == 0 ? 0 : 1; }\\n")
result = subprocess.run(["clang", str(source), str(driver), "-o", str(exe)])
if result.returncode != 0:
    raise SystemExit(result.returncode)
raise SystemExit(subprocess.run([str(exe)]).returncode)
""".lstrip(),
            encoding="utf-8",
        )
        database.write_text(
            json.dumps([{"directory": str(project), "file": str(source), "arguments": ["clang", "-c", str(source)]}]),
            encoding="utf-8",
        )
        command = [
            str(TOOL),
            "--audit",
            str(AUDIT),
            "--compile-db",
            str(database),
            "--operator",
            "comparison-boundary",
            str(project),
            "--",
            sys.executable,
            str(test_program),
        ]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        assert result.returncode == 0, result.stderr + result.stdout
        assert "selected=1 killed=1 survived=0" in result.stdout

        list_command = [
            str(TOOL),
            "--audit",
            str(AUDIT),
            "--compile-db",
            str(database),
            "--operator",
            "comparison-boundary",
            "--list",
            str(project),
        ]
        list_result = subprocess.run(list_command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        assert list_result.returncode == 0, list_result.stderr + list_result.stdout
        assert "comparison-boundary" in list_result.stdout

        fake_audit = project / "fake-audit.py"
        fake_audit.write_text(
            """#!/usr/bin/env python3
import json
import sys
output = sys.argv[sys.argv.index("--mutation-candidates-output") + 1]
with open(output, "w", encoding="utf-8") as stream:
    json.dump({"schema": "p101-mutation-candidates-v1", "candidates": []}, stream)
""",
            encoding="utf-8",
        )
        fake_audit.chmod(0o755)
        empty_command = [str(TOOL), "--audit", str(fake_audit), "--compile-db", str(database), str(project), "--", sys.executable, str(test_program)]
        empty_result = subprocess.run(empty_command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        assert empty_result.returncode == 2
        assert "no mutants" in empty_result.stderr

    print("PASS test_killed_boundary_mutant")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
