#!/usr/bin/env python3
"""Focused CLI contract for one-shell signed spot relief."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path


def run(command: list[str], *, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True)
    if expect_success and result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    if not expect_success and result.returncode == 0:
        raise RuntimeError(f"command unexpectedly succeeded: {' '.join(command)}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-tool", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    fixture = root / "tests/fixtures/procedural_surface_feature_relief_psg24r"
    graph = root / "tests/fixtures/procedural_surface_field_presets/pitted_concrete.json"
    recipe = root / "tests/fixtures/procedural_surface_rock_prism_psg0/recipe.json"
    source_digest = "a" * 64

    with tempfile.TemporaryDirectory(prefix="psg24r_relief_") as temporary:
        output = Path(temporary)
        paths = {
            "recipe": output / "recipe.json",
            "asset": output / "runtime_mesh.json",
            "material": output / "material.json",
            "manifest": output / "derived_asset.json",
            "summary": output / "summary.json",
        }
        command = [
            str(args.asset_tool.resolve()),
            "--graph", str(graph),
            "--binding", str(fixture / "positive_y.binding.json"),
            "--base-recipe", str(recipe),
            "--recipe-out", str(paths["recipe"]),
            "--asset-out", str(paths["asset"]),
            "--material-out", str(paths["material"]),
            "--manifest-out", str(paths["manifest"]),
            "--summary-out", str(paths["summary"]),
            "--width", "4.0", "--height", "0.8", "--depth", "4.0",
            "--target-edge", "0.1", "--amplitude", "0.08",
            "--edge-lock", "0.18",
            "--source-asset-id", "semantic_wall_prism",
            "--asset-id", "signed_wall_relief",
            "--selected-face", "positive_y",
            "--surface-feature-field", str(fixture / "signed_wall_spots.field.json"),
            "--feature-source-mesh-digest", source_digest,
            "--relief-scale", "1.0",
        ]
        run(command)
        first_bytes = {name: path.read_bytes() for name, path in paths.items()}
        summary = json.loads(first_bytes["summary"])
        receipt = summary["signed_feature_relief"]
        assert receipt["negative_depth_feature_count"] == 1
        assert receipt["positive_height_feature_count"] == 1
        assert receipt["zero_height_feature_count"] == 1
        assert receipt["negatively_displaced_vertex_count"] > 0
        assert receipt["positively_displaced_vertex_count"] > 0
        assert receipt["minimum_emitted_displacement_units"] < 0.0
        assert receipt["maximum_emitted_displacement_units"] > 0.0
        assert receipt["feature_source_identity_bound"] is True
        assert receipt["one_coherent_derived_shell"] is True
        assert summary["boundary_edge_count"] == 0
        assert summary["connected_component_count"] == 1
        assert summary["euler_characteristic"] == 2
        assert summary["selected_face_shell"][
            "maximum_unselected_face_absolute_displacement_units"
        ] == 0.0

        run(command)
        for name, path in paths.items():
            assert path.read_bytes() == first_bytes[name], name

        stale = command.copy()
        digest_index = stale.index("--feature-source-mesh-digest") + 1
        stale[digest_index] = "c" * 64
        run(stale, expect_success=False)

    print("PSG-24R signed relief CLI contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
