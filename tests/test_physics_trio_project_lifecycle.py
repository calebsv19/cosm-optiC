from __future__ import annotations

import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "bin/physics_trio_project_lifecycle.py"
SPEC = importlib.util.spec_from_file_location("physics_trio_project_lifecycle", MODULE_PATH)
assert SPEC and SPEC.loader
lifecycle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lifecycle)
FIXTURE = Path(__file__).resolve().parent / "fixtures/scene_project_worker_snapshot"


class PhysicsTrioLifecycleTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="physics_trio_lifecycle_")
        self.root = Path(self.temp.name)
        self.project = self.root / "project"
        shutil.copytree(FIXTURE, self.project)
        self.recipe_path = self.root / "recipe.json"
        self.recipe = {
            "schema": lifecycle.RECIPE_SCHEMA,
            "project_root": "project",
            "run_root": "run",
            "physics": {"action": "existing"},
            "ray_tracing": {"action": "existing", "start": 0, "count": 2, "stride": 2},
            "worker_export": {
                "queue_root": "queue",
                "item_name": "lifecycle-fixture",
                "job_id": "ray-tracing--lifecycle-fixture--20260713T000000Z--test0001",
                "ray_version": "0.6.2",
                "physics_version": "0.3.1",
                "execution_profile": "trio-headless-v1-mesh-sidecar",
            },
            "queue": {"staging_root": "staging"},
        }
        self.write_recipe()

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write_recipe(self) -> None:
        self.recipe_path.write_text(json.dumps(self.recipe), encoding="utf-8")

    def test_plan_orders_owner_phases_and_stops_at_dry_run(self) -> None:
        plan = lifecycle.execute(self.recipe_path, resume=False, plan_only=True)
        self.assertEqual(
            [phase["name"] for phase in plan["phases"]],
            [
                "line_drawing_contract",
                "physics_sim_cache",
                "ray_tracing_intent",
                "worker_snapshot",
                "queue_validate",
                "queue_prepare",
                "planner_dry_run",
            ],
        )
        self.assertFalse(plan["live_submit_supported"])
        planner = plan["phases"][-1]["commands"][0]["argv"]
        self.assertIn("--dry-run", planner)
        self.assertNotIn("submit", planner)
        export = plan["phases"][3]["commands"][0]["argv"]
        profile_index = export.index("--execution-profile")
        self.assertEqual(export[profile_index + 1], "trio-headless-v1-mesh-sidecar")
        queue_validate = plan["phases"][4]["commands"][0]["argv"]
        remote_root_index = queue_validate.index("--remote-root")
        self.assertEqual(
            queue_validate[remote_root_index + 1],
            ".codework-worker/export-dropbox",
        )

    def test_changed_recipe_cannot_resume_existing_status(self) -> None:
        run_root = self.root / "run"
        lifecycle.write_json_atomic(
            run_root / "status.json",
            {"schema": lifecycle.STATUS_SCHEMA, "recipe_sha256": "different", "phases": {}},
        )
        with self.assertRaisesRegex(lifecycle.LifecycleError, "recipe content changed"):
            lifecycle.execute(self.recipe_path, resume=True, plan_only=False)

    def test_completed_status_resumes_without_rewriting_evidence(self) -> None:
        plan = lifecycle.execute(self.recipe_path, resume=False, plan_only=True)
        status = {
            "schema": lifecycle.STATUS_SCHEMA,
            "status": "completed",
            "recipe_sha256": plan["recipe_sha256"],
            "finished_at": "stable-finish",
            "phases": {
                phase["name"]: {"status": "completed", "finished_at": "stable-phase"}
                for phase in plan["phases"]
            },
        }
        lifecycle.write_json_atomic(self.root / "run/status.json", status)
        resumed = lifecycle.execute(self.recipe_path, resume=True, plan_only=False)
        self.assertEqual(resumed, status)
        self.assertEqual(
            json.loads((self.root / "run/status.json").read_text(encoding="utf-8")), status
        )

    def test_out_of_project_runtime_is_rejected(self) -> None:
        manifest_path = self.project / "scene_project.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["active"]["scene_runtime"] = "../outside.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(lifecycle.LifecycleError, "escapes project root"):
            lifecycle.execute(self.recipe_path, resume=False, plan_only=True)


if __name__ == "__main__":
    unittest.main()
