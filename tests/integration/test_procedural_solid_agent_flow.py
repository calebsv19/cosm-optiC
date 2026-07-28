#!/usr/bin/env python3
"""Exercise PSG-10 inspect/edit/save/undo and adaptive compile as one flow."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def run(command: list[str]) -> dict:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return json.loads(result.stdout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--agent-tool", type=Path, required=True)
    parser.add_argument("--asset-tool", type=Path, required=True)
    args = parser.parse_args()

    output = ROOT / "build" / "agent_runs" / "ray_tracing" / \
        "procedural_solid" / "psg10" / "agent_flow"
    output.mkdir(parents=True, exist_ok=True)
    fixture = (
        ROOT / "tests" / "fixtures" / "procedural_solid_graphs" /
        "twisted_tapered_column.json"
    )
    edited = output / "edited.json"
    undo = output / "undo.json"
    restored = output / "restored.json"
    asset = output / "edited.runtime.json"
    receipt = output / "edited_mesh_receipt.json"

    inspected = run([
        str(args.agent_tool), "inspect", "--graph", str(fixture),
    ])
    base_digest = inspected["graph_digest_sha256"]
    assert inspected["node_count"] == 4
    assert any(
        item["id"] == "twisted.scalar_a"
        for item in inspected["parameters"]
    )

    applied = run([
        str(args.agent_tool), "apply",
        "--graph", str(fixture),
        "--expected-base-digest", base_digest,
        "--set", "twisted.scalar_a=0.55",
        "--set", "tapered.scalar_a=0.16",
        "--out", str(edited),
        "--undo-out", str(undo),
    ])
    assert applied["status"] == "committed"
    assert applied["result_graph_digest_sha256"] != base_digest

    compiled = subprocess.run([
        str(args.asset_tool),
        "--graph", str(edited),
        "--out", str(asset),
        "--summary-out", str(receipt),
        "--asset-id", "psg10_agent_edited_column",
        "--cells", "18",
        "--adaptive",
        "--maximum-cells", "72",
        "--feature-size", "0.16",
        "--collision-authority", "derived_shell",
    ], cwd=ROOT, text=True, capture_output=True)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    assert str(receipt) in compiled.stdout
    mesh_receipt = json.loads(receipt.read_text(encoding="utf-8"))
    assert mesh_receipt["adaptive_converged"]
    assert mesh_receipt["boundary_edge_count"] == 0
    assert mesh_receipt["nonmanifold_edge_count"] == 0

    restored_receipt = run([
        str(args.agent_tool), "restore",
        "--graph", str(edited),
        "--restore", str(undo),
        "--expected-base-digest", applied["result_graph_digest_sha256"],
        "--out", str(restored),
    ])
    assert restored_receipt["status"] == "committed"
    assert restored_receipt["result_graph_digest_sha256"] == base_digest

    stale = subprocess.run([
        str(args.agent_tool), "apply",
        "--graph", str(edited),
        "--expected-base-digest", base_digest,
        "--set", "twisted.scalar_a=0.2",
        "--out", str(output / "stale.json"),
        "--undo-out", str(output / "stale_undo.json"),
    ], cwd=ROOT, text=True, capture_output=True)
    assert stale.returncode != 0
    assert not (output / "stale.json").exists()

    summary = {
        "schema": "ray_tracing.procedural_solid_agent_flow",
        "schema_version": 1,
        "passed": True,
        "base_graph_digest_sha256": base_digest,
        "edited_graph_digest_sha256":
            applied["result_graph_digest_sha256"],
        "restored_graph_digest_sha256":
            restored_receipt["result_graph_digest_sha256"],
        "adaptive_pass_count": mesh_receipt["adaptive_pass_count"],
        "adaptive_selected_pass": mesh_receipt["adaptive_selected_pass"],
        "mesh_digest_sha256": mesh_receipt["mesh_digest_sha256"],
    }
    (output / "flow_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8",
    )
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
