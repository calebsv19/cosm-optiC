#!/usr/bin/env python3
"""Private STL intake auditor for RayTracing high-triangle stress assets.

This tool does not download or publish assets. It records a normalized intake
checkpoint for an existing STL source or curated-library asset before it is
converted into a runtime sidecar or used in high-triangle stress proofs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import time
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LIBRARY_ROOT = Path("/Users/calebsv/Desktop/stls/Curated_STL_Test_Library")
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "_private_workspace_artifacts/high_triangle_stress/intake"


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def stable_id(raw: str) -> str:
    lowered = raw.strip().lower()
    lowered = re.sub(r"[^a-z0-9]+", "_", lowered)
    lowered = re.sub(r"_+", "_", lowered).strip("_")
    return lowered or "stl_candidate"


def stl_scan(path: Path) -> dict[str, Any]:
    size = path.stat().st_size
    with path.open("rb") as handle:
        prefix = handle.read(512)

    binary_count = None
    binary_size_matches = False
    if size >= 84:
        with path.open("rb") as handle:
            handle.seek(80)
            raw_count = handle.read(4)
        if len(raw_count) == 4:
            binary_count = struct.unpack("<I", raw_count)[0]
            binary_size_matches = 84 + (binary_count * 50) == size

    looks_ascii = prefix.lstrip().lower().startswith(b"solid")
    if binary_size_matches:
        fmt = "stl_binary"
        triangle_count = binary_count
        confidence = "size_exact"
    elif looks_ascii:
        text = path.read_text(encoding="utf-8", errors="ignore")
        triangle_count = len(re.findall(r"(?im)^\s*facet\s+normal\b", text))
        fmt = "stl_ascii"
        confidence = "facet_count"
    else:
        fmt = "stl_unknown"
        triangle_count = binary_count if binary_count is not None else 0
        confidence = "weak_binary_header"

    return {
        "path": str(path),
        "file_size_bytes": size,
        "sha256": sha256_file(path),
        "source_format_detected": fmt,
        "triangle_count_detected": int(triangle_count or 0),
        "triangle_count_confidence": confidence,
        "binary_header_triangle_count": binary_count,
        "binary_size_matches": binary_size_matches,
    }


def manifest_assets(library_root: Path) -> list[dict[str, Any]]:
    manifest = library_root / "manifest.json"
    if not manifest.exists():
        return []
    data = load_json(manifest)
    assets = data.get("assets", [])
    return assets if isinstance(assets, list) else []


def find_asset(asset_id: str, assets: list[dict[str, Any]]) -> dict[str, Any] | None:
    for asset in assets:
        if isinstance(asset, dict) and asset.get("id") == asset_id:
            return asset
    return None


def resolve_library_path(library_root: Path, raw: str | None) -> Path | None:
    if not raw:
        return None
    path = Path(raw).expanduser()
    if path.is_absolute():
        return path
    return library_root / path


def runtime_asset_from_sidecar(sidecar_root: Path, runtime_asset_id: str) -> Path | None:
    candidates = []
    if runtime_asset_id:
        candidates.append(sidecar_root / "assets" / "mesh_assets" / f"{runtime_asset_id}.runtime.json")
    candidates.extend(sorted((sidecar_root / "assets" / "mesh_assets").glob("*.runtime.json")))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def sidecar_state(library_root: Path, asset_id: str, runtime_asset_id: str) -> dict[str, Any]:
    sidecar_root = library_root / "runtime_mesh_sidecars" / asset_id
    scene_path = sidecar_root / "scene_runtime.json"
    summary_path = sidecar_root / "import_summary.json"
    runtime_path = runtime_asset_from_sidecar(sidecar_root, runtime_asset_id)
    state: dict[str, Any] = {
        "root": str(sidecar_root),
        "exists": sidecar_root.exists(),
        "scene_runtime": str(scene_path),
        "scene_runtime_exists": scene_path.exists(),
        "import_summary": str(summary_path),
        "import_summary_exists": summary_path.exists(),
        "runtime_asset": str(runtime_path) if runtime_path else "",
        "runtime_asset_exists": bool(runtime_path and runtime_path.exists()),
    }
    if summary_path.exists():
        summary = load_json(summary_path)
        state["summary_vertices"] = summary.get("vertices", 0)
        state["summary_triangles"] = summary.get("triangles", 0)
        state["summary_surface_groups"] = summary.get("surface_groups", 0)
        state["summary_source_stl"] = summary.get("source_stl", "")
    if runtime_path and runtime_path.exists():
        runtime = load_json(runtime_path)
        mesh = runtime.get("mesh", {})
        state["runtime_asset_id"] = runtime.get("asset_id", "")
        state["runtime_vertex_count"] = mesh.get("vertex_count", len(mesh.get("vertices", [])))
        state["runtime_triangle_count"] = mesh.get("triangle_count", len(mesh.get("triangles", [])))
        state["runtime_bounds"] = runtime.get("local_bounds", {})
        groups = runtime.get("surface_groups", [])
        state["runtime_surface_group_count"] = len(groups) if isinstance(groups, list) else 0
    return state


def asset_metadata(asset: dict[str, Any], source_scan: dict[str, Any]) -> dict[str, Any]:
    manifest_sha = str(asset.get("sha256", ""))
    manifest_triangles = int(asset.get("triangle_count", 0) or 0)
    detected_triangles = int(source_scan.get("triangle_count_detected", 0) or 0)
    license_status = str(asset.get("license_status", ""))
    return {
        "id": asset.get("id", ""),
        "display_name": asset.get("display_name", ""),
        "status": asset.get("status", ""),
        "source_url": asset.get("source_url", ""),
        "source_path": asset.get("source_path", ""),
        "raw_source_path": asset.get("raw_source_path", ""),
        "source_format": asset.get("source_format", ""),
        "license_status": license_status,
        "stress_tags": asset.get("stress_tags", []),
        "recommended_first_material": asset.get("recommended_first_material", ""),
        "runtime_asset_id": asset.get("runtime_asset_id", ""),
        "manifest_file_size_bytes": asset.get("file_size_bytes", 0),
        "manifest_triangle_count": manifest_triangles,
        "manifest_sha256": manifest_sha,
        "sha256_matches_manifest": bool(manifest_sha and manifest_sha == source_scan.get("sha256")),
        "triangle_count_matches_manifest": bool(
            manifest_triangles and detected_triangles and manifest_triangles == detected_triangles
        ),
        "license_needs_publication_review": any(
            token in license_status.lower()
            for token in ("needed", "unknown", "personal test", "local_existing")
        ),
    }


def command_plan(asset_id: str,
                 source_path: Path,
                 library_root: Path,
                 runtime_asset_id: str) -> dict[str, Any]:
    sidecar_root = library_root / "runtime_mesh_sidecars" / asset_id
    scene_id = f"scene_{asset_id}_runtime_mesh"
    object_id = f"obj_{asset_id}"
    harness = "line_drawing/build/toolchains/clang/bin/imported_mesh_harness"
    return {
        "build_import_harness": "make -C line_drawing imported_mesh_harness",
        "convert_to_runtime_sidecar": [
            harness,
            "--stl",
            str(source_path),
            "--out",
            str(sidecar_root),
            "--asset-id",
            runtime_asset_id,
            "--scene-id",
            scene_id,
            "--object-id",
            object_id,
        ],
        "stress_proof_direct_light": [
            "python3",
            "ray_tracing/tools/run_private_high_triangle_stress.py",
            asset_id,
            "--run-id",
            f"hplus_{asset_id}_{time.strftime('%Y-%m-%d')}",
        ],
    }


def markdown_report(report: dict[str, Any]) -> str:
    source = report["source"]
    meta = report["metadata"]
    sidecar = report["sidecar"]
    gates = report["gates"]
    lines = [
        f"# STL Intake Report: {report['asset_id']}",
        "",
        f"- status: `{report['status']}`",
        f"- source: `{source['path']}`",
        f"- format: `{source['source_format_detected']}`",
        f"- size bytes: `{source['file_size_bytes']}`",
        f"- sha256: `{source['sha256']}`",
        f"- detected triangles: `{source['triangle_count_detected']}`",
        f"- manifest triangles: `{meta.get('manifest_triangle_count', 0)}`",
        f"- license status: `{meta.get('license_status', '')}`",
        f"- sidecar exists: `{sidecar['exists']}`",
        f"- runtime asset exists: `{sidecar['runtime_asset_exists']}`",
        f"- ready for stress proof: `{gates['ready_for_stress_proof']}`",
        "",
        "## Gate Notes",
        "",
    ]
    for note in gates["notes"]:
        lines.append(f"- {note}")
    lines.extend(["", "## Commands", ""])
    commands = report["commands"]
    lines.append(f"- build harness: `{commands['build_import_harness']}`")
    lines.append("- convert:")
    lines.append("  ```bash")
    lines.append("  " + " ".join(commands["convert_to_runtime_sidecar"]))
    lines.append("  ```")
    lines.append("- stress:")
    lines.append("  ```bash")
    lines.append("  " + " ".join(commands["stress_proof_direct_light"]))
    lines.append("  ```")
    lines.append("")
    return "\n".join(lines)


def build_report(args: argparse.Namespace) -> dict[str, Any]:
    library_root = args.library_root.expanduser().resolve()
    assets = manifest_assets(library_root)
    source_path: Path | None = None
    asset: dict[str, Any] = {}
    asset_id = stable_id(args.asset_id or args.source_stl.stem if args.source_stl else args.asset)

    if args.asset:
        asset = find_asset(args.asset, assets) or {}
        if not asset:
            raise FileNotFoundError(f"asset id not found in manifest: {args.asset}")
        asset_id = str(asset["id"])
        source_path = resolve_library_path(library_root, asset.get("source_path"))
    elif args.source_stl:
        source_path = args.source_stl.expanduser().resolve()
        runtime_asset_id = f"asset_stl_{asset_id}"
        asset = {
            "id": asset_id,
            "display_name": args.display_name or asset_id,
            "status": "intake_candidate_source_only",
            "source_path": str(source_path),
            "source_format": "",
            "license_status": args.license_status or "unknown; intake review required before publication",
            "stress_tags": args.stress_tag or [],
            "recommended_first_material": args.recommended_material or "matte_gray",
            "runtime_asset_id": runtime_asset_id,
        }
    else:
        raise ValueError("provide --asset or --source-stl")

    if not source_path or not source_path.exists():
        raise FileNotFoundError(f"source STL not found: {source_path}")

    source_scan = stl_scan(source_path)
    metadata = asset_metadata(asset, source_scan)
    runtime_asset_id = str(asset.get("runtime_asset_id") or f"asset_stl_{asset_id}")
    sidecar = sidecar_state(library_root, asset_id, runtime_asset_id)

    notes: list[str] = []
    if not metadata["sha256_matches_manifest"] and metadata.get("manifest_sha256"):
        notes.append("source sha256 does not match manifest")
    if not metadata["triangle_count_matches_manifest"] and metadata.get("manifest_triangle_count"):
        notes.append("detected STL triangle count differs from manifest")
    if metadata["license_needs_publication_review"]:
        notes.append("license/source status requires review before public proof or publication")
    if not sidecar["runtime_asset_exists"]:
        notes.append("runtime sidecar is missing; run conversion before stress proof")
    else:
        expected = int(sidecar.get("summary_triangles", 0) or sidecar.get("runtime_triangle_count", 0) or 0)
        detected = int(source_scan.get("triangle_count_detected", 0) or 0)
        if expected and detected and abs(expected - detected) > max(2, detected // 100):
            notes.append("runtime sidecar triangle count differs materially from source scan")
    if not notes:
        notes.append("intake gates passed for private stress use")

    ready_for_sidecar = bool(source_scan["sha256"] and source_scan["triangle_count_detected"] > 0)
    ready_for_stress = bool(
        ready_for_sidecar
        and sidecar["runtime_asset_exists"]
        and sidecar["scene_runtime_exists"]
        and int(sidecar.get("runtime_triangle_count", 0) or sidecar.get("summary_triangles", 0) or 0) > 0
    )

    report = {
        "schema_version": "ray_tracing_private_stl_intake_report_v1",
        "asset_id": asset_id,
        "generated_at_local": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "status": "ready_for_stress" if ready_for_stress else "ready_for_sidecar" if ready_for_sidecar else "blocked",
        "library_root": str(library_root),
        "source": source_scan,
        "metadata": metadata,
        "sidecar": sidecar,
        "gates": {
            "ready_for_sidecar": ready_for_sidecar,
            "ready_for_stress_proof": ready_for_stress,
            "notes": notes,
        },
        "commands": command_plan(asset_id, source_path, library_root, runtime_asset_id),
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--asset", help="asset id from the curated STL manifest")
    group.add_argument("--source-stl", type=Path, help="explicit STL source path for an intake candidate")
    parser.add_argument("--asset-id", help="candidate id to use with --source-stl")
    parser.add_argument("--display-name", default="")
    parser.add_argument("--license-status", default="")
    parser.add_argument("--stress-tag", action="append", default=[])
    parser.add_argument("--recommended-material", default="")
    parser.add_argument("--library-root", type=Path, default=DEFAULT_LIBRARY_ROOT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--run-id", default="")
    args = parser.parse_args()

    report = build_report(args)
    run_id = args.run_id or f"{report['asset_id']}_{time.strftime('%Y-%m-%d')}"
    run_root = (args.output_root / run_id).resolve()
    report_path = run_root / "intake_report.json"
    markdown_path = run_root / "intake_report.md"
    write_json(report_path, report)
    write_text(markdown_path, markdown_report(report))
    print(json.dumps({
        "status": report["status"],
        "asset_id": report["asset_id"],
        "report": str(report_path),
        "markdown": str(markdown_path),
        "ready_for_stress_proof": report["gates"]["ready_for_stress_proof"],
    }, indent=2, sort_keys=True))
    return 0 if report["gates"]["ready_for_sidecar"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        raise SystemExit(2)
