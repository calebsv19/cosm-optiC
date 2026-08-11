#!/usr/bin/env python3
"""PSG-19 fresh-STL, digest binding, and deterministic region carrier proof."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(command: list[str], *, success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError(f"unexpected exit {result.returncode}: {' '.join(command)}")
    return result


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} REGION_TOOL STL_TOOL IMPORT_HARNESS", file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parents[2]
    region_tool = pathlib.Path(sys.argv[1]).resolve()
    stl_tool = pathlib.Path(sys.argv[2]).resolve()
    harness = pathlib.Path(sys.argv[3]).resolve()
    fixture = root / "tests/fixtures/procedural_imported_surface_region_psg19"
    with tempfile.TemporaryDirectory(prefix="psg19_imported_region_") as temp:
        out = pathlib.Path(temp)
        authored = out / "authored"
        run([
            sys.executable, str(stl_tool), "create",
            "--recipe", str(fixture / "statue_fragment.recipe.json"),
            "--out-root", str(authored),
        ])
        stl = authored / "curated/psg19_plaster_statue_fragment/source/psg19_plaster_statue_fragment.stl"
        assert stl.exists()
        imported = out / "imported"
        run([
            str(harness), "--stl", str(stl), "--out", str(imported),
            "--asset-id", "psg19_plaster_statue_fragment",
            "--scene-id", "psg19_imported_surface_region",
            "--object-id", "psg19_statue",
        ])
        mesh = imported / "assets/mesh_assets/psg19_plaster_statue_fragment.runtime.json"
        assert mesh.exists()
        mesh_json = json.loads(mesh.read_text(encoding="utf-8"))
        mesh_sha = sha(mesh)
        artifact_a = out / "region_a.json"
        artifact_b = out / "region_b.json"
        receipt_a = out / "region_a.receipt.json"
        receipt_b = out / "region_b.receipt.json"
        solid_receipt = out / "solid.receipt.json"
        for artifact, receipt in ((artifact_a, receipt_a), (artifact_b, receipt_b)):
            run([
                str(region_tool), "--mesh", str(mesh),
                "--recipe", str(fixture / "plaster_peel.region_recipe.json"),
                "--out", str(artifact), "--summary-out", str(receipt),
                "--solid-receipt-out", str(solid_receipt),
            ])
        a = json.loads(receipt_a.read_text(encoding="utf-8"))
        b = json.loads(receipt_b.read_text(encoding="utf-8"))
        assert a == b
        assert sha(artifact_a) == sha(artifact_b)
        assert a["source_file_digest_sha256"] == mesh_sha
        assert a["vertex_count"] == mesh_json["mesh"]["vertex_count"]
        assert a["triangle_count"] == mesh_json["mesh"]["triangle_count"]
        assert a["topology_unchanged"] is True
        assert a["source_triangle_provenance_retained"] is True
        assert a["minimum"] <= 0.01
        assert a["maximum"] >= 0.85
        assert a["transition_vertex_count"] >= 16
        assert mesh_sha == sha(mesh)

        stale_mesh = out / "stale.runtime.json"
        stale = json.loads(mesh.read_text(encoding="utf-8"))
        stale["mesh"]["vertices"][0]["x"] += 0.001
        stale_mesh.write_text(json.dumps(stale), encoding="utf-8")
        stale_artifact = out / "stale_region.json"
        run([
            str(region_tool), "--mesh", str(stale_mesh),
            "--recipe", str(fixture / "plaster_peel.region_recipe.json"),
            "--out", str(stale_artifact),
        ])
        assert json.loads(stale_artifact.read_text(encoding="utf-8"))[
            "source_mesh_digest_sha256"
        ] != a["source_mesh_digest_sha256"]
        print(json.dumps({
            "status": "ok",
            "fixture_source": "fresh_generated_stl",
            "source_mesh_digest_sha256": a["source_mesh_digest_sha256"],
            "source_file_digest_sha256": a["source_file_digest_sha256"],
            "value_digest_sha256": a["value_digest_sha256"],
            "vertex_count": a["vertex_count"],
            "triangle_count": a["triangle_count"],
            "transition_vertex_count": a["transition_vertex_count"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
