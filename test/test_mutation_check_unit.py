#!/usr/bin/env python3
from __future__ import annotations

import argparse
import contextlib
import importlib.machinery
import importlib.util
import io
import json
import runpy
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "p101-mutation-check"


def load_tool():
    loader = importlib.machinery.SourceFileLoader("p101_mutation_check", str(TOOL))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    assert spec is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[loader.name] = module
    loader.exec_module(module)
    return module


tool = load_tool()


def candidate(path: Path, **overrides):
    value = {
        "path": str(path),
        "line": 1,
        "start": 0,
        "end": 1,
        "operator": "comparison-boundary",
        "original": "a",
        "replacement": "b",
    }
    value.update(overrides)
    return value


def args_for(project: Path, database: Path, **overrides):
    project = project.resolve()
    database = database.resolve()
    value = argparse.Namespace(
        project=project,
        project_spellings={str(project)},
        compile_db=database,
        audit=project / "audit",
        clang="clang",
        max_mutants=100,
        timeout=1.0,
        operator=[],
        list=False,
        json=False,
        test_command=["true"],
    )
    for name, setting in overrides.items():
        setattr(value, name, setting)
    return value


def capture(function, *args):
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        result = function(*args)
    return result, stdout.getvalue(), stderr.getvalue()


def test_argument_and_ignore_helpers() -> None:
    parsed = tool.parse_args(["--compile-db", "db", "--", "project", "--", "echo"])
    assert parsed.test_command == ["echo"]
    parsed = tool.parse_args(["--compile-db", "db", "project"])
    assert parsed.test_command == []
    ignored = tool.ignored("unused", [".git", "build-clang", "keep", "profile-x"])
    assert ignored == {".git", "build-clang", "profile-x"}


