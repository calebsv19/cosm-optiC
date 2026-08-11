#!/usr/bin/env python3
"""Materialize V2 curve-groom execution into independent runtime objects."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any

SCHEMA = "ray_tracing.surface_authoring_curve_groom_runtime_catalog"
BINDING_SCHEMA = "ray_tracing.surface_authoring_curve_groom_runtime_binding"


class MaterializationError(ValueError):
    pass


def canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def need_id(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise MaterializationError(f"{field} is required")
    return value


def number(value: object, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(float(value)):
        raise MaterializationError(f"{field} must be finite")
    return float(value)


def material(value: object, field: str, identity: dict[str, str] | None = None) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise MaterializationError(f"{field} must be an object")
    result = {"id": need_id(value.get("id"), f"{field}.id"),
              "base_color": {key: number(value.get("base_color", {}).get(key), f"{field}.base_color.{key}")
                             for key in ("r", "g", "b")},
              "roughness": number(value.get("roughness"), f"{field}.roughness"),
              "metallic": number(value.get("metallic"), f"{field}.metallic")}
    if not 0.0 <= result["roughness"] <= 1.0 or not 0.0 <= result["metallic"] <= 1.0:
        raise MaterializationError(f"{field} material scalars must be in [0, 1]")
    if identity:
        for key, expected in identity.items():
            if value.get(key) != expected:
                raise MaterializationError(f"{field}.{key} does not match planned material target")
            result[key] = expected
    return result


def checked(value: object, expected_path: str, expected_digest: str, schema_variant: str, field: str) -> dict[str, str]:
    if (not isinstance(value, dict) or not isinstance(expected_path, str) or
            not isinstance(value.get("path"), str) or
            Path(value["path"]).resolve() != Path(expected_path).resolve()):
        raise MaterializationError(f"{field} path does not match execution asset")
    path = Path(expected_path)
    if not path.is_file() or digest(path) != expected_digest or value.get("digest_sha256") != expected_digest:
        raise MaterializationError(f"{field} is stale or missing")
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("schema_variant") != schema_variant:
        raise MaterializationError(f"{field} has unsupported runtime schema")
    return {"asset_id": need_id(payload.get("asset_id"), f"{field}.asset_id"),
            "path": str(path.resolve()), "digest_sha256": expected_digest}


def materialize(execution: dict[str, Any], catalog: dict[str, Any], output_root: Path) -> dict[str, Any]:
    if (execution.get("schema") != "ray_tracing.surface_authoring_curve_groom_execution_receipt" or
            catalog.get("schema") != SCHEMA or catalog.get("schema_version") != 1 or
            catalog.get("source") != execution.get("source")):
        raise MaterializationError("unsupported or source-mismatched runtime catalog")
    executed = {item.get("consumer_id"): item for item in execution.get("requests", []) if isinstance(item, dict)}
    configured = {item.get("consumer_id"): item for item in catalog.get("requests", []) if isinstance(item, dict)}
    if not executed or set(executed) != set(configured) or None in executed:
        raise MaterializationError("catalog requests do not exactly match curve groom execution")
    output_root.mkdir(parents=True, exist_ok=True)
    bindings = []
    for consumer_id in sorted(executed):
        receipt, config = executed[consumer_id], configured[consumer_id]
        source_config = config.get("source_mesh")
        if not isinstance(source_config, dict):
            raise MaterializationError(f"{consumer_id}.source_mesh must be an object")
        source = checked(source_config, source_config.get("path"),
                         source_config.get("digest_sha256"), "mesh_asset_runtime_v1",
                         f"{consumer_id}.source_mesh")
        curve = checked(config.get("curve_asset"), receipt.get("curve_asset_path"),
                        receipt.get("curve_asset_digest_sha256"), "curve_asset_runtime_v1",
                        f"{consumer_id}.curve_asset")
        if receipt.get("source_mesh_digest_sha256") != execution["source"].get("mesh_digest_sha256"):
            raise MaterializationError("curve groom source mesh digest is inconsistent")
        runtime = config.get("runtime")
        if not isinstance(runtime, dict):
            raise MaterializationError(f"{consumer_id}.runtime must be an object")
        target = receipt.get("material_target")
        if not isinstance(target, dict):
            raise MaterializationError("curve groom execution has no material target")
        identity = {key: target.get(key) for key in ("resource_id", "resource_digest_sha256", "receipt_digest_sha256")}
        if any(not isinstance(value, str) for value in identity.values()):
            raise MaterializationError("curve groom material target is invalid")
        source_object_id = need_id(runtime.get("source_object_id"), f"{consumer_id}.runtime.source_object_id")
        curve_object_id = need_id(runtime.get("curve_object_id"), f"{consumer_id}.runtime.curve_object_id")
        if source_object_id == curve_object_id:
            raise MaterializationError("source and curve object IDs must differ")
        binding = {
            "schema": BINDING_SCHEMA, "schema_version": 1, "consumer_id": consumer_id,
            "source": {**source, "mesh_digest_sha256": receipt["source_mesh_digest_sha256"],
                       "object_id": source_object_id,
                       "material": material(runtime.get("source_material"), f"{consumer_id}.runtime.source_material")},
            "curve": {**curve, "object_id": curve_object_id,
                      "object_type": "curve_asset_instance",
                      "material": material(runtime.get("curve_material"), f"{consumer_id}.runtime.curve_material", identity)},
            "geometry_mutation": "separate_curve_asset_only_source_immutable",
            "scene_promotion": "forbidden",
        }
        path = output_root / consumer_id / "curve_groom.runtime.binding.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(canonical(binding) + "\n", encoding="utf-8")
        bindings.append({"consumer_id": consumer_id, "binding_path": str(path),
                         "binding_digest_sha256": digest(path), "binding": binding})
    return {"schema": "ray_tracing.surface_authoring_curve_groom_runtime_materialization_receipt",
            "schema_version": 1, "source": execution["source"], "bindings": bindings,
            "scene_promotion": "forbidden"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution-receipt", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = materialize(json.loads(args.execution_receipt.read_text(encoding="utf-8")),
                              json.loads(args.catalog.read_text(encoding="utf-8")), args.output_root)
        print(json.dumps({"status": "ok", "receipt": receipt}, sort_keys=True))
        return 0
    except (MaterializationError, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
