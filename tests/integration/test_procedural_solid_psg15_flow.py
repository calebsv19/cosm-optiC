#!/usr/bin/env python3
"""Focused PSG-15 graph authoring, determinism, and undo flow."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], expect_ok: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True,
                            check=False)
    if expect_ok and result.returncode:
        raise AssertionError(
            f"command failed: {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}")
    if not expect_ok and not result.returncode:
        raise AssertionError(f"command unexpectedly passed: {' '.join(command)}")
    return result


def receipt(command: list[str]) -> dict:
    return json.loads(run(command).stdout.splitlines()[0])


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_procedural_solid_psg15_flow.py GRAPH_TOOL")
    tool = str(Path(sys.argv[1]).resolve())
    binding_digest = "a" * 64
    with tempfile.TemporaryDirectory(prefix="psg15_flow_") as temporary:
        root = Path(temporary)
        graph = root / "graph.json"
        snapshot = root / "snapshot.json"
        initialized = receipt([
            tool, "init", "--template", "snow_accumulation",
            "--graph-id", "snow_test", "--binding-id", "binding_test",
            "--binding-digest", binding_digest, "--output", str(graph)])
        inspected = receipt([tool, "inspect", "--graph", str(graph)])
        assert initialized["graph_digest_sha256"] == (
            inspected["graph_digest_sha256"])
        added = receipt([
            tool, "add-node", "--graph", str(graph), "--expected-digest",
            initialized["graph_digest_sha256"], "--node", "agent_bias",
            "--kind", "constant", "--snapshot", str(snapshot)])
        assert added["node_count"] == initialized["node_count"] + 1
        edited = receipt([
            tool, "set", "--graph", str(graph), "--expected-digest",
            added["graph_digest_sha256"], "--node", "agent_bias",
            "--parameter", "value", "--value", "0.35"])
        assert edited["graph_digest_sha256"] != added["graph_digest_sha256"]
        connected = receipt([
            tool, "connect", "--graph", str(graph), "--expected-digest",
            edited["graph_digest_sha256"], "--node", "snow_mask",
            "--input", "b", "--source", "agent_bias"])
        compiled = receipt([tool, "compile", "--graph", str(graph)])
        assert compiled["graph_digest_sha256"] == (
            connected["graph_digest_sha256"])
        stale = run([
            tool, "set", "--graph", str(graph), "--expected-digest",
            initialized["graph_digest_sha256"], "--node", "agent_bias",
            "--parameter", "value", "--value", "0.4"], expect_ok=False)
        assert "stale_base" not in stale.stdout or stale.returncode != 0
        restored = receipt([
            tool, "restore", "--graph", str(graph), "--snapshot",
            str(snapshot), "--expected-digest",
            connected["graph_digest_sha256"]])
        assert restored["graph_digest_sha256"] == (
            initialized["graph_digest_sha256"])
        repeat = root / "repeat.json"
        repeated = receipt([
            tool, "init", "--template", "snow_accumulation",
            "--graph-id", "snow_test", "--binding-id", "binding_test",
            "--binding-digest", binding_digest, "--output", str(repeat)])
        assert repeated["graph_digest_sha256"] == (
            initialized["graph_digest_sha256"])
        cycle = run([
            tool, "connect", "--graph", str(repeat), "--expected-digest",
            repeated["graph_digest_sha256"], "--node", "snow_mask",
            "--input", "a", "--source", "snow_mask"], expect_ok=False)
        assert cycle.returncode != 0
    print("PSG-15 focused agent flow passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
