#!/usr/bin/env python3
"""Generate the dedicated smooth high-resolution acceptance sphere."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))

from generate_mesh_asset_sphere_fixtures import make_sphere  # noqa: E402


ASSET_ID = "asset_sphere_256x128_smooth_acceptance"


def main() -> int:
    asset = make_sphere(ASSET_ID, 256, 128, "ppm_acceptance_smooth_high")
    mesh = asset["mesh"]
    mesh["normal_count"] = mesh["vertex_count"]
    mesh["normal_provenance"] = "generated_smooth"
    mesh["normals"] = [dict(vertex) for vertex in mesh["vertices"]]
    asset["compile_meta"]["normal_policy"] = "analytic_outward_unit_normal"
    asset["compile_meta"]["acceptance_role"] = "ppm_charcoal_lens"
    output = (Path(__file__).resolve().parent / "assets" / "mesh_assets" /
              f"{ASSET_ID}.runtime.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(asset, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "asset_path": str(output),
        "vertex_count": mesh["vertex_count"],
        "normal_count": mesh["normal_count"],
        "triangle_count": mesh["triangle_count"],
        "normal_provenance": mesh["normal_provenance"],
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
