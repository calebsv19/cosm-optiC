#!/usr/bin/env python3
"""Focused PSG-23E carrier-aware guide/clump authoring contract."""

from __future__ import annotations

import hashlib
import json
import math
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from procedural_surface_visual_proof import (
    object_audit,
    render_request,
    run_render_cli,
    write_json,
)


def run(command: list[str], *, success: bool = True) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if (result.returncode == 0) != success:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise AssertionError(
            f"unexpected exit {result.returncode}: {' '.join(command)}")
    return result.stdout


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def distance(a: list[float], b: list[float]) -> float:
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def compile_groom(
    tool: pathlib.Path,
    authoring: pathlib.Path,
    mesh: pathlib.Path,
    region: pathlib.Path,
    root: pathlib.Path,
    name: str,
    *,
    success: bool = True,
) -> tuple[pathlib.Path, pathlib.Path]:
    runtime = root / f"{name}.curve_runtime.json"
    receipt = root / f"{name}.receipt.json"
    run([
        sys.executable, str(tool), "compile",
        "--authoring", str(authoring),
        "--mesh", str(mesh),
        "--region", str(region),
        "--output", str(runtime),
        "--receipt", str(receipt),
    ], success=success)
    return runtime, receipt


def mesh_object(asset_id: str) -> dict:
    return {
        "object_id": "psg23e_scalp",
        "object_type": "mesh_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {"kind": "mesh_asset", "id": asset_id},
        "material_ref": {"id": "scalp_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def curve_object(asset_id: str, runtime: pathlib.Path) -> dict:
    return {
        "object_id": "psg23e_curves",
        "object_type": "curve_asset_instance",
        "dimensional_mode": "full_3d",
        "transform": {
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
        },
        "geometry_ref": {
            "kind": "curve_asset",
            "id": asset_id,
            "runtime_path": str(runtime.resolve()),
            "sha256": sha(runtime),
        },
        "material_ref": {"id": "hair_material"},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def scene(asset_id: str, runtime: pathlib.Path) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "psg23e_focused_mixed_scalp",
        "source_scene_id": "psg23e_focused_mixed_scalp",
        "compile_meta": {
            "compiler_version": "psg23e_focused_contract",
            "compiled_at_ns": 0,
            "normalization": "carrier_aware_guide_clump_curves",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [mesh_object("psg23a_scalp_bust"),
                    curve_object(asset_id, runtime)],
        "materials": [
            {
                "id": "scalp_material",
                "name": "Cool scalp carrier",
                "base_color": {"r": 0.18, "g": 0.24, "b": 0.34},
                "roughness": 0.88,
                "metallic": 0.0,
            },
            {
                "id": "hair_material",
                "name": "Bright gold diagnostic hair",
                "base_color": {"r": 0.95, "g": 0.58, "b": 0.10},
                "roughness": 0.46,
                "metallic": 0.0,
            },
        ],
        "lights": [],
        "extensions": {},
    }


def main() -> int:
    if len(sys.argv) != 7:
        print(
            f"usage: {sys.argv[0]} GROOM_TOOL REGION_TOOL STL_TOOL "
            "IMPORT_HARNESS RENDER_CLI FIXTURE_DIR",
            file=sys.stderr,
        )
        return 2
    groom_tool = pathlib.Path(sys.argv[1]).resolve()
    region_tool = pathlib.Path(sys.argv[2]).resolve()
    stl_tool = pathlib.Path(sys.argv[3]).resolve()
    harness = pathlib.Path(sys.argv[4]).resolve()
    render_cli = pathlib.Path(sys.argv[5]).resolve()
    fixture = pathlib.Path(sys.argv[6]).resolve()
    with tempfile.TemporaryDirectory(prefix="psg23e_groom_") as temporary:
        out = pathlib.Path(temporary)
        authored = out / "authored"
        run([
            sys.executable, str(stl_tool), "create",
            "--recipe", str(fixture / "scalp_bust.recipe.json"),
            "--out-root", str(authored),
        ])
        stl = (
            authored / "curated/psg23a_scalp_bust"
            / "source/psg23a_scalp_bust.stl"
        )
        imported = out / "scene"
        run([
            str(harness), "--stl", str(stl), "--out", str(imported),
            "--asset-id", "psg23a_scalp_bust",
            "--scene-id", "psg23e_focused",
            "--object-id", "psg23e_scalp",
        ])
        mesh = (
            imported / "assets/mesh_assets"
            / "psg23a_scalp_bust.runtime.json"
        )
        source_sha = sha(mesh)
        region = out / "scalp_hair.region.json"
        run([
            str(region_tool),
            "--mesh", str(mesh),
            "--recipe", str(fixture / "scalp_hair.region_recipe.json"),
            "--out", str(region),
            "--summary-out", str(out / "region.receipt.json"),
        ])
        authoring = out / "baseline.groom.json"
        run([
            sys.executable, str(groom_tool), "init",
            "--mesh", str(mesh), "--region", str(region),
            "--asset-id", "psg23e_focused_groom",
            "--output", str(authoring),
            "--set", "groom.strand_count=64",
            "--set", "groom.guide_count=8",
            "--set", "groom.points_per_strand=9",
        ])
        inspect = json.loads(run([
            sys.executable, str(groom_tool), "inspect",
            "--input", str(authoring),
        ]))
        assert "groom.clump_strength" in inspect["editable_fields"]
        assert "groom.part_strength" in inspect["editable_fields"]
        assert inspect["binding"]["source_file_sha256"] == source_sha

        first_runtime, first_receipt = compile_groom(
            groom_tool, authoring, mesh, region, out, "first")
        second_runtime, second_receipt = compile_groom(
            groom_tool, authoring, mesh, region, out, "second")
        assert sha(first_runtime) == sha(second_runtime)
        assert load(first_receipt) == load(second_receipt)
        assert sha(mesh) == source_sha
        runtime = load(first_runtime)
        receipt = load(first_receipt)
        for field in (
            "exact_source_and_carrier_binding",
            "root_triangle_mapping_retained",
            "root_barycentrics_valid",
            "finite_positive_curve_asset",
            "guide_assignment_complete",
            "replaceable_serialized_curve_asset",
        ):
            assert receipt[field] is True, field
        assert receipt["hair_bsdf_added"] is False
        assert receipt["strand_count"] == 64
        assert receipt["guide_count"] == 8
        assert receipt["control_point_count"] == 64 * 9
        assert sum(receipt["clump_histogram"]) == 64
        assert len(runtime["strands"]) == 64
        assert {item["guide_index"] for item in runtime["strands"]} == set(
            range(8))
        mesh_document = load(mesh)
        penetration = load(authoring)["groom"]["root_penetration"]
        for strand in runtime["strands"]:
            triangle = mesh_document["mesh"]["triangles"][
                strand["source_triangle_index"]]
            vertices = [
                mesh_document["mesh"]["vertices"][triangle[key]]
                for key in ("a", "b", "c")
            ]
            centroid = [
                sum(vertex[axis] for vertex in vertices) / 3.0
                for axis in ("x", "y", "z")
            ]
            normal = strand["root_normal"]
            point = strand["points"][0]["position"]
            embedded = [point[axis] for axis in ("x", "y", "z")]
            expected = [
                centroid[index] - normal[index] * penetration
                for index in range(3)
            ]
            assert distance(embedded, expected) < 2.0e-9
            assert abs(sum(strand["root_barycentrics"]) - 1.0) < 2.0e-12
            radii = [point["radius"] for point in strand["points"]]
            assert all(
                radii[index] > radii[index + 1]
                for index in range(len(radii) - 1))

        strong = out / "strong.groom.json"
        run([
            sys.executable, str(groom_tool), "edit",
            "--input", str(authoring), "--output", str(strong),
            "--expect-sha256", sha(authoring),
            "--set", "groom.clump_strength=0.95",
            "--set", "groom.clump_tip_spread=0.006",
        ])
        strong_runtime, _ = compile_groom(
            groom_tool, strong, mesh, region, out, "strong")
        strong_document = load(strong_runtime)
        loose = out / "loose.groom.json"
        run([
            sys.executable, str(groom_tool), "edit",
            "--input", str(authoring), "--output", str(loose),
            "--expect-sha256", sha(authoring),
            "--set", "groom.clump_strength=0.0",
        ])
        loose_runtime, _ = compile_groom(
            groom_tool, loose, mesh, region, out, "loose")
        loose_document = load(loose_runtime)

        def tip_spread(document: dict) -> float:
            groups: dict[int, list[list[float]]] = {}
            for strand in document["strands"]:
                point = strand["points"][-1]["position"]
                groups.setdefault(strand["guide_index"], []).append(
                    [point[axis] for axis in ("x", "y", "z")])
            total = 0.0
            count = 0
            for tips in groups.values():
                center = [
                    sum(tip[axis] for tip in tips) / len(tips)
                    for axis in range(3)]
                total += sum(distance(tip, center) for tip in tips)
                count += len(tips)
            return total / count

        assert tip_spread(strong_document) < tip_spread(loose_document) * 0.5
        run([
            sys.executable, str(groom_tool), "edit",
            "--input", str(authoring), "--output", str(out / "stale.json"),
            "--expect-sha256", "0" * 64,
            "--set", "groom.clump_strength=0.8",
        ], success=False)
        run([
            sys.executable, str(groom_tool), "edit",
            "--input", str(authoring), "--output", str(out / "invalid.json"),
            "--set", "groom.guide_count=128",
        ], success=False)
        tampered_mesh = out / "tampered.runtime.json"
        tampered_mesh.write_bytes(mesh.read_bytes())
        tampered = load(tampered_mesh)
        tampered["mesh"]["vertices"][0]["x"] += 0.001
        write_json(tampered_mesh, tampered)
        compile_groom(
            groom_tool, authoring, tampered_mesh, region, out,
            "tampered", success=False)
        tampered_region = out / "tampered.region.json"
        tampered_carrier = load(region)
        tampered_carrier["vertex_weights"][0] = 0.125
        write_json(tampered_region, tampered_carrier)
        compile_groom(
            groom_tool, authoring, mesh, tampered_region, out,
            "tampered_carrier", success=False)

        scene_path = imported / "psg23e.scene.json"
        request_path = imported / "psg23e.request.json"
        write_json(
            scene_path,
            scene("psg23e_focused_groom", first_runtime))
        raw = imported / "render"
        write_json(request_path, render_request(
            "psg23e_focused_mixed_scalp",
            {
                "id": "psg23e_focused",
                "camera_position": {"x": 2.65, "y": -3.15, "z": 2.72},
                "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.58},
            },
            scene_path, request_path, raw, {
                "render": {
                    "width": 640, "height": 480,
                    "temporal_frames": 2,
                    "integrator_3d": "disney_v2",
                    "camera_zoom": 1.0,
                },
                "lighting": {
                    "light_mode": 2,
                    "ambient_strength": 0.34,
                    "key_intensity": 1.55,
                    "top_fill_strength": 0.95,
                },
            }))
        summary_path = raw / "render_summary.json"
        run_render_cli(render_cli, request_path, summary_path)
        summary = load(summary_path)
        scalp_audit = object_audit(summary, "psg23e_scalp")
        curve_audit = object_audit(summary, "psg23e_curves")
        assert scalp_audit["primary_hit_pixels"] > 100
        assert curve_audit["primary_hit_pixels"] > 250
        print(json.dumps({
            "status": "ok",
            "strand_count": receipt["strand_count"],
            "guide_count": receipt["guide_count"],
            "control_point_count": receipt["control_point_count"],
            "curve_primary_hit_pixels": curve_audit["primary_hit_pixels"],
            "runtime_asset_sha256": receipt["runtime_asset_sha256"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
