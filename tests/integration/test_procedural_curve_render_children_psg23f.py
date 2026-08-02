#!/usr/bin/env python3
"""Focused PSG-23F guide-to-render-child density and LOD contract."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from procedural_surface_visual_proof import (  # noqa: E402
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


def load(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_children(
    tool: pathlib.Path,
    authoring: pathlib.Path,
    guides: pathlib.Path,
    mesh: pathlib.Path,
    root: pathlib.Path,
    name: str,
    lod: str,
    *,
    success: bool = True,
) -> tuple[pathlib.Path, pathlib.Path]:
    runtime = root / f"{name}.{lod}.curve_runtime.json"
    receipt = root / f"{name}.{lod}.receipt.json"
    run([
        sys.executable, str(tool), "compile",
        "--authoring", str(authoring),
        "--guide-asset", str(guides),
        "--mesh", str(mesh),
        "--lod", lod,
        "--output", str(runtime),
        "--receipt", str(receipt),
    ], success=success)
    return runtime, receipt


def scene(runtime: pathlib.Path) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "psg23f_focused_dense_hair",
        "source_scene_id": "psg23f_focused_dense_hair",
        "compile_meta": {
            "compiler_version": "psg23f_focused_contract",
            "compiled_at_ns": 0,
            "normalization": "guide_to_thin_render_children",
        },
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [
            {
                "object_id": "psg23f_scalp",
                "object_type": "mesh_asset_instance",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
                "geometry_ref": {
                    "kind": "mesh_asset", "id": "psg23a_scalp_bust"},
                "material_ref": {"id": "scalp_material"},
                "flags": {
                    "visible": True, "locked": False, "selectable": True},
            },
            {
                "object_id": "psg23f_hair",
                "object_type": "curve_asset_instance",
                "dimensional_mode": "full_3d",
                "transform": {
                    "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
                },
                "geometry_ref": {
                    "kind": "curve_asset",
                    "id": "psg23f_focused_children",
                    "runtime_path": str(runtime.resolve()),
                    "sha256": sha(runtime),
                },
                "material_ref": {"id": "hair_material"},
                "flags": {
                    "visible": True, "locked": False, "selectable": True},
            },
        ],
        "materials": [
            {
                "id": "scalp_material",
                "name": "Neutral scalp",
                "base_color": {"r": 0.20, "g": 0.16, "b": 0.14},
                "roughness": 0.90,
                "metallic": 0.0,
            },
            {
                "id": "hair_material",
                "name": "Fine chestnut diagnostic hair",
                "base_color": {"r": 0.52, "g": 0.16, "b": 0.055},
                "roughness": 0.42,
                "metallic": 0.0,
            },
        ],
        "lights": [],
        "extensions": {},
    }


def main() -> int:
    if len(sys.argv) != 8:
        print(
            f"usage: {sys.argv[0]} CHILD_TOOL GROOM_TOOL REGION_TOOL "
            "STL_TOOL IMPORT_HARNESS RENDER_CLI FIXTURE_DIR",
            file=sys.stderr,
        )
        return 2
    child_tool = pathlib.Path(sys.argv[1]).resolve()
    groom_tool = pathlib.Path(sys.argv[2]).resolve()
    region_tool = pathlib.Path(sys.argv[3]).resolve()
    stl_tool = pathlib.Path(sys.argv[4]).resolve()
    harness = pathlib.Path(sys.argv[5]).resolve()
    render_cli = pathlib.Path(sys.argv[6]).resolve()
    fixture = pathlib.Path(sys.argv[7]).resolve()
    with tempfile.TemporaryDirectory(prefix="psg23f_children_") as temporary:
        out = pathlib.Path(temporary)
        authored = out / "authored"
        run([
            sys.executable, str(stl_tool), "create",
            "--recipe", str(fixture / "scalp_bust.recipe.json"),
            "--out-root", str(authored),
        ])
        stl = (
            authored / "curated/psg23a_scalp_bust"
            / "source/psg23a_scalp_bust.stl")
        imported = out / "scene"
        run([
            str(harness), "--stl", str(stl), "--out", str(imported),
            "--asset-id", "psg23a_scalp_bust",
            "--scene-id", "psg23f_focused",
            "--object-id", "psg23f_scalp",
        ])
        mesh = (
            imported / "assets/mesh_assets"
            / "psg23a_scalp_bust.runtime.json")
        source_sha = sha(mesh)
        region = out / "scalp_hair.region.json"
        run([
            str(region_tool),
            "--mesh", str(mesh),
            "--recipe", str(fixture / "scalp_hair.region_recipe.json"),
            "--out", str(region),
            "--summary-out", str(out / "region.receipt.json"),
        ])
        groom = out / "guides.groom.json"
        run([
            sys.executable, str(groom_tool), "init",
            "--mesh", str(mesh), "--region", str(region),
            "--asset-id", "psg23f_focused_guides",
            "--output", str(groom),
            "--set", "groom.strand_count=64",
            "--set", "groom.guide_count=8",
            "--set", "groom.points_per_strand=9",
            "--set", "groom.root_radius=0.009",
            "--set", "groom.tip_radius=0.002",
        ])
        guides = out / "guides.curve_runtime.json"
        guide_receipt = out / "guides.receipt.json"
        run([
            sys.executable, str(groom_tool), "compile",
            "--authoring", str(groom),
            "--mesh", str(mesh), "--region", str(region),
            "--output", str(guides), "--receipt", str(guide_receipt),
        ])
        guide_sha = sha(guides)
        default_authoring = out / "children.defaults.json"
        run([
            sys.executable, str(child_tool), "init",
            "--guide-asset", str(guides),
            "--mesh", str(mesh),
            "--asset-id", "psg23f_default_density",
            "--output", str(default_authoring),
        ])
        default_document = load(default_authoring)
        assert default_document["lod"] == {
            "preview_children_per_parent": 4,
            "interactive_children_per_parent": 16,
            "final_children_per_parent": 48,
        }
        authoring = out / "children.authoring.json"
        run([
            sys.executable, str(child_tool), "init",
            "--guide-asset", str(guides),
            "--mesh", str(mesh),
            "--asset-id", "psg23f_focused_children",
            "--output", str(authoring),
            "--set", "lod.preview_children_per_parent=2",
            "--set", "lod.interactive_children_per_parent=5",
            "--set", "lod.final_children_per_parent=10",
            "--set", "children.root_radius_scale=0.20",
            "--set", "children.tip_radius_scale=0.10",
        ])
        inspected = json.loads(run([
            sys.executable, str(child_tool), "inspect",
            "--input", str(authoring),
        ]))
        assert "lod.final_children_per_parent" in inspected["editable_fields"]
        assert "children.root_radius_scale" in inspected["editable_fields"]

        outputs = {}
        receipts = {}
        for lod, expected_count in (
            ("preview", 128), ("interactive", 320), ("final", 640),
        ):
            runtime, receipt = compile_children(
                child_tool, authoring, guides, mesh, out, lod, lod)
            outputs[lod] = load(runtime)
            receipts[lod] = load(receipt)
            assert receipts[lod]["render_child_count"] == expected_count
            assert receipts[lod]["primitive_count"] == expected_count * 8
            assert receipts[lod]["exact_guide_and_source_binding"] is True
            assert receipts[lod]["parents_included_as_render_curves"] is False
            assert receipts[lod]["hair_bsdf_added"] is False

        final_repeat, final_repeat_receipt = compile_children(
            child_tool, authoring, guides, mesh, out, "repeat", "final")
        assert sha(final_repeat) == sha(out / "final.final.curve_runtime.json")
        assert load(final_repeat_receipt) == receipts["final"]
        assert sha(mesh) == source_sha
        assert sha(guides) == guide_sha

        final_by_id = {
            strand["render_child_id"]: strand
            for strand in outputs["final"]["strands"]}
        for lod in ("preview", "interactive"):
            for strand in outputs[lod]["strands"]:
                reference = final_by_id[strand["render_child_id"]]
                assert strand["parent_strand_index"] == \
                    reference["parent_strand_index"]
                assert strand["child_index"] == reference["child_index"]
                assert strand["root_barycentrics"] == \
                    reference["root_barycentrics"]
                assert strand["points"] == reference["points"]
        guide_document = load(guides)
        parent_max_radius = max(
            point["radius"]
            for strand in guide_document["strands"]
            for point in strand["points"])
        assert receipts["final"]["maximum_radius"] < parent_max_radius * 0.21
        for strand in outputs["final"]["strands"]:
            barycentrics = strand["root_barycentrics"]
            assert abs(sum(barycentrics) - 1.0) < 2.0e-11
            assert all(0.0 < value < 1.0 for value in barycentrics)
            radii = [point["radius"] for point in strand["points"]]
            assert all(radius > 0.0 for radius in radii)
            assert radii[-1] < radii[0] * 0.15

        run([
            sys.executable, str(child_tool), "edit",
            "--input", str(authoring),
            "--output", str(out / "stale.json"),
            "--expect-sha256", "0" * 64,
            "--set", "lod.final_children_per_parent=12",
        ], success=False)
        run([
            sys.executable, str(child_tool), "edit",
            "--input", str(authoring),
            "--output", str(out / "invalid.json"),
            "--set", "lod.preview_children_per_parent=12",
        ], success=False)
        tampered_guides = out / "tampered.guides.json"
        tampered = load(guides)
        tampered["strands"][0]["points"][1]["position"]["x"] += 0.001
        write_json(tampered_guides, tampered)
        compile_children(
            child_tool, authoring, tampered_guides, mesh, out,
            "tampered", "final", success=False)

        final_runtime = out / "final.final.curve_runtime.json"
        scene_path = imported / "psg23f.scene.json"
        request_path = imported / "psg23f.request.json"
        write_json(scene_path, scene(final_runtime))
        raw = imported / "render"
        write_json(request_path, render_request(
            "psg23f_focused_dense_hair",
            {
                "id": "psg23f_focused",
                "camera_position": {"x": 1.72, "y": -2.08, "z": 2.52},
                "camera_look_at": {"x": 0.0, "y": 0.0, "z": 1.60},
            },
            scene_path, request_path, raw, {
                "render": {
                    "width": 640, "height": 480,
                    "temporal_frames": 1,
                    "integrator_3d": "disney_v2",
                    "camera_zoom": 1.35,
                },
                "lighting": {
                    "light_mode": 2,
                    "environment_light_mode": "ambient",
                    "ambient_strength": 0.72,
                    "light_intensity": 5.2,
                    "light_radius": 0.24,
                    "top_fill_strength": 1.4,
                },
            }))
        summary_path = raw / "render_summary.json"
        run_render_cli(render_cli, request_path, summary_path)
        summary = load(summary_path)
        scalp_audit = object_audit(summary, "psg23f_scalp")
        hair_audit = object_audit(summary, "psg23f_hair")
        assert scalp_audit["primary_hit_pixels"] > 100
        assert hair_audit["primary_hit_pixels"] > 1000
        print(json.dumps({
            "status": "ok",
            "parent_strand_count": receipts["final"]["parent_strand_count"],
            "render_child_count": receipts["final"]["render_child_count"],
            "primitive_count": receipts["final"]["primitive_count"],
            "maximum_radius": receipts["final"]["maximum_radius"],
            "hair_primary_hit_pixels": hair_audit["primary_hit_pixels"],
            "native_prepare_frame_ms":
                summary["timing_breakdown"]["native_prepare_frame_ms"],
            "render_trace_ms":
                summary["timing_breakdown"]["render_trace_ms"],
        }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
