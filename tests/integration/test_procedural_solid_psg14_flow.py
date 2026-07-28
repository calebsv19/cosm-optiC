#!/usr/bin/env python3
"""Exercise PSG-14 authored material and region-reference transactions."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = (
    ROOT / "tests" / "fixtures" / "procedural_solid_graphs"
    / "rounded_block_with_tunnel.json"
)


def run(command: list[str], expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if expect_success and result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    if not expect_success and result.returncode == 0:
        raise RuntimeError(f"command unexpectedly succeeded: {' '.join(command)}")
    return result


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: test_procedural_solid_psg14_flow.py "
            "SOLID_TOOL REGION_BINDING_TOOL MATERIAL_TOOL AUTHORED_BINDING_TOOL"
        )
    solid_tool = Path(sys.argv[1]).resolve()
    region_tool = Path(sys.argv[2]).resolve()
    material_tool = Path(sys.argv[3]).resolve()
    authored_binding_tool = Path(sys.argv[4]).resolve()
    with tempfile.TemporaryDirectory(prefix="psg14_flow_") as temp:
        root = Path(temp)
        mesh = root / "tunnel.runtime.json"
        solid_receipt = root / "tunnel.receipt.json"
        run([
            str(solid_tool), "--graph", str(FIXTURE),
            "--out", str(mesh), "--summary-out", str(solid_receipt),
            "--asset-id", "psg14_tunnel", "--cells", "20",
            "--local-adaptive", "--maximum-cells", "40",
            "--feature-size", "0.18",
            "--collision-authority", "derived_shell",
        ])
        mesh_digest_before = digest(mesh)

        region_base = root / "region.base.json"
        region_init = json.loads(run([
            str(region_tool), "init", "--mesh", str(mesh),
            "--solid-receipt", str(solid_receipt),
            "--binding-id", "psg14_regions", "--fallback", "default",
            "--out", str(region_base),
        ]).stdout)
        region_binding = root / "region.binding.json"
        run([
            str(region_tool), "apply", "--mesh", str(mesh),
            "--binding", str(region_base),
            "--expected-base-digest", region_init["binding_digest_sha256"],
            "--set-kind", "retained=rough_metal",
            "--set-kind", "cut=glossy",
            "--out", str(region_binding),
            "--undo-out", str(root / "region.undo.json"),
        ])

        concrete = root / "concrete.material.json"
        concrete_init = json.loads(run([
            str(material_tool), "init", "--template", "pitted_concrete",
            "--material-id", "concrete_custom", "--out", str(concrete),
        ]).stdout)
        assert concrete_init["texture_node"]["kind"] == "concrete"
        assert len(concrete_init["parameters"]) >= 20

        concrete_edited = root / "concrete.edited.json"
        concrete_undo = root / "concrete.undo.json"
        concrete_apply = json.loads(run([
            str(material_tool), "apply", "--material", str(concrete),
            "--expected-base-digest",
            concrete_init["material_digest_sha256"],
            "--set", "base_color.r=0.31",
            "--set", "base_color.g=0.36",
            "--set", "base_color.b=0.42",
            "--set", "roughness=0.91",
            "--set", "texture.scale_units=0.045",
            "--set", "texture.surface_damage=0.84",
            "--out", str(concrete_edited),
            "--undo-out", str(concrete_undo),
        ]).stdout)
        assert concrete_apply["base_color"]["r"] == 0.31
        assert concrete_apply["roughness"] == 0.91

        stale_material = root / "stale.material.json"
        stale_undo = root / "stale.material.undo.json"
        run([
            str(material_tool), "apply", "--material", str(concrete),
            "--expected-base-digest", "0" * 64,
            "--set", "roughness=0.2", "--out", str(stale_material),
            "--undo-out", str(stale_undo),
        ], expect_success=False)
        assert not stale_material.exists()
        assert not stale_undo.exists()

        restored_material = root / "concrete.restored.json"
        restored_receipt = json.loads(run([
            str(material_tool), "restore", "--material", str(concrete_edited),
            "--restore", str(concrete_undo),
            "--expected-base-digest",
            concrete_apply["material_digest_sha256"],
            "--out", str(restored_material),
        ]).stdout)
        assert restored_receipt["material_digest_sha256"] == concrete_init[
            "material_digest_sha256"
        ]

        crystal = root / "crystal.material.json"
        run([
            str(material_tool), "init", "--template", "emissive_crystal",
            "--material-id", "crystal_cut", "--out", str(crystal),
        ])

        authored_base = root / "authored.base.json"
        authored_init = json.loads(run([
            str(authored_binding_tool), "init", "--mesh", str(mesh),
            "--region-binding", str(region_binding),
            "--binding-id", "psg14_authored", "--out", str(authored_base),
        ]).stdout)
        authored_binding = root / "authored.binding.json"
        authored_undo = root / "authored.undo.json"
        authored_apply = json.loads(run([
            str(authored_binding_tool), "apply", "--mesh", str(mesh),
            "--region-binding", str(region_binding),
            "--authored-binding", str(authored_base),
            "--expected-base-digest",
            authored_init["binding_digest_sha256"],
            "--set-kind", f"retained={concrete_edited}",
            "--set-kind", f"cut={crystal}",
            "--out", str(authored_binding),
            "--undo-out", str(authored_undo),
        ]).stdout)
        assert authored_apply["assignment_count"] == 2
        assert {
            entry["material_id"] for entry in authored_apply["assignments"]
        } == {"concrete_custom", "crystal_cut"}

        compiled = root / "compiled.json"
        compile_receipt = json.loads(run([
            str(authored_binding_tool), "compile", "--mesh", str(mesh),
            "--region-binding", str(region_binding),
            "--authored-binding", str(authored_binding),
            "--out", str(compiled),
        ]).stdout)
        assert json.loads(compiled.read_text()) == compile_receipt
        assert digest(mesh) == mesh_digest_before

        tampered_material = json.loads(concrete_edited.read_text())
        tampered_material["surface"]["roughness"] = 0.17
        concrete_edited.write_text(json.dumps(tampered_material, indent=2))
        tampered_compile = root / "tampered.compile.json"
        run([
            str(authored_binding_tool), "compile", "--mesh", str(mesh),
            "--region-binding", str(region_binding),
            "--authored-binding", str(authored_binding),
            "--out", str(tampered_compile),
        ], expect_success=False)
        assert not tampered_compile.exists()
        assert digest(mesh) == mesh_digest_before

    print("PSG-14 authored material and binding agent flow passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
