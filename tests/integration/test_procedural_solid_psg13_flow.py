#!/usr/bin/env python3
"""End-to-end PSG-13 material binding and agent transaction contract."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(
    command: list[str], *, expect_success: bool = True
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != expect_success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError(
            f"unexpected exit {result.returncode}: {' '.join(command)}"
        )
    return result


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(
            f"usage: {sys.argv[0]} SOLID_ASSET_TOOL MATERIAL_AGENT_TOOL",
            file=sys.stderr,
        )
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    asset_tool = pathlib.Path(sys.argv[1]).resolve()
    agent_tool = pathlib.Path(sys.argv[2]).resolve()
    graph = (
        root
        / "tests/fixtures/procedural_solid_graphs"
        / "rounded_block_with_tunnel.json"
    )
    with tempfile.TemporaryDirectory(prefix="psg13_material_flow_") as temp:
        run_root = pathlib.Path(temp)
        mesh = run_root / "solid.runtime.json"
        solid_receipt = run_root / "solid.receipt.json"
        run(
            [
                str(asset_tool),
                "--graph",
                str(graph),
                "--out",
                str(mesh),
                "--summary-out",
                str(solid_receipt),
                "--asset-id",
                "psg13_tunnel",
                "--cells",
                "20",
                "--local-adaptive",
                "--maximum-cells",
                "40",
                "--feature-size",
                "0.18",
                "--collision-authority",
                "derived_shell",
            ]
        )
        original_mesh_file_digest = digest(mesh)
        original_mesh = json.loads(mesh.read_text(encoding="utf-8"))
        original_receipt = json.loads(solid_receipt.read_text(encoding="utf-8"))
        assert original_receipt["cut_triangle_count"] > 0
        assert original_receipt["retained_triangle_count"] > 0

        base_binding = run_root / "base.binding.json"
        init_result = run(
            [
                str(agent_tool),
                "init",
                "--mesh",
                str(mesh),
                "--solid-receipt",
                str(solid_receipt),
                "--binding-id",
                "psg13_tunnel_materials",
                "--fallback",
                "default",
                "--out",
                str(base_binding),
            ]
        )
        init_receipt = json.loads(init_result.stdout)
        assert init_receipt["mesh_digest_sha256"] == original_receipt[
            "mesh_digest_sha256"
        ]
        assert init_receipt["region_digest_sha256"] == original_receipt[
            "region_digest_sha256"
        ]
        assert init_receipt["assignment_count"] == 0
        assert all(region["used_fallback"] for region in init_receipt["regions"])

        stale_output = run_root / "stale.binding.json"
        stale_undo = run_root / "stale.undo.json"
        run(
            [
                str(agent_tool),
                "apply",
                "--mesh",
                str(mesh),
                "--binding",
                str(base_binding),
                "--expected-base-digest",
                "0" * 64,
                "--set-kind",
                "cut=glossy",
                "--out",
                str(stale_output),
                "--undo-out",
                str(stale_undo),
            ],
            expect_success=False,
        )
        assert not stale_output.exists()
        assert not stale_undo.exists()

        edited_binding = run_root / "edited.binding.json"
        undo_binding = run_root / "undo.binding.json"
        apply_result = run(
            [
                str(agent_tool),
                "apply",
                "--mesh",
                str(mesh),
                "--binding",
                str(base_binding),
                "--expected-base-digest",
                init_receipt["binding_digest_sha256"],
                "--set-kind",
                "retained=rough_metal",
                "--set-kind",
                "cut=glossy",
                "--out",
                str(edited_binding),
                "--undo-out",
                str(undo_binding),
            ]
        )
        apply_receipt = json.loads(apply_result.stdout)
        materials = {
            region["kind"]: region["material"]
            for region in apply_receipt["regions"]
        }
        assert materials["retained"] == "rough_metal"
        assert materials["cut"] == "glossy"
        assert apply_receipt["assignment_count"] == apply_receipt["region_count"]

        compiled = run_root / "compiled.json"
        compile_result = run(
            [
                str(agent_tool),
                "compile",
                "--mesh",
                str(mesh),
                "--binding",
                str(edited_binding),
                "--out",
                str(compiled),
            ]
        )
        compiled_receipt = json.loads(compile_result.stdout)
        assert json.loads(compiled.read_text(encoding="utf-8")) == compiled_receipt
        assert compiled_receipt["binding_digest_sha256"] == apply_receipt[
            "binding_digest_sha256"
        ]
        assert digest(mesh) == original_mesh_file_digest
        assert json.loads(mesh.read_text(encoding="utf-8")) == original_mesh

        restored = run_root / "restored.binding.json"
        restore_result = run(
            [
                str(agent_tool),
                "restore",
                "--mesh",
                str(mesh),
                "--binding",
                str(edited_binding),
                "--restore",
                str(undo_binding),
                "--expected-base-digest",
                apply_receipt["binding_digest_sha256"],
                "--out",
                str(restored),
            ]
        )
        restore_receipt = json.loads(restore_result.stdout)
        assert restore_receipt["binding_digest_sha256"] == init_receipt[
            "binding_digest_sha256"
        ]
        assert restore_receipt["assignment_count"] == 0

        stale_binding = json.loads(edited_binding.read_text(encoding="utf-8"))
        stale_binding["mesh_digest_sha256"] = "f" * 64
        stale_binding_path = run_root / "stale-mesh.binding.json"
        stale_binding_path.write_text(
            json.dumps(stale_binding, indent=2), encoding="utf-8"
        )
        stale_compile = run_root / "stale-compiled.json"
        run(
            [
                str(agent_tool),
                "compile",
                "--mesh",
                str(mesh),
                "--binding",
                str(stale_binding_path),
                "--out",
                str(stale_compile),
            ],
            expect_success=False,
        )
        assert not stale_compile.exists()
        assert digest(mesh) == original_mesh_file_digest

    print("PSG-13 material binding agent flow passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
