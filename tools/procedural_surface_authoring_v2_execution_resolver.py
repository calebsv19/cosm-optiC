#!/usr/bin/env python3
"""Resolve receipt-bound v2 authoring resources into concrete runtime paths.

The editable document intentionally contains no machine paths. This resolver
validates a separate artifact catalog and emits only the existing runtime
material reference shape; it neither executes a family compiler nor mutates a
scene or mesh.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).parent))
import procedural_surface_authoring_document_v2 as document

SCHEMA = "ray_tracing.surface_authoring_execution_catalog"
VERSION = 1


class ResolverError(ValueError):
    pass


def canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_path(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def entry(value: object, field: str) -> dict[str, str]:
    if not isinstance(value, dict):
        raise ResolverError(f"{field} must be an object")
    path = value.get("path")
    if not isinstance(path, str) or not path:
        raise ResolverError(f"{field}.path is required")
    return {"path": path,
            "digest_sha256": document.need_hex(value.get("digest_sha256"),
                                                f"{field}.digest_sha256")}


def checked_path(entry_value: dict[str, str], field: str) -> str:
    path = Path(entry_value["path"])
    if not path.is_file():
        raise ResolverError(f"{field}.path is missing")
    actual = sha256_path(path)
    if actual != entry_value["digest_sha256"]:
        raise ResolverError(f"{field}.digest_sha256 is stale")
    return str(path.resolve())


def runtime_entries(kind: str, value: object, field: str) -> dict[str, dict[str, str]]:
    if not isinstance(value, dict):
        raise ResolverError(f"{field} must be an object")
    expected = {
        "material_graph": ("binding", "authored_binding", "graph"),
        "selector_carrier": ("surface_region",),
        "microdetail_field": ("surface_feature_field",),
        "signed_relief_recipe": ("relief_recipe", "surface_feature_field"),
        "deep_inset_recipe": ("inset_recipe", "surface_feature_field"),
        "attachment_recipe": ("attachment_recipe",),
        # The editable groom authoring is the resource artifact.  The tool is
        # separately digest-bound so a request cannot silently change compiler.
        "curve_groom_recipe": ("groom_authoring", "groom_tool"),
    }.get(kind)
    if expected is None:
        return {}
    if set(value) != set(expected):
        raise ResolverError(f"{field} keys do not match resource kind")
    return {key: entry(value[key], f"{field}.{key}") for key in expected}


def validate_catalog(value: object, authoring: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(value, dict) or value.get("schema") != SCHEMA or value.get("schema_version") != VERSION:
        raise ResolverError("unsupported catalog schema/version")
    source = value.get("source")
    if source != authoring["source"]:
        raise ResolverError("catalog source does not match document source")
    raw = value.get("resources")
    if not isinstance(raw, list):
        raise ResolverError("catalog.resources must be an array")
    authoring_resources = {item["id"]: item for item in authoring["resources"]}
    result = {}
    for index, item in enumerate(raw):
        field = f"catalog.resources[{index}]"
        if not isinstance(item, dict):
            raise ResolverError(f"{field} must be an object")
        resource_id = document.need_id(item.get("id"), f"{field}.id")
        if resource_id not in authoring_resources or resource_id in result:
            raise ResolverError(f"{field}.id is unknown or duplicate")
        expected = authoring_resources[resource_id]
        artifact = entry(item.get("artifact"), f"{field}.artifact")
        receipt = entry(item.get("receipt"), f"{field}.receipt")
        if (artifact["digest_sha256"] != expected["digest_sha256"] or
                receipt["digest_sha256"] != expected["receipt"]["digest_sha256"]):
            raise ResolverError(f"{field} does not match document identity")
        runtime = runtime_entries(expected["kind"], item.get("runtime", {}),
                                  f"{field}.runtime")
        primary_runtime_key = {
            "material_graph": "graph",
            "selector_carrier": "surface_region",
            "microdetail_field": "surface_feature_field",
            "signed_relief_recipe": "relief_recipe",
            "deep_inset_recipe": "inset_recipe",
            "attachment_recipe": "attachment_recipe",
            "curve_groom_recipe": "groom_authoring",
        }.get(expected["kind"])
        if (primary_runtime_key is not None and
                runtime[primary_runtime_key]["digest_sha256"] !=
                artifact["digest_sha256"]):
            raise ResolverError(f"{field}.runtime primary artifact digest is stale")
        result[resource_id] = {
            "artifact": artifact, "receipt": receipt,
            "runtime": runtime,
        }
    return result


def resolve(authoring_value: object, catalog_value: object) -> dict[str, Any]:
    authoring = document.validate(authoring_value)
    catalog = validate_catalog(catalog_value, authoring)
    resources = {item["id"]: item for item in authoring["resources"]}
    nodes = {item["id"]: item for item in authoring["nodes"]}
    plan = document.compile_plan(authoring)
    required_ids = set(resources)
    if not required_ids.issubset(catalog):
        raise ResolverError("catalog is missing a document resource")
    checked: dict[str, dict[str, Any]] = {}
    for resource_id in sorted(required_ids):
        resolved = catalog[resource_id]
        checked[resource_id] = {
            "artifact_path": checked_path(resolved["artifact"],
                                           f"catalog.{resource_id}.artifact"),
            "receipt_path": checked_path(resolved["receipt"],
                                          f"catalog.{resource_id}.receipt"),
            "runtime": {key: checked_path(item, f"catalog.{resource_id}.runtime.{key}")
                        for key, item in resolved["runtime"].items()},
        }
    material = [item for item in plan["adapters"]
                if item["adapter"] == "material_runtime_binding"]
    microdetail = [item for item in plan["adapters"]
                   if item["adapter"] == "microdetail_field_binding"]
    selectors = [item for item in plan["adapters"]
                 if item["adapter"] == "selector_carrier_binding"]
    if len(material) != 1 or len(microdetail) != 1:
        raise ResolverError("this executable bridge requires one material and one microdetail consumer")
    material_runtime = checked[material[0]["resource"]["id"]]["runtime"]
    microdetail_runtime = checked[microdetail[0]["resource"]["id"]]["runtime"]
    named_selectors = []
    for adapter in selectors:
        source_node = nodes[adapter["input"]["node"]]
        if source_node["kind"] != "selector":
            raise ResolverError("selector carrier binding must be driven by a selector node")
        resource_id = adapter["resource"]["id"]
        named_selectors.append({
            "name": source_node["selector_name"],
            "surface_region_path": checked[resource_id]["runtime"]["surface_region"],
            "resource_id": resource_id,
            "resource_digest_sha256": resources[resource_id]["digest_sha256"],
            "receipt_digest_sha256": resources[resource_id]["receipt"]["digest_sha256"],
        })
    if len({item["name"] for item in named_selectors}) != len(named_selectors):
        raise ResolverError("selector names must be unique")

    signed_relief_requests = []
    deep_inset_requests = []
    attachment_requests = []
    curve_groom_requests = []
    for adapter in plan["adapters"]:
        if (adapter["adapter"] != "geometry_derived_asset_request" or
                adapter.get("domain") != "signed_relief"):
            continue
        output_node = nodes[adapter["input"]["node"]]
        if output_node["kind"] != "domain_output" or output_node.get("domain") != "signed_relief":
            raise ResolverError("signed relief request must be driven by a signed_relief output")
        scalar_input = next((connection["from"] for connection in authoring["connections"]
                             if connection["to"]["node"] == output_node["id"] and
                             connection["to"]["port"] == "value"), None)
        if scalar_input is None:
            raise ResolverError("signed relief output has no scalar input")
        scalar_node = nodes[scalar_input["node"]]
        if scalar_node["kind"] != "scalar_field":
            raise ResolverError("signed relief output must be driven by a scalar field")
        scalar_resource_id = scalar_node["resource"]
        recipe_resource_id = adapter["resource"]["id"]
        signed_relief_requests.append({
            "consumer_id": adapter["consumer_id"],
            "execution": "request_only_no_geometry_mutation",
            "executor": "procedural_surface_feature_relief_shell",
            "source": authoring["source"],
            "scalar_field": {
                "node_id": scalar_node["id"],
                "resource_id": scalar_resource_id,
                "resource_digest_sha256": resources[scalar_resource_id]["digest_sha256"],
                "receipt_digest_sha256": resources[scalar_resource_id]["receipt"]["digest_sha256"],
            },
            "signed_relief_recipe": {
                "resource_id": recipe_resource_id,
                "path": checked[recipe_resource_id]["runtime"]["relief_recipe"],
                "surface_feature_field_path": checked[recipe_resource_id]["runtime"]["surface_feature_field"],
                "resource_digest_sha256": resources[recipe_resource_id]["digest_sha256"],
                "receipt_digest_sha256": resources[recipe_resource_id]["receipt"]["digest_sha256"],
            },
        })
    for adapter in plan["adapters"]:
        if (adapter["adapter"] != "geometry_derived_asset_request" or
                adapter.get("domain") != "deep_inset"):
            continue
        output_node = nodes[adapter["input"]["node"]]
        if output_node["kind"] != "domain_output" or output_node.get("domain") != "deep_inset":
            raise ResolverError("deep inset request must be driven by a deep_inset output")
        scalar_input = next((connection["from"] for connection in authoring["connections"]
                             if connection["to"]["node"] == output_node["id"] and
                             connection["to"]["port"] == "value"), None)
        if scalar_input is None or nodes[scalar_input["node"]]["kind"] != "scalar_field":
            raise ResolverError("deep inset output must be driven by a scalar field")
        scalar_node = nodes[scalar_input["node"]]
        recipe_resource_id = adapter["resource"]["id"]
        feature_field_path = checked[recipe_resource_id]["runtime"]["surface_feature_field"]
        try:
            feature_field = json.loads(Path(feature_field_path).read_text(encoding="utf-8"))
            feature_values = {int(item["feature_id"]): float(item["height_or_depth"])
                              for item in feature_field["features"]}
        except (KeyError, TypeError, ValueError, json.JSONDecodeError, OSError) as error:
            raise ResolverError("deep inset feature field is not inspectable") from error
        feature_ids = adapter.get("feature_ids", [])
        if any(feature_id not in feature_values for feature_id in feature_ids):
            raise ResolverError("deep inset feature IDs are absent from feature field")
        if any(feature_values[feature_id] >= 0.0 for feature_id in feature_ids):
            raise ResolverError("deep inset requires explicitly negative feature values")
        deep_inset_requests.append({
            "consumer_id": adapter["consumer_id"],
            "execution": "request_only_no_geometry_mutation",
            "executor": "procedural_surface_feature_inset_compiler",
            "source": authoring["source"],
            "scalar_field": {"node_id": scalar_node["id"],
                             "resource_id": scalar_node["resource"],
                             "resource_digest_sha256": resources[scalar_node["resource"]]["digest_sha256"],
                             "receipt_digest_sha256": resources[scalar_node["resource"]]["receipt"]["digest_sha256"]},
            "deep_inset_recipe": {"resource_id": recipe_resource_id,
                                  "path": checked[recipe_resource_id]["runtime"]["inset_recipe"],
                                  "resource_digest_sha256": resources[recipe_resource_id]["digest_sha256"],
                                  "receipt_digest_sha256": resources[recipe_resource_id]["receipt"]["digest_sha256"]},
            "feature_ids": feature_ids,
            "feature_field_path": feature_field_path,
            "derived_asset_requirement": "distinct_closed_shell_required",
        })
    for adapter in plan["adapters"]:
        if adapter["adapter"] != "attachment_compile_request":
            continue
        output_node = nodes[adapter["input"]["node"]]
        if output_node["kind"] != "domain_output" or output_node.get("domain") != "attached_asset":
            raise ResolverError("attachment request must be driven by an attached_asset output")
        scalar_input = next((connection["from"] for connection in authoring["connections"]
                             if connection["to"]["node"] == output_node["id"] and
                             connection["to"]["port"] == "value"), None)
        if scalar_input is None or nodes[scalar_input["node"]]["kind"] != "scalar_field":
            raise ResolverError("attachment output must be driven by a scalar field")
        scalar_node = nodes[scalar_input["node"]]
        selector_input = next((connection["from"] for connection in authoring["connections"]
                               if connection["to"]["node"] == scalar_node["id"]), None)
        if selector_input is None or nodes[selector_input["node"]]["kind"] != "selector":
            raise ResolverError("attachment scalar field must be driven by a selector")
        selector_node = nodes[selector_input["node"]]
        recipe_resource_id = adapter["resource"]["id"]
        material_resource_id = adapter["material_resource"]
        attachment_requests.append({
            "consumer_id": adapter["consumer_id"],
            "execution": "request_only_no_geometry_mutation",
            "executor": "procedural_imported_surface_growth",
            "source": authoring["source"],
            "root": {"policy": adapter["root_policy"],
                     "selector_name": selector_node["selector_name"],
                     "selector_resource_id": selector_node["resource"],
                     "selector_resource_digest_sha256": resources[selector_node["resource"]]["digest_sha256"]},
            "scalar_field": {"node_id": scalar_node["id"], "resource_id": scalar_node["resource"],
                             "resource_digest_sha256": resources[scalar_node["resource"]]["digest_sha256"],
                             "receipt_digest_sha256": resources[scalar_node["resource"]]["receipt"]["digest_sha256"]},
            "attachment_recipe": {"resource_id": recipe_resource_id,
                                  "path": checked[recipe_resource_id]["runtime"]["attachment_recipe"],
                                  "resource_digest_sha256": resources[recipe_resource_id]["digest_sha256"],
                                  "receipt_digest_sha256": resources[recipe_resource_id]["receipt"]["digest_sha256"]},
            "clearance_factor": adapter["clearance_factor"],
            "material_target": {"resource_id": material_resource_id,
                                "resource_digest_sha256": resources[material_resource_id]["digest_sha256"],
                                "receipt_digest_sha256": resources[material_resource_id]["receipt"]["digest_sha256"]},
            "derived_asset_requirement": "separate_closed_attached_asset_required",
        })
    for adapter in plan["adapters"]:
        if adapter["adapter"] != "curve_groom_compile_request":
            continue
        output_node = nodes[adapter["input"]["node"]]
        if output_node["kind"] != "domain_output" or output_node.get("domain") != "curve_groom":
            raise ResolverError("curve groom request must be driven by a curve_groom output")
        scalar_input = next((connection["from"] for connection in authoring["connections"]
                             if connection["to"]["node"] == output_node["id"] and
                             connection["to"]["port"] == "value"), None)
        if scalar_input is None or nodes[scalar_input["node"]]["kind"] != "scalar_field":
            raise ResolverError("curve groom output must be driven by a scalar field")
        scalar_node = nodes[scalar_input["node"]]
        selector_input = next((connection["from"] for connection in authoring["connections"]
                               if connection["to"]["node"] == scalar_node["id"]), None)
        if selector_input is None or nodes[selector_input["node"]]["kind"] != "selector":
            raise ResolverError("curve groom scalar field must be driven by a selector")
        selector_node = nodes[selector_input["node"]]
        recipe_resource_id = adapter["resource"]["id"]
        material_resource_id = adapter["material_resource"]
        curve_groom_requests.append({
            "consumer_id": adapter["consumer_id"],
            "execution": "request_only_no_geometry_mutation",
            "executor": "procedural_carrier_curve_groom_authoring",
            "source": authoring["source"],
            "root": {"policy": adapter["root_policy"],
                     "selector_name": selector_node["selector_name"],
                     "selector_resource_id": selector_node["resource"],
                     "selector_resource_digest_sha256": resources[selector_node["resource"]]["digest_sha256"]},
            "scalar_field": {"node_id": scalar_node["id"], "resource_id": scalar_node["resource"],
                             "resource_digest_sha256": resources[scalar_node["resource"]]["digest_sha256"],
                             "receipt_digest_sha256": resources[scalar_node["resource"]]["receipt"]["digest_sha256"]},
            "curve_groom_recipe": {
                "resource_id": recipe_resource_id,
                "authoring_path": checked[recipe_resource_id]["runtime"]["groom_authoring"],
                "authoring_digest_sha256": resources[recipe_resource_id]["digest_sha256"],
                "groom_tool_path": checked[recipe_resource_id]["runtime"]["groom_tool"],
                "groom_tool_digest_sha256": catalog[recipe_resource_id]["runtime"]["groom_tool"]["digest_sha256"],
                "resource_digest_sha256": resources[recipe_resource_id]["digest_sha256"],
                "receipt_digest_sha256": resources[recipe_resource_id]["receipt"]["digest_sha256"],
            },
            "groom": adapter["groom"],
            "material_target": {"resource_id": material_resource_id,
                                "resource_digest_sha256": resources[material_resource_id]["digest_sha256"],
                                "receipt_digest_sha256": resources[material_resource_id]["receipt"]["digest_sha256"]},
            "derived_asset_requirement": "separate_serialized_curve_asset_required",
        })
    runtime_ref = {
        "binding_path": material_runtime["binding"],
        "authored_binding_path": material_runtime["authored_binding"],
        "graph_path": material_runtime["graph"],
        "surface_feature_field_path": microdetail_runtime["surface_feature_field"],
        "named_surface_selectors": sorted(named_selectors, key=lambda item: item["name"]),
    }
    return {
        "schema": "ray_tracing.surface_authoring_execution_plan",
        "schema_version": 1,
        "document_digest_sha256": document.digest(authoring),
        "source": authoring["source"],
        "runtime_material_ref": runtime_ref,
        "signed_relief_requests": sorted(signed_relief_requests,
                                          key=lambda item: item["consumer_id"]),
        "deep_inset_requests": sorted(deep_inset_requests,
                                       key=lambda item: item["consumer_id"]),
        "attachment_requests": sorted(attachment_requests,
                                      key=lambda item: item["consumer_id"]),
        "curve_groom_requests": sorted(curve_groom_requests,
                                         key=lambda item: item["consumer_id"]),
        "verified_resources": sorted(checked),
        "geometry_mutation": "forbidden",
        "scene_promotion": "forbidden",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--document", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = resolve(json.loads(args.document.read_text(encoding="utf-8")),
                         json.loads(args.catalog.read_text(encoding="utf-8")))
        print(json.dumps({"status": "ok", "execution_plan": result}, sort_keys=True))
        return 0
    except (ResolverError, document.Error, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