def test_discovery_paths() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        output = root / "out.json"
        args = args_for(root, root / "db", operator=["wanted"], max_mutants=1)
        completed = subprocess.CompletedProcess([], 0, stderr="")

        def successful_run(*unused_args, **unused_kwargs):
            output.write_text(
                json.dumps(
                    {
                        "schema": "p101-mutation-candidates-v1",
                        "candidates": [
                            {"operator": "wanted", "value": 1},
                            {"operator": "other", "value": 2},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            return completed

        with mock.patch.object(tool.subprocess, "run", successful_run):
            assert tool.discover(args, output) == [{"operator": "wanted", "value": 1}]
            args.operator = []
            assert len(tool.discover(args, output)) == 1

        with mock.patch.object(tool.subprocess, "run", return_value=subprocess.CompletedProcess([], 2, stderr="bad")):
            try:
                tool.discover(args, output)
                raise AssertionError("discovery failure accepted")
            except RuntimeError as exc:
                assert str(exc) == "bad"

        output.unlink(missing_ok=True)
        with mock.patch.object(tool.subprocess, "run", return_value=completed):
            try:
                tool.discover(args, output)
                raise AssertionError("missing output accepted")
            except RuntimeError as exc:
                assert "candidate discovery failed" in str(exc)

        output.write_text('{"schema":"wrong","candidates":[]}', encoding="utf-8")
        with mock.patch.object(tool.subprocess, "run", return_value=completed):
            try:
                tool.discover(args, output)
                raise AssertionError("wrong schema accepted")
            except RuntimeError as exc:
                assert "unsupported schema" in str(exc)

        output.write_text('{"schema":"p101-mutation-candidates-v1","candidates":{}}', encoding="utf-8")
        with mock.patch.object(tool.subprocess, "run", return_value=completed):
            try:
                tool.discover(args, output)
                raise AssertionError("non-list candidates accepted")
            except RuntimeError:
                pass


def test_command_and_rewrite_helpers() -> None:
    completed = subprocess.CompletedProcess([], 7, stdout="x" * 2100)
    with mock.patch.object(tool.subprocess, "run", return_value=completed):
        code, output = tool.run_command(["false"], Path("."), 1.0)
    assert code == 7 and len(output) == 2000

    timeout_text = subprocess.TimeoutExpired(["slow"], 1, output="output")
    with mock.patch.object(tool.subprocess, "run", side_effect=timeout_text):
        assert tool.run_command(["slow"], Path("."), 1.0) == (None, "output")
    timeout_bytes = subprocess.TimeoutExpired(["slow"], 1, output=b"output")
    with mock.patch.object(tool.subprocess, "run", side_effect=timeout_bytes):
        assert tool.run_command(["slow"], Path("."), 1.0) == (None, "")

    args = argparse.Namespace(project_spellings={"/a/long", "/a"})
    assert tool.rewrite_project_path(args, Path("/copy"), "/a/long/file") == "/copy/file"


def test_compile_database_variants() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        copied = root / "copy"
        copied.mkdir()
        source = root / "source.c"
        source.write_text("a", encoding="utf-8")
        database = root / "compile_commands.json"
        args = args_for(root, database)
        item = candidate(source)

        database.write_text(json.dumps([{"directory": str(root), "file": str(source), "arguments": []}]), encoding="utf-8")
        try:
            tool.compile_mutant(args, copied, item)
            raise AssertionError("empty command accepted")
        except RuntimeError as exc:
            assert "empty" in str(exc)

        database.write_text(json.dumps([{"directory": str(root), "file": "source.c", "command": "clang -c source.c -o old.o -MF dep -MT target -MQ queue -output -ObjC"}]), encoding="utf-8")
        with mock.patch.object(tool, "run_command", return_value=(0, "ok")) as run:
            assert tool.compile_mutant(args, copied, item) == (0, "ok")
            command = run.call_args.args[0]
            assert "-fsyntax-only" in command
            assert "-ObjC" in command
            assert "old.o" not in command
            assert "dep" not in command
            assert run.call_args.args[1].is_dir()

        build_directory = root / "build-clang"
        database.write_text(json.dumps([{"directory": str(build_directory), "file": str(source), "arguments": ["clang", "-c", str(source)]}]), encoding="utf-8")
        with mock.patch.object(tool, "run_command", return_value=(0, "ok")) as run:
            assert tool.compile_mutant(args, copied, item) == (0, "ok")
            assert run.call_args.args[1] == copied / "build-clang"
            assert (copied / "build-clang").is_dir()

        database.write_text(json.dumps([{"directory": str(root.parent), "file": str(source), "arguments": ["clang", "-c", str(source)]}]), encoding="utf-8")
        try:
            tool.compile_mutant(args, copied, item)
            raise AssertionError("outside directory accepted")
        except RuntimeError as exc:
            assert "outside project" in str(exc)

        database.write_text("[]", encoding="utf-8")
        try:
            tool.compile_mutant(args, copied, item)
            raise AssertionError("missing entry accepted")
        except RuntimeError as exc:
            assert "no command" in str(exc)


def test_candidate_application() -> None:
    with tempfile.TemporaryDirectory() as temp:
        project = Path(temp) / "project"
        copied = Path(temp) / "copied"
        project.mkdir()
        copied.mkdir()
        project = project.resolve()
        copied = copied.resolve()
        (project / "file.c").write_text("abc", encoding="utf-8")
        (copied / "file.c").write_text("abc", encoding="utf-8")
        item = candidate(project / "file.c")
        tool.apply_candidate(project, copied, item)
        assert (copied / "file.c").read_text(encoding="utf-8") == "bbc"

        for broken in (
            candidate(project / "file.c", start=-1),
            candidate(project / "file.c", end=9),
            candidate(project / "file.c", original="z"),
        ):
            try:
                tool.apply_candidate(project, copied, broken)
                raise AssertionError("invalid location accepted")
            except RuntimeError as exc:
                assert "source changed" in str(exc)

        try:
            tool.apply_candidate(project, copied, candidate(Path(temp) / "outside.c"))
            raise AssertionError("outside candidate accepted")
        except RuntimeError as exc:
            assert "outside project" in str(exc)


def test_execute_mutant_outcomes() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        source = root / "file.c"
        source.write_text("a", encoding="utf-8")
        database = root / "compile_commands.json"
        database.write_text("[]", encoding="utf-8")
        args = args_for(root, database)
        item = candidate(source)

        cases = [
            ((None, "timeout"), None, "inconclusive"),
            ((2, "compile"), None, "inconclusive"),
            ((0, "compile"), (None, "timeout"), "inconclusive"),
            ((0, "compile"), (0, "pass"), "survived"),
            ((0, "compile"), (1, "fail"), "killed"),
        ]
        for compile_result, test_result, outcome in cases:
            run_side_effect = [] if test_result is None else [test_result]
            with (
                mock.patch.object(tool, "apply_candidate"),
                mock.patch.object(tool, "compile_mutant", return_value=compile_result),
                mock.patch.object(tool, "run_command", side_effect=run_side_effect),
            ):
                result = tool.execute_mutant(args, item)
            assert result.outcome == outcome


def test_output_variants() -> None:
    item = candidate(Path("file.c"))
    results = [
        tool.Result(item, "survived", 0, ""),
        tool.Result(item, "killed", 1, ""),
        tool.Result(item, "inconclusive", None, ""),
    ]
    args = argparse.Namespace(json=False)
    _, stdout, _ = capture(tool.output_results, args, results, 3)
    assert "survived comparison-boundary" in stdout
    assert "killed=1 survived=1 inconclusive=1" in stdout
    args.json = True
    _, stdout, _ = capture(tool.output_results, args, results, 3)
    report = json.loads(stdout)
    assert report["findings"][0]["id"] == "P101-MUTATION-001"


def test_main_validation_and_outcomes() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        database = root / "compile_commands.json"
        database.write_text("[]", encoding="utf-8")
        audit = root / "audit"
        audit.write_text("", encoding="utf-8")
        prefix = ["--compile-db", str(database), "--audit", str(audit)]
        base = [*prefix, str(root)]

        result, _, stderr = capture(tool.main, ["--compile-db", str(root / "missing"), str(root)])
        assert result == 2 and "missing" in stderr
        for option in ("--max-mutants", "--timeout"):
            result, _, stderr = capture(tool.main, [*prefix, option, "0", "--list", str(root)])
            assert result == 2 and "positive" in stderr
        result, _, stderr = capture(tool.main, base)
        assert result == 2 and "supply a test command" in stderr

        items = [candidate(root / "file.c")]
        with mock.patch.object(tool, "discover", return_value=items):
            result, stdout, _ = capture(tool.main, [*prefix, "--list", str(root)])
        assert result == 0 and "candidate(s)" in stdout
        with mock.patch.object(tool, "discover", return_value=items):
            result, stdout, _ = capture(tool.main, [*prefix, "--list", "--json", str(root)])
        listed = json.loads(stdout)
        assert result == 0
        assert listed["schema"] == "p101-mutation-candidates-v1"
        assert listed["summary"]["selected"] == 1

        with mock.patch.object(tool, "discover", return_value=[]):
            result, _, stderr = capture(tool.main, [*base, "--", "true"])
        assert result == 2 and "no mutants" in stderr

        for baseline, expected in ((None, "timed out"), (3, "exit 3")):
            with (
                mock.patch.object(tool, "discover", return_value=items),
                mock.patch.object(tool, "run_command", return_value=(baseline, "detail")),
            ):
                result, _, stderr = capture(tool.main, [*base, "--", "true"])
            assert result == 2 and expected in stderr

        outcomes = (
            ([tool.Result(items[0], "inconclusive", None, "")], 2),
            ([tool.Result(items[0], "survived", 0, "")], 1),
            ([tool.Result(items[0], "killed", 1, "")], 0),
        )
        for results, expected in outcomes:
            with (
                mock.patch.object(tool, "discover", return_value=items),
                mock.patch.object(tool, "run_command", return_value=(0, "")),
                mock.patch.object(tool, "execute_mutant", side_effect=results),
                mock.patch.object(tool, "output_results"),
            ):
                result, _, _ = capture(tool.main, [*base, "--", "true"])
            assert result == expected

        with mock.patch.object(tool, "discover", side_effect=json.JSONDecodeError("bad", "", 0)):
            result, _, stderr = capture(tool.main, [*base, "--", "true"])
        assert result == 2 and "bad" in stderr


def main() -> int:
    test_argument_and_ignore_helpers()
    test_discovery_paths()
    test_command_and_rewrite_helpers()
    test_compile_database_variants()
    test_candidate_application()
    test_execute_mutant_outcomes()
    test_output_variants()
    test_main_validation_and_outcomes()
    with mock.patch.object(sys, "argv", [str(TOOL), "--compile-db", "missing", "missing"]):
        try:
            runpy.run_path(str(TOOL), run_name="__main__")
        except SystemExit as exc:
            assert exc.code == 2
    print("PASS test_mutation_check_unit")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
