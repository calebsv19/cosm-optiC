#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


RAY_ROOT = Path(__file__).resolve().parents[1]
TOOLS_ROOT = RAY_ROOT / "tools"
FIXTURE_ROOT = RAY_ROOT / "tests" / "fixtures" / "scene_project_worker_snapshot"
sys.path.insert(0, str(TOOLS_ROOT))

from scene_project_contract import (  # noqa: E402
    SceneProjectValidationError,
    validate_explicit_paths,
    validate_project,
)
from scene_project_worker_export import export_scene_plus_physics_cache  # noqa: E402


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class SceneProjectContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory(prefix="physics_trio_scene_contract_")
        self.root = Path(self.tempdir.name)
        self.project = self.root / "original" / "nested" / "project"
        shutil.copytree(FIXTURE_ROOT, self.project)

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_all_three_adapters_validate_canonical_project(self) -> None:
        report = validate_project(self.project)
        self.assertEqual(report["status"], "ok")
        self.assertEqual(set(report["adapters"]), {"line_drawing", "physics_sim", "ray_tracing"})
        self.assertEqual(report["adapters"]["line_drawing"]["mesh_asset_ids"], ["fixture_mesh"])
        self.assertEqual(report["adapters"]["physics_sim"]["retained_frame_indices"], [0, 2, 4])
        self.assertEqual(report["adapters"]["ray_tracing"]["selected_frame_indices"], [0, 2])
        relpaths = {entry["relpath"] for entry in report["content"]["files"]}
        self.assertIn("assets/mesh_assets/fixture_mesh.runtime.json", relpaths)
        self.assertIn("assets/vf3d/active/frame_000004.vf3d", relpaths)
        self.assertIn("assets/physics/active/water_surface_000004.json", relpaths)

    def test_staged_line_validation_does_not_require_downstream_outputs(self) -> None:
        shutil.rmtree(self.project / "physics_sim")
        shutil.rmtree(self.project / "ray_tracing")
        report = validate_project(self.project, through="line_drawing")
        self.assertEqual(report["validated_through"], "line_drawing")
        self.assertEqual(set(report["adapters"]), {"line_drawing"})
        with self.assertRaisesRegex(SceneProjectValidationError, "physics_cache"):
            validate_project(self.project, through="physics_sim")

    def test_relocated_project_has_identical_content_and_package_identity(self) -> None:
        original_report = validate_project(self.project)
        relocated = self.root / "relocated" / "different" / "portable_root"
        shutil.copytree(self.project, relocated)
        relocated_report = validate_project(relocated)
        self.assertNotEqual(original_report["project_root"], relocated_report["project_root"])
        self.assertEqual(original_report["content"], relocated_report["content"])

        original_export = export_scene_plus_physics_cache(
            project_root=self.project,
            output_root=self.root / "queue_original",
            item_name="h1-golden",
            job_id="ray-tracing--h1-golden--20260713T000000Z--golden01",
            ray_version="0.6.2",
            physics_version="0.3.1",
        )
        relocated_export = export_scene_plus_physics_cache(
            project_root=relocated,
            output_root=self.root / "queue_relocated",
            item_name="h1-golden",
            job_id="ray-tracing--h1-golden--20260713T000000Z--golden01",
            ray_version="0.6.2",
            physics_version="0.3.1",
        )
        original_manifest = read_json(Path(original_export["manifest_path"]))
        relocated_manifest = read_json(Path(relocated_export["manifest_path"]))
        self.assertEqual(
            original_manifest["source_project_content_sha256"],
            relocated_manifest["source_project_content_sha256"],
        )
        self.assertEqual(
            original_manifest["source_project_content_file_count"],
            relocated_manifest["source_project_content_file_count"],
        )
        worker_request = read_json(
            Path(relocated_export["bundle_root"]) / "render_request.json"
        )
        self.assertEqual(worker_request["scene"]["runtime_scene_path"], "scene_runtime.json")
        self.assertEqual(
            worker_request["volume"]["source_path"],
            "assets/physics/active/scene_bundle.json",
        )

    def test_forward_unknown_fields_are_preserved_and_accepted(self) -> None:
        for relpath in (
            "scene_project.json",
            "scene_authoring.json",
            "scene_runtime.json",
            "object_manifest.json",
            "physics_sim/active_cache_manifest.json",
            "ray_tracing/render_request.json",
        ):
            path = self.project / relpath
            value = read_json(path)
            value["future_contract_extension"] = {"version": 99, "preserve": True}
            write_json(path, value)
        report = validate_project(self.project)
        self.assertEqual(report["status"], "ok")
        self.assertTrue(
            read_json(self.project / "ray_tracing/render_request.json")
            ["future_contract_extension"]["preserve"]
        )

    def test_line_drawing_top_level_pointer_and_manifest_shape_is_accepted(self) -> None:
        project_path = self.project / "scene_project.json"
        project = read_json(project_path)
        active = project.pop("active")
        project.pop("scene_id")
        project["authoring_scene"] = active["scene_authoring"]
        project["runtime_scene"] = active["scene_runtime"]
        project["object_manifest"] = active["object_manifest"]
        project["active_cache"] = active["physics_cache"]
        project["active_render_request"] = active["render_request"]
        write_json(project_path, project)

        manifest_path = self.project / "object_manifest.json"
        manifest = read_json(manifest_path)
        manifest["schema"] = "line_drawing_object_manifest_v1"
        manifest.pop("scene_id")
        manifest["objects"] = [
            {
                "id": "fixture_mesh_object",
                "kind": "mesh_asset_instance",
                "mesh_asset_id": "fixture_mesh",
                "mesh_sidecar_path": "assets/mesh_assets/fixture_mesh.runtime.json",
            }
        ]
        write_json(manifest_path, manifest)
        report = validate_project(self.project)
        self.assertEqual(report["scene_id"], "ray_scene_project_worker_snapshot")
        self.assertEqual(report["adapters"]["line_drawing"]["mesh_asset_ids"], ["fixture_mesh"])

    def test_declared_attachment_hash_is_checked(self) -> None:
        object_manifest_path = self.project / "object_manifest.json"
        object_manifest = read_json(object_manifest_path)
        asset_path = self.project / object_manifest["objects"][0]["runtime_asset"]
        object_manifest["objects"][0]["sha256"] = sha256(asset_path)
        object_manifest["objects"][0]["size_bytes"] = asset_path.stat().st_size
        write_json(object_manifest_path, object_manifest)
        self.assertEqual(validate_project(self.project)["status"], "ok")
        object_manifest["objects"][0]["sha256"] = "0" * 64
        write_json(object_manifest_path, object_manifest)
        with self.assertRaisesRegex(SceneProjectValidationError, "SHA-256 mismatch"):
            validate_project(self.project)

    def test_out_of_project_active_pointer_is_rejected(self) -> None:
        project_path = self.project / "scene_project.json"
        project = read_json(project_path)
        project["active"]["physics_cache"] = "../outside.json"
        write_json(project_path, project)
        with self.assertRaisesRegex(SceneProjectValidationError, "stay inside"):
            validate_project(self.project)

    def test_missing_selected_frame_is_rejected(self) -> None:
        (self.project / "assets/vf3d/active/frame_000002.vf3d").unlink()
        with self.assertRaisesRegex(SceneProjectValidationError, "file is missing"):
            validate_project(self.project)

    def test_legacy_loose_and_explicit_paths_remain_compatible(self) -> None:
        runtime_path = self.root / "legacy_scene" / "runtime.json"
        cache_path = self.root / "legacy_cache" / "cache.json"
        request_path = self.root / "legacy_ray" / "request.json"
        runtime_path.parent.mkdir(parents=True)
        cache_path.parent.mkdir(parents=True)
        request_path.parent.mkdir(parents=True)
        shutil.copy2(self.project / "scene_runtime.json", runtime_path)
        shutil.copy2(self.project / "physics_sim/active_cache_manifest.json", cache_path)
        shutil.copy2(self.project / "ray_tracing/render_request.json", request_path)
        report = validate_explicit_paths(
            runtime_path, cache_manifest=cache_path, render_request=request_path
        )
        self.assertEqual(report["mode"], "legacy_explicit_paths")
        self.assertFalse(report["portable"])
        self.assertEqual(set(report["adapters"]), {"line_drawing", "physics_sim", "ray_tracing"})

    def test_cli_writes_machine_readable_report(self) -> None:
        report_path = self.root / "reports" / "validation.json"
        result = subprocess.run(
            [
                sys.executable,
                str(TOOLS_ROOT / "validate_scene_project.py"),
                "--project-root",
                str(self.project),
                "--report",
                str(report_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(read_json(report_path)["status"], "ok")


if __name__ == "__main__":
    unittest.main()
