#!/usr/bin/env python3
"""Execute one receipt-bound v2 signed-relief request through PSG-24R.

This is deliberately a narrow executor.  It accepts only an execution plan
already emitted by the v2 resolver and invokes the existing signed-relief asset
tool; it cannot execute inset or attachment requests, alter a source asset, or
promote a scene.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

SCHEMA = "ray_tracing.surface_authoring_signed_relief_execution_catalog"
VERSION = 1


class ExecutionError(ValueError):
    pass


def sha256_path(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def entry(value: object, field: str) -> tuple[Path, str]:
    if not isinstance(value, dict) or not isinstance(value.get("path"), str):
        raise ExecutionError(f"{field}.path is required")
    digest = value.get("digest_sha256")
    if not isinstance(digest, str) or len(digest) != 64:
        raise ExecutionError(f"{field}.digest_sha256 is required")
    path = Path(value["path"]).resolve()
    if not path.is_file() or sha256_path(path) != digest:
        raise ExecutionError(f"{field} is stale or missing")
    return path, digest


def number(value: object, field: str, positive: bool = True) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ExecutionError(f"{field} must be numeric")
    result = float(value)
    if (positive and result <= 0.0) or not positive and result < 0.0:
        raise ExecutionError(f"{field} is out of range")
    return result


def execute(plan: dict[str, Any], catalog: dict[str, Any], output_root: Path) -> dict[str, Any]:
    if catalog.get("schema") != SCHEMA or catalog.get("schema_version") != VERSION:
        raise ExecutionError("unsupported execution catalog schema/version")
    if catalog.get("source") != plan.get("source"):
        raise ExecutionError("catalog source does not match execution plan")
    requests = catalog.get("requests")
    if not isinstance(requests, list):
        raise ExecutionError("catalog.requests must be an array")
    plan_requests = {item["consumer_id"]: item
                     for item in plan.get("signed_relief_requests", [])}
    catalog_requests = {item.get("consumer_id"): item for item in requests
                        if isinstance(item, dict) and isinstance(item.get("consumer_id"), str)}
    if set(plan_requests) != set(catalog_requests):
        raise ExecutionError("catalog requests do not exactly match signed relief plan")
    output_root = output_root.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    results = []
    for consumer_id in sorted(plan_requests):
        request, config = plan_requests[consumer_id], catalog_requests[consumer_id]
        tool, _ = entry(config.get("asset_tool"), f"requests.{consumer_id}.asset_tool")
        graph, _ = entry(config.get("graph"), f"requests.{consumer_id}.graph")
        binding, _ = entry(config.get("binding"), f"requests.{consumer_id}.binding")
        base_recipe, _ = entry(config.get("base_recipe"), f"requests.{consumer_id}.base_recipe")
        field = Path(request["signed_relief_recipe"]["surface_feature_field_path"]).resolve()
        if not field.is_file():
            raise ExecutionError("resolved signed relief feature field is missing")
        expected_field_digest = config.get("feature_field_digest_sha256")
        if not isinstance(expected_field_digest, str) or sha256_path(field) != expected_field_digest:
            raise ExecutionError("resolved signed relief feature field is stale")
        options = config.get("options")
        if not isinstance(options, dict):
            raise ExecutionError("signed relief execution options are required")
        asset_id = options.get("derived_asset_id")
        source_asset_id = options.get("source_asset_id")
        selected_face = options.get("selected_face")
        if not all(isinstance(value, str) and value for value in (asset_id, source_asset_id, selected_face)):
            raise ExecutionError("signed relief identity options are required")
        root = output_root / consumer_id
        root.mkdir(parents=True, exist_ok=True)
        paths = {name: root / filename for name, filename in {
            "recipe": "recipe.json", "asset": "runtime_mesh.json", "material": "material.json",
            "manifest": "derived_asset.json", "summary": "summary.json"}.items()}
        command = [str(tool), "--graph", str(graph), "--binding", str(binding),
                   "--base-recipe", str(base_recipe), "--recipe-out", str(paths["recipe"]),
                   "--asset-out", str(paths["asset"]), "--material-out", str(paths["material"]),
                   "--manifest-out", str(paths["manifest"]), "--summary-out", str(paths["summary"]),
                   "--width", str(number(options.get("width"), "width")),
                   "--height", str(number(options.get("height"), "height")),
                   "--depth", str(number(options.get("depth"), "depth")),
                   "--target-edge", str(number(options.get("target_edge"), "target_edge")),
                   "--amplitude", str(number(options.get("amplitude"), "amplitude", positive=False)),
                   "--edge-lock", str(number(options.get("edge_lock"), "edge_lock", positive=False)),
                   "--asset-id", asset_id, "--source-asset-id", source_asset_id,
                   "--selected-face", selected_face, "--surface-feature-field", str(field),
                   "--feature-source-mesh-digest", request["source"]["mesh_digest_sha256"],
                   "--relief-scale", str(number(options.get("relief_scale"), "relief_scale"))]
        completed = subprocess.run(command, text=True, capture_output=True)
        if completed.returncode:
            raise ExecutionError(f"signed relief compiler failed: {completed.stderr.strip()}")
        summary = json.loads(paths["summary"].read_text(encoding="utf-8"))
        relief = summary.get("signed_feature_relief", {})
        if (summary.get("mesh_digest_sha256") == request["source"]["mesh_digest_sha256"] or
                relief.get("feature_source_identity_bound") is not True or
                relief.get("one_coherent_derived_shell") is not True or
                summary.get("boundary_edge_count") != 0 or
                summary.get("connected_component_count") != 1 or
                summary.get("selected_face_shell", {}).get("maximum_unselected_face_absolute_displacement_units") != 0.0):
            raise ExecutionError("signed relief output failed source/derived topology gates")
        results.append({"consumer_id": consumer_id, "execution": "executed_distinct_derived_shell",
                        "source_mesh_digest_sha256": request["source"]["mesh_digest_sha256"],
                        "derived_mesh_digest_sha256": summary["mesh_digest_sha256"],
                        "source_mesh_immutable": True, "topology": {"boundary_edge_count": 0,
                        "connected_component_count": 1, "euler_characteristic": summary.get("euler_characteristic")},
                        "paths": {name: str(path) for name, path in paths.items()},
                        "signed_feature_relief": relief})
    return {"schema": "ray_tracing.surface_authoring_signed_relief_execution_receipt",
            "schema_version": 1, "source": plan["source"], "requests": results,
            "geometry_mutation": "derived_shell_only_source_immutable", "scene_promotion": "forbidden"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = execute(json.loads(args.plan.read_text()), json.loads(args.catalog.read_text()), args.output_root)
        print(json.dumps({"status": "ok", "receipt": receipt}, sort_keys=True))
        return 0
    except (ExecutionError, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
