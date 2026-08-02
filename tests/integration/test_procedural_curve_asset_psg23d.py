#!/usr/bin/env python3
"""Contract tests for PSG-23D curve authoring and runtime scene ingestion."""

from __future__ import annotations

import copy
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], expect_success: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if expect_success and result.returncode:
        raise AssertionError(
            f"command failed: {' '.join(command)}\n{result.stdout}{result.stderr}")
    if not expect_success and result.returncode == 0:
        raise AssertionError(f"command unexpectedly passed: {' '.join(command)}")
    return result


def write(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def scene(asset: Path, asset_sha: str, scale: tuple[float, float, float]) -> dict:
    return {
        "schema_family": "codework_scene",
        "schema_variant": "scene_runtime_v1",
        "schema_version": 1,
        "scene_id": "psg23d_ingestion_contract",
        "space_mode_default": "3d",
        "unit_system": "meters",
        "world_scale": 1.0,
        "objects": [{
            "object_id": "contract_curves",
            "object_type": "curve_asset_instance",
            "dimensional_mode": "full_3d",
            "transform": {
                "position": {"x": 0.0, "y": 0.0, "z": 0.0},
                "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
                "scale": {"x": scale[0], "y": scale[1], "z": scale[2]},
            },
            "geometry_ref": {
                "kind": "curve_asset",
                "id": "psg23d_baseline_fiber_field",
                "runtime_path": asset.name,
                "sha256": asset_sha,
            },
            "material_ref": {"id": "contract_material"},
            "flags": {"visible": True, "locked": False, "selectable": True},
        }],
        "materials": [{
            "id": "contract_material",
            "name": "PSG-23D contract material",
            "base_color": {"r": 0.75, "g": 0.28, "b": 0.08},
            "roughness": 0.7,
            "metallic": 0.0,
        }],
        "lights": [],
        "extensions": {},
    }


def request(scene_path: Path, output: Path) -> dict:
    return {
        "schema_version": "ray_tracing_agent_render_request_v1",
        "run_id": "psg23d_ingestion_contract",
        "scene": {"runtime_scene_path": scene_path.name},
        "volume": {"enabled": False},
        "render": {
            "start_frame": 0,
            "frame_count": 1,
            "width": 160,
            "height": 128,
            "normalized_t": 0.0,
            "temporal_frames": 1,
            "integrator_3d": "disney_v2",
            "denoise_enabled": False,
        },
        "inspection": {
            "camera_position": {"x": 2.1, "y": -2.7, "z": 1.8},
            "camera_look_at": {"x": 0.0, "y": 0.0, "z": 0.4},
            "camera_zoom": 1.0,
            "light_mode": 2,
            "ambient_strength": 0.35,
            "key_intensity": 1.5,
            "top_fill_strength": 0.8,
            "object_audit_enabled": True,
        },
        "output": {"root": str(output), "overwrite": True},
        "progress": {
            "summary_path": str(output / "render_summary.json"),
            "progress_path": str(output / "render_progress.json"),
        },
    }


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: test_procedural_curve_asset_psg23d.py "
            "AUTHORING_TOOL RENDER_CLI BASELINE")
    tool = Path(sys.argv[1]).resolve()
    render_cli = Path(sys.argv[2]).resolve()
    baseline_path = Path(sys.argv[3]).resolve()
    with tempfile.TemporaryDirectory(prefix="psg23d_contract_") as temp_text:
        temp = Path(temp_text)
        first = temp / "curve.runtime.json"
        second = temp / "curve.repeat.runtime.json"
        for output in (first, second):
            run([
                sys.executable, str(tool), "generate",
                "--authoring", str(baseline_path), "--output", str(output)])
        assert first.read_bytes() == second.read_bytes()

        inspect = json.loads(run([
            sys.executable, str(tool), "inspect",
            "--input", str(baseline_path)]).stdout)
        assert set(inspect["editable_handles"]) == {
            "density", "spacing", "length", "direction", "shape", "profile"}

        baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
        baseline_digest = hashlib.sha256(
            (json.dumps(
                baseline, sort_keys=True, separators=(",", ":")) + "\n"
             ).encode("utf-8")).hexdigest()
        edited = temp / "edited.authoring.json"
        run([
            sys.executable, str(tool), "edit",
            "--input", str(baseline_path), "--output", str(edited),
            "--expect-sha256", baseline_digest,
            "--set", "layout.rows=7", "--set", "strand.length=0.9"])
        edited_document = json.loads(edited.read_text(encoding="utf-8"))
        assert edited_document["layout"]["rows"] == 7
        assert edited_document["strand"]["length"] == 0.9
        run([
            sys.executable, str(tool), "edit",
            "--input", str(baseline_path), "--output", str(edited),
            "--expect-sha256", "0" * 64, "--set", "layout.rows=8"],
            expect_success=False)

        scene_path = temp / "scene.json"
        request_path = temp / "request.json"
        render_output = temp / "render"
        write(scene_path, scene(first, sha(first), (1.0, 1.0, 1.0)))
        write(request_path, request(scene_path, render_output))
        run([str(render_cli), "--request", str(request_path)])
        summary = json.loads(
            (render_output / "render_summary.json").read_text(encoding="utf-8"))
        audit = next(
            item for item in summary["object_audit"]
            if item["object_id"] == "contract_curves")
        assert audit["primary_hit_pixels"] > 100

        tampered = json.loads(first.read_text(encoding="utf-8"))
        tampered["strands"][0]["points"][0]["radius"] = 0.0
        malformed = temp / "malformed.runtime.json"
        write(malformed, tampered)
        write(scene_path, scene(malformed, sha(malformed), (1.0, 1.0, 1.0)))
        malformed_result = run(
            [str(render_cli), "--request", str(request_path)],
            expect_success=False)
        assert "radius invalid" in (
            malformed_result.stdout + malformed_result.stderr)

        write(scene_path, scene(first, sha(first), (1.0, 2.0, 1.0)))
        nonuniform_result = run(
            [str(render_cli), "--request", str(request_path)],
            expect_success=False)
        assert "uniform scale" in (
            nonuniform_result.stdout + nonuniform_result.stderr)

        digest_bound_scene = scene(first, "f" * 64, (1.0, 1.0, 1.0))
        write(scene_path, digest_bound_scene)
        digest_result = run(
            [str(render_cli), "--request", str(request_path)],
            expect_success=False)
        assert "sha256 mismatch" in (
            digest_result.stdout + digest_result.stderr)
    print("PSG-23D serialized curve asset contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
