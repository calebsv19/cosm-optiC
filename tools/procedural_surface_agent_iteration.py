#!/usr/bin/env python3
"""Run one inspect-edit-compile-render-evaluate-undo PSG-8 agent loop."""

from __future__ import annotations

import argparse
import json
import platform
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures" / "procedural_surface_field_presets"


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine() /
        "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--agent-tool", type=Path,
        default=default_tool("procedural_surface_agent_tool"),
    )
    parser.add_argument(
        "--asset-tool", type=Path,
        default=default_tool("procedural_surface_field_preset_asset_tool"),
    )
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument(
        "--output-root", type=Path,
        default=(
            ROOT / "build" / "agent_runs" / "ray_tracing" /
            "procedural_surface_agent_iteration" / "psg8"
        ),
    )
    return parser.parse_args()


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result.stdout


def load(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def write(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    args = parse_args()
    output_root = args.output_root.resolve()
    authored = output_root / "authored"
    proof = output_root / "visual_proof"
    authored.mkdir(parents=True, exist_ok=True)
    graph = FIXTURES / "wind_shaped_sand.json"
    manifest = FIXTURES / "wind_shaped_sand.parameters.json"
    binding = FIXTURES / "wind_shaped_sand.top.binding.json"

    inspect = json.loads(run([
        str(args.agent_tool.resolve()), "inspect",
        "--graph", str(graph),
        "--manifest", str(manifest),
        "--binding", str(binding),
    ]))
    base_digest = inspect["graph_digest_sha256"]
    edited_graph = authored / "wind_shaped_sand_wide.json"
    undo_graph = authored / "undo_graph.json"
    edit_receipt = authored / "edit_receipt.json"
    run([
        str(args.agent_tool.resolve()), "apply",
        "--graph", str(graph),
        "--manifest", str(manifest),
        "--binding", str(binding),
        "--expected-base-digest", base_digest,
        "--set", "dune_spacing=8.5",
        "--set", "dune_primary_strength=0.68",
        "--out", str(edited_graph),
        "--undo-out", str(undo_graph),
        "--receipt-out", str(edit_receipt),
    ])
    receipt = load(edit_receipt)
    edited_digest = receipt["result_graph_digest_sha256"]
    if edited_digest == base_digest:
        raise RuntimeError("agent edit did not change graph identity")

    source_contract = load(FIXTURES / "preset_binding_visual_contract.json")
    contract = {
        **source_contract,
        "proof_id": "procedural_surface_agent_iteration_psg8",
        "title": "PSG-8 recursive agent-authored sand iteration",
        "visual_intent": (
            "Compare the checked-in top-bound sand graph with an agent-created "
            "duplicate whose typed dune spacing and strength parameters changed."
        ),
        "expected_visual_signal": (
            "The edited duplicate keeps top-only surface ownership while dune "
            "frequency and ridge strength visibly change."
        ),
        "rejection_condition": (
            "Reject if the digest-guarded edit is stale, topology/material "
            "loading fails, or all views remain visually equivalent."
        ),
        "assertions": {
            "minimum_primary_hit_pixels": 18000,
            "minimum_image_luma_standard_deviation": 7.0,
            "minimum_pairwise_changed_pixels": 7000,
        },
        "presets": [
            {
                "id": "sand_agent_baseline",
                "label": "AGENT BASELINE",
                "graph": str(graph),
                "binding": str(binding),
                "target_edge_length_units": 0.05,
                "displacement_amplitude_units": 0.26,
                "edge_lock_width_units": 0.30,
            },
            {
                "id": "sand_agent_wide_ridges",
                "label": "AGENT WIDE RIDGES",
                "graph": str(edited_graph),
                "binding": str(binding),
                "target_edge_length_units": 0.05,
                "displacement_amplitude_units": 0.26,
                "edge_lock_width_units": 0.30,
            },
        ],
    }
    contract_path = authored / "iteration_visual_contract.json"
    write(contract_path, contract)
    run([
        "python3",
        str(ROOT / "tools" /
            "procedural_surface_field_preset_visual_proof.py"),
        "--contract", str(contract_path),
        "--base-recipe", str(
            ROOT / "tests" / "fixtures" /
            "procedural_surface_rock_prism_psg0" / "recipe.json"
        ),
        "--asset-tool", str(args.asset_tool.resolve()),
        "--render-cli", str(args.render_cli.resolve()),
        "--output-root", str(proof),
    ])
    visual = load(proof / "summary.json")

    restored_graph = authored / "restored_graph.json"
    restore_receipt = authored / "restore_receipt.json"
    run([
        str(args.agent_tool.resolve()), "restore",
        "--graph", str(edited_graph),
        "--restore", str(undo_graph),
        "--expected-base-digest", edited_digest,
        "--out", str(restored_graph),
        "--receipt-out", str(restore_receipt),
    ])
    restored = json.loads(run([
        str(args.agent_tool.resolve()), "inspect",
        "--graph", str(restored_graph),
        "--manifest", str(manifest),
        "--binding", str(binding),
    ]))
    failures = list(visual["failures"])
    if restored["graph_digest_sha256"] != base_digest:
        failures.append("undo did not restore the original graph digest")
    summary = {
        "schema": "ray_tracing.procedural_surface_agent_iteration",
        "schema_version": 1,
        "passed": not failures,
        "authority": "local_diagnostic_only",
        "base_graph_digest_sha256": base_digest,
        "edited_graph_digest_sha256": edited_digest,
        "restored_graph_digest_sha256": restored["graph_digest_sha256"],
        "parameter_edits": receipt["edits"],
        "pairwise_comparisons": visual["pairwise_comparisons"],
        "visual_contact_sheet": visual["contact_sheet"],
        "failures": failures,
    }
    write(output_root / "iteration_summary.json", summary)
    print(json.dumps(summary, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
