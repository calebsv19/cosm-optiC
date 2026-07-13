from __future__ import annotations

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_ROOT))

from scene_project_worker_export import export_scene_plus_physics_cache
from scene_project_worker_receipt import (
    WorkerReceiptError,
    import_worker_receipt,
    validate_worker_receipt,
)


ROOT = Path(__file__).resolve().parent
FIXTURE = ROOT / "fixtures/scene_project_worker_snapshot"


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SceneProjectWorkerReceiptTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="scene_project_receipt_")
        self.root = Path(self.temp.name)
        self.project = self.root / "project"
        shutil.copytree(FIXTURE, self.project)
        self.queue = self.root / "queue"
        self.job_id = "ray-tracing--receipt-test--20260713T000000Z--receipt01"
        result = export_scene_plus_physics_cache(
            project_root=self.project,
            output_root=self.queue,
            item_name="receipt-test",
            job_id=self.job_id,
            ray_version="0.6.2",
            physics_version="0.3.1",
        )
        self.export_manifest = Path(result["manifest_path"])
        self.evidence = self.root / "completion_evidence.json"
        self.summary = self.root / "render_summary.json"
        self.summary.write_text('{"status":"completed"}\n', encoding="utf-8")
        self.evidence.write_text(
            json.dumps(
                {
                    "schema": "physics_trio_worker_completion_evidence_v1",
                    "job_id": self.job_id,
                    "worker": {
                        "worker_id": "linuxpc",
                        "platform": "linux",
                        "arch": "x86_64",
                        "capabilities": ["trio-headless-v1", "completed-artifact-bulk-v1"],
                    },
                    "package_versions": {"ray_tracing": "0.6.2", "physics_sim": "0.3.1"},
                    "terminal": {
                        "status": "completed",
                        "publication_state": "batch_ready",
                        "completed_at": "2026-07-13T00:00:00Z",
                        "preview_url": "https://visualizer.example/run",
                    },
                    "artifact_finalize_receipt": {
                        "schema": "codework_completed_artifact_finalize_receipt_v1",
                        "job_id": self.job_id,
                        "stage": "ray_tracing",
                        "finalize_token": "a" * 64,
                        "content_sha256": "b" * 64,
                        "file_count": 2,
                        "total_bytes": 10,
                    },
                    "retry_lineage": [{"attempt": 1, "status": "finalized"}],
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_import_is_idempotent_and_validates_offline(self) -> None:
        authoring_before = (self.project / "scene_authoring.json").read_bytes()
        cache_before = (self.project / "physics_sim/active_cache_manifest.json").read_bytes()
        first = import_worker_receipt(
            project_root=self.project,
            export_manifest_path=self.export_manifest,
            completion_evidence_path=self.evidence,
            render_run_id="render-receipt-01",
            render_summary_path=self.summary,
        )
        self.assertEqual(first["status"], "imported")
        second = import_worker_receipt(
            project_root=self.project,
            export_manifest_path=self.export_manifest,
            completion_evidence_path=self.evidence,
            render_run_id="render-receipt-01",
            render_summary_path=self.summary,
        )
        self.assertEqual(second["status"], "already_imported")
        validated = validate_worker_receipt(self.project, "render-receipt-01")
        self.assertEqual(validated["status"], "ok")
        self.assertEqual(validated["receipt"]["lineage"]["physics_run_id"], "physics-fixture-run")
        self.assertEqual(validated["receipt"]["selection"]["frame_indices"], [0, 2])
        self.assertEqual((self.project / "scene_authoring.json").read_bytes(), authoring_before)
        self.assertEqual((self.project / "physics_sim/active_cache_manifest.json").read_bytes(), cache_before)

    def test_job_mismatch_and_nonterminal_evidence_are_rejected(self) -> None:
        evidence = read_json(self.evidence)
        evidence["job_id"] = "different"
        self.evidence.write_text(json.dumps(evidence), encoding="utf-8")
        with self.assertRaisesRegex(WorkerReceiptError, "job id"):
            import_worker_receipt(
                project_root=self.project,
                export_manifest_path=self.export_manifest,
                completion_evidence_path=self.evidence,
                render_run_id="render-receipt-02",
            )
        evidence["job_id"] = self.job_id
        evidence["terminal"]["status"] = "running"
        self.evidence.write_text(json.dumps(evidence), encoding="utf-8")
        with self.assertRaisesRegex(WorkerReceiptError, "terminal completed"):
            import_worker_receipt(
                project_root=self.project,
                export_manifest_path=self.export_manifest,
                completion_evidence_path=self.evidence,
                render_run_id="render-receipt-02",
            )

    def test_tampered_retained_summary_and_unsafe_run_id_are_rejected(self) -> None:
        import_worker_receipt(
            project_root=self.project,
            export_manifest_path=self.export_manifest,
            completion_evidence_path=self.evidence,
            render_run_id="render-receipt-03",
            render_summary_path=self.summary,
        )
        retained = self.project / "ray_tracing/runs/render-receipt-03/render_summary.json"
        retained.write_text("tampered\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkerReceiptError, "render summary SHA-256"):
            validate_worker_receipt(self.project, "render-receipt-03")
        with self.assertRaisesRegex(WorkerReceiptError, "contained path segment"):
            validate_worker_receipt(self.project, "../escape")

    def test_tampered_retained_path_is_contained_even_with_valid_receipt_hash(self) -> None:
        imported = import_worker_receipt(
            project_root=self.project,
            export_manifest_path=self.export_manifest,
            completion_evidence_path=self.evidence,
            render_run_id="render-receipt-04",
        )
        receipt_path = Path(imported["receipt_path"])
        receipt = read_json(receipt_path)
        receipt["retained_files"]["render_request"] = "../../scene_authoring.json"
        receipt.pop("receipt_sha256")
        from scene_project_worker_receipt import canonical_receipt_hash

        receipt["receipt_sha256"] = canonical_receipt_hash(receipt)
        receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
        with self.assertRaisesRegex(WorkerReceiptError, "escapes the retained run root"):
            validate_worker_receipt(self.project, "render-receipt-04")

    def test_existing_run_symlink_cannot_escape_project(self) -> None:
        runs_root = self.project / "ray_tracing/runs"
        runs_root.mkdir(parents=True)
        (runs_root / "render-receipt-05").symlink_to(self.root / "outside", target_is_directory=True)
        with self.assertRaisesRegex(WorkerReceiptError, "run root escapes"):
            import_worker_receipt(
                project_root=self.project,
                export_manifest_path=self.export_manifest,
                completion_evidence_path=self.evidence,
                render_run_id="render-receipt-05",
            )

    def test_invalid_evidence_does_not_create_partial_run(self) -> None:
        evidence = read_json(self.evidence)
        evidence["worker"].pop("worker_id")
        self.evidence.write_text(json.dumps(evidence), encoding="utf-8")
        with self.assertRaisesRegex(WorkerReceiptError, "worker.worker_id"):
            import_worker_receipt(
                project_root=self.project,
                export_manifest_path=self.export_manifest,
                completion_evidence_path=self.evidence,
                render_run_id="render-receipt-06",
            )
        self.assertFalse((self.project / "ray_tracing/runs/render-receipt-06").exists())


if __name__ == "__main__":
    unittest.main()
