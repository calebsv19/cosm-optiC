#!/usr/bin/env python3

from __future__ import annotations

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


RAY_ROOT = Path(__file__).resolve().parents[1]
TOOLS_ROOT = RAY_ROOT / "tools"
FIXTURE_ROOT = RAY_ROOT / "tests" / "fixtures" / "scene_project_worker_snapshot"
sys.path.insert(0, str(TOOLS_ROOT))

from scene_project_worker_export import (  # noqa: E402
    SceneProjectExportError,
    export_scene_plus_physics_cache,
)


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SceneProjectWorkerExportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory(prefix="ray_scene_project_worker_export_")
        self.root = Path(self.tempdir.name)
        self.project = self.root / "project"
        shutil.copytree(FIXTURE_ROOT, self.project)
        self.output = self.root / "queue"

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def export(self, *, force_worker_id: str | None = None) -> dict:
        return export_scene_plus_physics_cache(
            project_root=self.project,
            output_root=self.output,
            item_name="r2b-fixture",
            job_id="ray-tracing--r2b-fixture--20260712T000000Z--r2bfixture01",
            ray_version="0.6.2",
            physics_version="0.1.0",
            force_worker_id=force_worker_id,
        )

    def test_selected_scene_and_physics_snapshot_is_portable(self) -> None:
        result = self.export()
        bundle = Path(result["bundle_root"])

        self.assertEqual(result["selected_frame_indices"], [0, 2])
        self.assertTrue((bundle / "scene_project.json").is_file())
        self.assertTrue((bundle / "scene_authoring.json").is_file())
        self.assertTrue((bundle / "scene_runtime.json").is_file())
        self.assertTrue((bundle / "object_manifest.json").is_file())
        self.assertTrue((bundle / "assets/mesh_assets/fixture_mesh.runtime.json").is_file())
        self.assertTrue((bundle / "assets/vf3d/active/frame_000000.vf3d").is_file())
        self.assertTrue((bundle / "assets/vf3d/active/frame_000002.vf3d").is_file())
        self.assertFalse((bundle / "assets/vf3d/active/frame_000004.vf3d").exists())
        self.assertTrue((bundle / "assets/vf3d/active/frame_000000.pack").is_file())
        self.assertTrue((bundle / "assets/physics/active/water_surface_000002.json").is_file())
        self.assertFalse((bundle / "assets/physics/active/water_surface_000004.json").exists())

        scene_runtime = read_json(bundle / "scene_runtime.json")
        mesh_object = next(
            obj for obj in scene_runtime["objects"] if obj.get("object_type") == "mesh_asset_instance"
        )
        self.assertEqual(
            mesh_object["extensions"]["line_drawing"]["runtime_mesh_path"],
            "assets/mesh_assets/fixture_mesh.runtime.json",
        )

        vf3d_manifest = read_json(bundle / "assets/vf3d/active/manifest.json")
        self.assertEqual(vf3d_manifest["run_name"], ".")
        self.assertEqual([frame["frame_index"] for frame in vf3d_manifest["frames"]], [0, 2])
        water_manifest = read_json(bundle / "assets/physics/active/water_manifest_v1.json")
        self.assertEqual([frame["frame_index"] for frame in water_manifest["frames"]], [0, 2])
        scene_bundle = read_json(bundle / "assets/physics/active/scene_bundle.json")
        self.assertEqual(scene_bundle["fluid_source"]["path"], "../../vf3d/active/manifest.json")

        cache = read_json(bundle / "physics_sim/active_cache_manifest.json")
        self.assertEqual(cache["retained_frame_indices"], [0, 2])
        self.assertEqual(cache["frame_count"], 2)
        self.assertEqual(cache["scene_bundle"], "assets/physics/active/scene_bundle.json")
        project_request = read_json(bundle / "ray_tracing/render_request.json")
        self.assertTrue(project_request["custom_fixture_field"]["preserve"])
        self.assertEqual(project_request["volume"]["source_path"], "../assets/physics/active/scene_bundle.json")

        job = read_json(bundle / "request/job.json")
        self.assertEqual(job["start_stage"], "ray_tracing")
        support = job["input_contract"]["support_files_relpaths"]
        self.assertIn("line_drawing/assets/mesh_assets/fixture_mesh.runtime.json", support)
        self.assertIn("assets/vf3d/active/frame_000002.vf3d", support)
        self.assertIn("assets/physics/active/scene_bundle.json", support)
        self.assertFalse(any(relpath.startswith("support/") for relpath in support))
        self.assertNotIn("line_drawing/scene_project.json", support)
        self.assertNotIn("line_drawing/scene_authoring.json", support)
        self.assertNotIn("line_drawing/object_manifest.json", support)
        self.assertNotIn("physics_sim/active_cache_manifest.json", support)
        self.assertNotIn("ray_tracing/render_request.json", support)
        # The live preview profile currently accepts at most 16 inline files.
        # Runtime scene and the generated worker request add two entries.
        self.assertLessEqual(len(support) + 2, 16)
        worker_scene_bundle = read_json(
            bundle / "request/payload/assets/physics/active/scene_bundle.json"
        )
        self.assertEqual(worker_scene_bundle["fluid_source"]["path"], "../../vf3d/active/manifest.json")
        worker_cache = read_json(
            bundle / "request/payload/physics_sim/active_cache_manifest.json"
        )
        self.assertEqual(worker_cache["scene_bundle"], "assets/physics/active/scene_bundle.json")
        worker_scene = read_json(bundle / "request/payload/line_drawing/scene_runtime.json")
        worker_mesh_object = next(
            obj for obj in worker_scene["objects"] if obj.get("object_type") == "mesh_asset_instance"
        )
        self.assertEqual(
            worker_mesh_object["extensions"]["line_drawing"]["runtime_mesh_path"],
            "assets/mesh_assets/fixture_mesh.runtime.json",
        )
        queue = read_json(Path(result["worker_job_queue_path"]))
        profile_index = queue["submit_args"].index("--execution-profile")
        self.assertEqual(
            queue["submit_args"][profile_index + 1],
            "trio-headless-v1-mesh-sidecar",
        )
        self.assertNotIn("--ray-disable-volume", queue["submit_args"])
        volume_index = queue["submit_args"].index("--ray-volume-source-path")
        self.assertEqual(
            queue["submit_args"][volume_index + 1],
            "assets/physics/active/scene_bundle.json",
        )
        lineage = read_json(bundle / "lineage/source_lineage.json")
        self.assertEqual(lineage["source_active_run_id"], "physics-fixture-run")
        self.assertEqual(lineage["selected_frame_indices"], [0, 2])
        package_manifest = read_json(bundle / "manifest.json")
        manifest_relpaths = {entry["relpath"] for entry in package_manifest["files"]}
        self.assertIn("bundle/assets/vf3d/active/manifest.json", manifest_relpaths)
        self.assertIn("bundle/assets/physics/active/scene_bundle.json", manifest_relpaths)

    def test_optional_worker_target_is_job_scoped(self) -> None:
        result = self.export(force_worker_id="linuxpc")
        queue = read_json(Path(result["worker_job_queue_path"]))
        target_index = queue["submit_args"].index("--force-worker-id")
        self.assertEqual(queue["submit_args"][target_index + 1], "linuxpc")

    def test_out_of_project_active_cache_is_rejected(self) -> None:
        manifest_path = self.project / "scene_project.json"
        manifest = read_json(manifest_path)
        manifest["active"]["physics_cache"] = "../outside.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(SceneProjectExportError, "stay inside"):
            self.export()

    def test_missing_requested_frame_is_rejected_without_partial_item(self) -> None:
        (self.project / "assets/vf3d/active/frame_000002.vf3d").unlink()
        with self.assertRaisesRegex(SceneProjectExportError, "file is missing"):
            self.export()
        self.assertFalse((self.output / "inbox/r2b-fixture").exists())

    def test_metadata_only_mesh_asset_is_rejected_without_partial_item(self) -> None:
        mesh_path = self.project / "assets/mesh_assets/fixture_mesh.runtime.json"
        mesh_path.write_text(
            json.dumps(
                {
                    "schema_variant": "mesh_asset_runtime_v1",
                    "asset_id": "fixture_mesh",
                    "vertex_count": 8,
                    "triangle_count": 12,
                }
            ),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(SceneProjectExportError, "schema_family"):
            self.export()
        self.assertFalse((self.output / "inbox/r2b-fixture").exists())


if __name__ == "__main__":
    unittest.main()
