#!/usr/bin/env python3
"""Import and validate project-local RayTracing worker completion receipts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
from pathlib import Path
from typing import Any

from scene_project_contract import SceneProjectValidationError, validate_project


EVIDENCE_SCHEMA = "physics_trio_worker_completion_evidence_v1"
RECEIPT_SCHEMA = "ray_tracing_worker_receipt_v1"
EXPORT_MANIFEST_SCHEMA = "ray_tracing_portable_worker_export_manifest/v1"
FINALIZE_RECEIPT_SCHEMA = "codework_completed_artifact_finalize_receipt_v1"


class WorkerReceiptError(RuntimeError):
    pass


def read_object(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise WorkerReceiptError(f"failed to read JSON object {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise WorkerReceiptError(f"JSON root must be an object: {path}")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def safe_run_id(value: str) -> str:
    if not value or value in {".", ".."} or "/" in value or "\\" in value:
        raise WorkerReceiptError(f"render run id must be one contained path segment: {value}")
    if any(character not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_" for character in value):
        raise WorkerReceiptError("render run id contains unsupported characters")
    return value


def contained_run_file(run_root: Path, relpath: Any, *, field: str) -> Path:
    if not isinstance(relpath, str) or not relpath.strip():
        raise WorkerReceiptError(f"{field} must be a non-empty relative path")
    candidate = (run_root / relpath).resolve()
    try:
        candidate.relative_to(run_root.resolve())
    except ValueError as exc:
        raise WorkerReceiptError(f"{field} escapes the retained run root: {relpath}") from exc
    return candidate


def retained_run_root(project_root: Path, run_id: str) -> Path:
    candidate = (project_root / "ray_tracing" / "runs" / run_id).resolve()
    try:
        candidate.relative_to(project_root.resolve())
    except ValueError as exc:
        raise WorkerReceiptError("retained RayTracing run root escapes the project") from exc
    return candidate


def active_project_path(project_root: Path, project: dict[str, Any], key: str, fallback: str) -> Path:
    active = project.get("active")
    value = active.get(key) if isinstance(active, dict) else None
    if value is None:
        aliases = {"render_request": "active_render_request"}
        value = project.get(aliases.get(key, key))
    if not isinstance(value, str) or not value.strip():
        value = fallback
    candidate = (project_root / value).resolve()
    try:
        candidate.relative_to(project_root)
    except ValueError as exc:
        raise WorkerReceiptError(f"active {key} escapes project root: {value}") from exc
    if not candidate.is_file():
        raise WorkerReceiptError(f"active {key} is missing: {candidate}")
    return candidate


def require_text(parent: dict[str, Any], key: str, *, field: str) -> str:
    value = parent.get(key)
    if not isinstance(value, str) or not value.strip():
        raise WorkerReceiptError(f"{field}.{key} must be a non-empty string")
    return value


def require_sha256(parent: dict[str, Any], key: str, *, field: str) -> str:
    value = require_text(parent, key, field=field)
    if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise WorkerReceiptError(f"{field}.{key} must be a lowercase SHA-256 digest")
    return value


def canonical_receipt_hash(receipt: dict[str, Any]) -> str:
    value = {key: item for key, item in receipt.items() if key != "receipt_sha256"}
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def copy_exact(source: Path, destination: Path) -> None:
    if destination.exists():
        if not destination.is_file() or sha256_file(destination) != sha256_file(source):
            raise WorkerReceiptError(f"existing retained run file conflicts: {destination}")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(f".{destination.name}.tmp")
    shutil.copy2(source, temporary)
    os.replace(temporary, destination)


def import_worker_receipt(
    *,
    project_root: Path,
    export_manifest_path: Path,
    completion_evidence_path: Path,
    render_run_id: str,
    render_summary_path: Path | None = None,
) -> dict[str, Any]:
    root = project_root.resolve()
    run_id = safe_run_id(render_run_id)
    try:
        project_validation = validate_project(root)
    except SceneProjectValidationError as exc:
        raise WorkerReceiptError(str(exc)) from exc
    project = read_object(root / "scene_project.json")
    render_request_path = active_project_path(
        root, project, "render_request", "ray_tracing/render_request.json"
    )
    export_manifest_path = export_manifest_path.resolve()
    evidence_path = completion_evidence_path.resolve()
    export_manifest = read_object(export_manifest_path)
    evidence = read_object(evidence_path)
    if export_manifest.get("schema") != EXPORT_MANIFEST_SCHEMA:
        raise WorkerReceiptError("unsupported worker export manifest schema")
    if evidence.get("schema") != EVIDENCE_SCHEMA:
        raise WorkerReceiptError("unsupported worker completion evidence schema")
    if export_manifest.get("source_project_content_sha256") != project_validation["content"]["sha256"]:
        raise WorkerReceiptError("worker export source-project identity does not match current project")
    job_id = require_text(export_manifest, "job_id", field="export_manifest")
    if evidence.get("job_id") != job_id:
        raise WorkerReceiptError("completion evidence job id does not match worker export")
    terminal = evidence.get("terminal")
    worker = evidence.get("worker")
    if not isinstance(terminal, dict) or terminal.get("status") != "completed":
        raise WorkerReceiptError("completion evidence must report terminal completed status")
    if not isinstance(worker, dict):
        raise WorkerReceiptError("completion evidence worker object is required")
    worker_id = require_text(worker, "worker_id", field="worker")

    artifact_receipt = evidence.get("artifact_finalize_receipt")
    if artifact_receipt is not None:
        if not isinstance(artifact_receipt, dict) or artifact_receipt.get("schema") != FINALIZE_RECEIPT_SCHEMA:
            raise WorkerReceiptError("completion artifact finalize receipt is unsupported")
        if artifact_receipt.get("job_id") != job_id:
            raise WorkerReceiptError("artifact finalize receipt job id does not match")
        require_text(artifact_receipt, "stage", field="artifact_finalize_receipt")
        require_sha256(artifact_receipt, "finalize_token", field="artifact_finalize_receipt")
        require_sha256(artifact_receipt, "content_sha256", field="artifact_finalize_receipt")
        for key in ("file_count", "total_bytes"):
            value = artifact_receipt.get(key)
            if not isinstance(value, int) or isinstance(value, bool) or value < 0:
                raise WorkerReceiptError(
                    f"artifact_finalize_receipt.{key} must be a non-negative integer"
                )

    selected_frames = export_manifest.get("selected_frame_indices")
    if not isinstance(selected_frames, list) or not all(
        isinstance(value, int) and not isinstance(value, bool) and value >= 0
        for value in selected_frames
    ):
        raise WorkerReceiptError("worker export selected frame indices are invalid")
    physics_adapter = project_validation["adapters"]["physics_sim"]
    capabilities = worker.get("capabilities", [])
    if not isinstance(capabilities, list) or not all(
        isinstance(value, str) and value.strip() for value in capabilities
    ):
        raise WorkerReceiptError("worker.capabilities must be an array of non-empty strings")
    package_versions = evidence.get("package_versions", {})
    if not isinstance(package_versions, dict):
        raise WorkerReceiptError("completion evidence package_versions must be an object")
    retry_lineage = evidence.get("retry_lineage", [])
    if not isinstance(retry_lineage, list):
        raise WorkerReceiptError("completion evidence retry_lineage must be an array")
    summary_source: Path | None = None
    if render_summary_path is not None:
        summary_source = render_summary_path.resolve()
        if not summary_source.is_file():
            raise WorkerReceiptError(f"render summary is missing: {summary_source}")
    run_root = retained_run_root(root, run_id)
    retained_request = run_root / "render_request.json"
    retained_summary = run_root / "render_summary.json"
    copy_exact(render_request_path, retained_request)
    if summary_source is not None:
        copy_exact(summary_source, retained_summary)

    receipt: dict[str, Any] = {
        "schema": RECEIPT_SCHEMA,
        "project": {
            "scene_id": project_validation["scene_id"],
            "source_content_sha256": project_validation["content"]["sha256"],
            "source_content_file_count": project_validation["content"]["file_count"],
        },
        "lineage": {
            "physics_run_id": physics_adapter.get("active_run_id"),
            "render_run_id": run_id,
            "export_item_name": export_manifest.get("item_name"),
            "worker_job_id": job_id,
        },
        "selection": {
            "frame_indices": selected_frames,
            "render_request_sha256": sha256_file(retained_request),
        },
        "package": {
            "export_manifest_sha256": sha256_file(export_manifest_path),
            "versions": package_versions,
        },
        "worker": {
            "worker_id": worker_id,
            "platform": worker.get("platform"),
            "arch": worker.get("arch"),
            "capabilities": capabilities,
        },
        "terminal": {
            "status": terminal["status"],
            "publication_state": terminal.get("publication_state"),
            "completed_at": terminal.get("completed_at"),
            "preview_url": terminal.get("preview_url"),
            "catalog_url": terminal.get("catalog_url"),
        },
        "artifact_transport": artifact_receipt,
        "retry_lineage": retry_lineage,
        "retained_files": {
            "render_request": "render_request.json",
            "render_summary": "render_summary.json" if render_summary_path is not None else None,
        },
    }
    if render_summary_path is not None:
        receipt["selection"]["render_summary_sha256"] = sha256_file(retained_summary)
    receipt["receipt_sha256"] = canonical_receipt_hash(receipt)
    receipt_path = run_root / "worker_receipt.json"
    if receipt_path.exists():
        existing = read_object(receipt_path)
        if existing != receipt:
            raise WorkerReceiptError("existing worker receipt conflicts with completion evidence")
        return {"status": "already_imported", "receipt_path": str(receipt_path), "receipt": receipt}
    write_json_atomic(receipt_path, receipt)
    return {"status": "imported", "receipt_path": str(receipt_path), "receipt": receipt}


def validate_worker_receipt(project_root: Path, render_run_id: str) -> dict[str, Any]:
    root = project_root.resolve()
    run_id = safe_run_id(render_run_id)
    run_root = retained_run_root(root, run_id)
    receipt_path = run_root / "worker_receipt.json"
    receipt = read_object(receipt_path)
    if receipt.get("schema") != RECEIPT_SCHEMA:
        raise WorkerReceiptError("unsupported worker receipt schema")
    if receipt.get("receipt_sha256") != canonical_receipt_hash(receipt):
        raise WorkerReceiptError("worker receipt SHA-256 mismatch")
    lineage = receipt.get("lineage")
    retained = receipt.get("retained_files")
    selection = receipt.get("selection")
    if not isinstance(lineage, dict) or lineage.get("render_run_id") != run_id:
        raise WorkerReceiptError("worker receipt render-run identity mismatch")
    if not isinstance(retained, dict) or not isinstance(selection, dict):
        raise WorkerReceiptError("worker receipt retained-file contract is missing")
    request_path = contained_run_file(
        run_root, retained.get("render_request"), field="retained_files.render_request"
    )
    if not request_path.is_file() or sha256_file(request_path) != selection.get("render_request_sha256"):
        raise WorkerReceiptError("retained render request SHA-256 mismatch")
    summary_relpath = retained.get("render_summary")
    if summary_relpath:
        summary_path = contained_run_file(
            run_root, summary_relpath, field="retained_files.render_summary"
        )
        if not summary_path.is_file() or sha256_file(summary_path) != selection.get("render_summary_sha256"):
            raise WorkerReceiptError("retained render summary SHA-256 mismatch")
    return {"status": "ok", "receipt_path": str(receipt_path), "receipt": receipt}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    import_parser = sub.add_parser("import")
    import_parser.add_argument("--project-root", type=Path, required=True)
    import_parser.add_argument("--export-manifest", type=Path, required=True)
    import_parser.add_argument("--completion-evidence", type=Path, required=True)
    import_parser.add_argument("--render-run-id", required=True)
    import_parser.add_argument("--render-summary", type=Path)
    validate_parser = sub.add_parser("validate")
    validate_parser.add_argument("--project-root", type=Path, required=True)
    validate_parser.add_argument("--render-run-id", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "import":
            result = import_worker_receipt(
                project_root=args.project_root,
                export_manifest_path=args.export_manifest,
                completion_evidence_path=args.completion_evidence,
                render_run_id=args.render_run_id,
                render_summary_path=args.render_summary,
            )
        else:
            result = validate_worker_receipt(args.project_root, args.render_run_id)
    except WorkerReceiptError as exc:
        print(json.dumps({"status": "error", "error": str(exc)}, indent=2), file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
