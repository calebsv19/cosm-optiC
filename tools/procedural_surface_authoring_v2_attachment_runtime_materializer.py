#!/usr/bin/env python3
"""Materialize a digest-bound runtime composition for v2 attached assets.

This adapter owns no scene and mutates no mesh.  It converts a successful
PSG-22 execution receipt into the explicit source/growth/material/lighting
binding a renderer must consume.  A proof scene may add cameras, but may not
silently substitute either asset, material target, or lighting contract.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

SCHEMA = "ray_tracing.surface_authoring_attachment_runtime_catalog"
BINDING_SCHEMA = "ray_tracing.surface_authoring_attachment_runtime_binding"


class MaterializationError(ValueError):
    pass


def canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def need_id(value: object, field: str) -> str:
    if not isinstance(value, str) or not value or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for c in value):
        raise MaterializationError(f"{field} must be a stable identifier")
    return value


def need_digest(value: object, field: str) -> str:
    if not isinstance(value, str) or len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
        raise MaterializationError(f"{field} must be a lowercase sha256")
    return value


def checked_entry(value: object, field: str) -> Path:
    if not isinstance(value, dict):
        raise MaterializationError(f"{field} must be an object")
    path_value = value.get("path")
    expected = need_digest(value.get("digest_sha256"), f"{field}.digest_sha256")
    if not isinstance(path_value, str) or not path_value:
        raise MaterializationError(f"{field}.path is required")
    path = Path(path_value).resolve()
    if not path.is_file() or digest(path) != expected:
        raise MaterializationError(f"{field} is stale or missing")
    return path


def number(value: object, field: str, low: float = 0.0, high: float = 1.0) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not low <= float(value) <= high:
        raise MaterializationError(f"{field} must be in [{low}, {high}]")
    return float(value)


def material(value: object, field: str, required_identity: dict[str, str] | None = None) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise MaterializationError(f"{field} must be an object")
    result = {
        "id": need_id(value.get("id"), f"{field}.id"),
        "base_color": {key: number(value.get("base_color", {}).get(key), f"{field}.base_color.{key}")
                       for key in ("r", "g", "b")},
        "roughness": number(value.get("roughness"), f"{field}.roughness"),
        "metallic": number(value.get("metallic"), f"{field}.metallic"),
    }
    if required_identity is not None:
        for key, expected in required_identity.items():
            if value.get(key) != expected:
                raise MaterializationError(f"{field}.{key} does not match planned material target")
            result[key] = expected
    return result


def lighting(value: object) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise MaterializationError("runtime.lighting must be an object")
    if value.get("environment_light_mode") != "ambient":
        raise MaterializationError("runtime.lighting.environment_light_mode must be ambient")
    result = {
        "ambient_strength": number(value.get("ambient_strength"), "runtime.lighting.ambient_strength", 0.01, 4.0),
        "environment_brightness": number(value.get("environment_brightness"), "runtime.lighting.environment_brightness", 0.01, 4.0),
        "background_brightness": number(value.get("background_brightness"), "runtime.lighting.background_brightness", 0.0, 4.0),
        "top_fill_strength": number(value.get("top_fill_strength"), "runtime.lighting.top_fill_strength", 0.0, 4.0),
        "light_intensity": number(value.get("light_intensity"), "runtime.lighting.light_intensity", 0.01, 8.0),
        "environment_light_mode": "ambient",
        "background_color": {key: number(value.get("background_color", {}).get(key), f"runtime.lighting.background_color.{key}")
                             for key in ("r", "g", "b")},
    }
    return result


def mesh_asset(path: Path, field: str) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise MaterializationError(f"{field} is not a readable runtime mesh") from error
    if payload.get("schema_variant") != "mesh_asset_runtime_v1":
        raise MaterializationError(f"{field} must be mesh_asset_runtime_v1")
    return {"asset_id": need_id(payload.get("asset_id"), f"{field}.asset_id"),
            "path": str(path), "digest_sha256": digest(path)}


def materialize(execution: dict[str, Any], catalog: dict[str, Any], output_root: Path) -> dict[str, Any]:
    if execution.get("schema") != "ray_tracing.surface_authoring_attachment_execution_receipt":
        raise MaterializationError("unsupported execution receipt")
    if catalog.get("schema") != SCHEMA or catalog.get("schema_version") != 1 or catalog.get("source") != execution.get("source"):
        raise MaterializationError("unsupported or source-mismatched runtime catalog")
    executed = {item.get("consumer_id"): item for item in execution.get("requests", []) if isinstance(item, dict)}
    configured = {item.get("consumer_id"): item for item in catalog.get("requests", []) if isinstance(item, dict)}
    if not executed or set(executed) != set(configured):
        raise MaterializationError("runtime catalog requests do not exactly match execution receipt")
    output_root = output_root.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    bindings = []
    for consumer_id in sorted(executed):
        receipt, config = executed[consumer_id], configured[consumer_id]
        source_path = checked_entry(config.get("source_mesh"), f"{consumer_id}.source_mesh")
        growth_path = checked_entry(config.get("growth_mesh"), f"{consumer_id}.growth_mesh")
        if str(growth_path) != receipt.get("asset_path") or digest(growth_path) != receipt.get("asset_digest_sha256"):
            raise MaterializationError("runtime growth mesh does not match execution asset")
        source = mesh_asset(source_path, f"{consumer_id}.source_mesh")
        growth = mesh_asset(growth_path, f"{consumer_id}.growth_mesh")
        if receipt.get("source_mesh_digest_sha256") != execution["source"].get("mesh_digest_sha256"):
            raise MaterializationError("execution source mesh digest is inconsistent")
        runtime = config.get("runtime")
        if not isinstance(runtime, dict):
            raise MaterializationError(f"{consumer_id}.runtime must be an object")
        target = receipt.get("material_target")
        if not isinstance(target, dict):
            raise MaterializationError("execution receipt has no material target")
        target_identity = {key: target.get(key) for key in ("resource_id", "resource_digest_sha256", "receipt_digest_sha256")}
        if any(not isinstance(value, str) for value in target_identity.values()):
            raise MaterializationError("execution material target is invalid")
        binding = {
            "schema": BINDING_SCHEMA,
            "schema_version": 1,
            "consumer_id": consumer_id,
            "source": {**source, "mesh_digest_sha256": receipt["source_mesh_digest_sha256"],
                       "object_id": need_id(runtime.get("source_object_id"), f"{consumer_id}.runtime.source_object_id"),
                       "material": material(runtime.get("source_material"), f"{consumer_id}.runtime.source_material")},
            "attachment": {**growth, "mesh_digest_sha256": receipt["growth_mesh_digest_sha256"],
                           "object_id": need_id(runtime.get("attachment_object_id"), f"{consumer_id}.runtime.attachment_object_id"),
                           "material": material(runtime.get("attachment_material"), f"{consumer_id}.runtime.attachment_material", target_identity)},
            "lighting": lighting(runtime.get("lighting")),
            "geometry_mutation": "separate_attached_asset_only_source_immutable",
            "scene_promotion": "forbidden",
        }
        if binding["source"]["object_id"] == binding["attachment"]["object_id"]:
            raise MaterializationError("source and attachment object IDs must differ")
        binding_path = output_root / consumer_id / "attachment.runtime.binding.json"
        binding_path.parent.mkdir(parents=True, exist_ok=True)
        binding_path.write_text(canonical(binding) + "\n", encoding="utf-8")
        bindings.append({"consumer_id": consumer_id, "binding_path": str(binding_path),
                         "binding_digest_sha256": digest(binding_path), "binding": binding})
    return {"schema": "ray_tracing.surface_authoring_attachment_runtime_materialization_receipt",
            "schema_version": 1, "source": execution["source"], "bindings": bindings,
            "scene_promotion": "forbidden"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution-receipt", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = materialize(json.loads(args.execution_receipt.read_text()), json.loads(args.catalog.read_text()), args.output_root)
        print(json.dumps({"status": "ok", "receipt": receipt}, sort_keys=True))
        return 0
    except (MaterializationError, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
