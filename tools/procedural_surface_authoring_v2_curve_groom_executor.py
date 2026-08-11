#!/usr/bin/env python3
"""Execute receipt-bound V2 curve-groom requests through the PSG-23E tool."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

SCHEMA = "ray_tracing.surface_authoring_curve_groom_execution_catalog"


class ExecutionError(ValueError):
    pass


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked(path_value: object, expected: object, field: str) -> Path:
    if not isinstance(path_value, str) or not isinstance(expected, str):
        raise ExecutionError(f"{field} path/digest is required")
    path = Path(path_value).resolve()
    if not path.is_file() or digest(path) != expected:
        raise ExecutionError(f"{field} is stale or missing")
    return path


def catalog_entry(value: object, field: str) -> Path:
    if not isinstance(value, dict):
        raise ExecutionError(f"{field} must be an object")
    return checked(value.get("path"), value.get("digest_sha256"), field)


def groom_sets(groom: dict[str, Any]) -> list[str]:
    # PSG-23E owns final field validation.  JSON values preserve vector and
    # numeric identity when forwarding the V2 document's bounded controls.
    return [f"groom.{key}={json.dumps(groom[key], separators=(',', ':'))}"
            for key in sorted(groom)]


def execute(plan: dict[str, Any], catalog: dict[str, Any], output_root: Path) -> dict[str, Any]:
    if (catalog.get("schema") != SCHEMA or catalog.get("schema_version") != 1 or
            catalog.get("source") != plan.get("source")):
        raise ExecutionError("unsupported or source-mismatched execution catalog")
    planned = {item.get("consumer_id"): item
               for item in plan.get("curve_groom_requests", []) if isinstance(item, dict)}
    configured = {item.get("consumer_id"): item
                  for item in catalog.get("requests", []) if isinstance(item, dict)}
    if not planned or set(planned) != set(configured) or None in planned:
        raise ExecutionError("catalog requests do not exactly match curve groom plan")
    output_root = output_root.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    results = []
    for consumer_id in sorted(planned):
        request, config = planned[consumer_id], configured[consumer_id]
        recipe = request.get("curve_groom_recipe")
        if not isinstance(recipe, dict) or not isinstance(request.get("groom"), dict):
            raise ExecutionError("curve groom request is malformed")
        tool = checked(recipe.get("groom_tool_path"), recipe.get("groom_tool_digest_sha256"),
                       f"{consumer_id}.groom_tool")
        authoring = checked(recipe.get("authoring_path"), recipe.get("authoring_digest_sha256"),
                            f"{consumer_id}.groom_authoring")
        mesh = catalog_entry(config.get("source_mesh"), f"{consumer_id}.source_mesh")
        carrier = catalog_entry(config.get("carrier"), f"{consumer_id}.carrier")
        if config["carrier"]["digest_sha256"] != request["root"]["selector_resource_digest_sha256"]:
            raise ExecutionError("carrier does not match planned selector resource")
        try:
            if json.loads(mesh.read_text(encoding="utf-8")).get("mesh_digest_sha256") != request["source"]["mesh_digest_sha256"]:
                raise ExecutionError("source mesh does not match planned identity")
        except json.JSONDecodeError as error:
            raise ExecutionError("source mesh is not inspectable") from error
        root = output_root / consumer_id
        root.mkdir(parents=True, exist_ok=True)
        effective = root / "groom.authoring.json"
        asset = root / "groom.curve.runtime.json"
        receipt = root / "groom.receipt.json"
        edit = [sys.executable, str(tool), "edit", "--input", str(authoring),
                "--output", str(effective), "--expect-sha256", digest(authoring)]
        for assignment in groom_sets(request["groom"]):
            edit.extend(["--set", assignment])
        result = subprocess.run(edit, text=True, capture_output=True)
        if result.returncode:
            raise ExecutionError(f"curve groom edit failed: {result.stderr.strip()}")
        result = subprocess.run([sys.executable, str(tool), "compile", "--authoring", str(effective),
                                 "--mesh", str(mesh), "--region", str(carrier), "--output", str(asset),
                                 "--receipt", str(receipt)], text=True, capture_output=True)
        if result.returncode:
            raise ExecutionError(f"curve groom compiler failed: {result.stderr.strip()}")
        summary = json.loads(receipt.read_text(encoding="utf-8"))
        required = ("exact_source_and_carrier_binding", "root_triangle_mapping_retained",
                    "root_barycentrics_valid", "finite_positive_curve_asset",
                    "guide_assignment_complete", "replaceable_serialized_curve_asset")
        if not all(summary.get(key) is True for key in required):
            raise ExecutionError("curve groom output failed provenance/runtime gates")
        if summary.get("strand_count") != request["groom"]["strand_count"]:
            raise ExecutionError("curve groom output did not realize strand_count")
        results.append({
            "consumer_id": consumer_id,
            "execution": "executed_separate_serialized_curve_asset",
            "source_mesh_digest_sha256": request["source"]["mesh_digest_sha256"],
            "source_mesh_immutable": True,
            "curve_asset_path": str(asset), "curve_asset_digest_sha256": digest(asset),
            "effective_authoring_path": str(effective),
            "effective_authoring_digest_sha256": digest(effective),
            "receipt_path": str(receipt), "receipt": summary,
            "root": request["root"], "groom": request["groom"],
            "material_target": request["material_target"],
        })
    return {"schema": "ray_tracing.surface_authoring_curve_groom_execution_receipt",
            "schema_version": 1, "source": plan["source"], "requests": results,
            "geometry_mutation": "separate_curve_asset_only_source_immutable",
            "scene_promotion": "forbidden"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = execute(json.loads(args.plan.read_text(encoding="utf-8")),
                          json.loads(args.catalog.read_text(encoding="utf-8")),
                          args.output_root)
        print(json.dumps({"status": "ok", "receipt": receipt}, sort_keys=True))
        return 0
    except (ExecutionError, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
