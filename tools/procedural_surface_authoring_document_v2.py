#!/usr/bin/env python3
"""Typed, receipt-bound surface-authoring document v2.

V2 is an orchestration graph, not a geometry generator or a renderer.  It
describes safe paths from selector -> scalar field -> domain output -> a closed
family adapter.  The family compilers remain the authority for generated
artifacts and their receipts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import tempfile
from collections import defaultdict, deque
from pathlib import Path
from typing import Any

SCHEMA = "ray_tracing.surface_authoring_document"
VERSION = 2
HEX = re.compile(r"^[0-9a-f]{64}$")
ID = re.compile(r"^[A-Za-z0-9_.-]+$")

DOMAIN_SELECTOR = "selector_mask"
DOMAIN_SCALAR = "scalar_field"
PHYSICAL_DOMAINS = {
    "material", "microdetail_normal", "signed_relief", "attached_asset",
    "deep_inset", "curve_groom",
}
ALL_DOMAINS = {DOMAIN_SELECTOR, DOMAIN_SCALAR, *PHYSICAL_DOMAINS}

RESOURCE_KINDS = {
    "selector_carrier": {DOMAIN_SELECTOR},
    "scalar_field": {DOMAIN_SCALAR},
    "material_graph": {"material"},
    "microdetail_field": {"microdetail_normal"},
    "signed_relief_recipe": {"signed_relief"},
    "attachment_recipe": {"attached_asset"},
    "curve_groom_recipe": {"curve_groom"},
    "deep_inset_recipe": {"deep_inset"},
}
ADAPTERS = {
    "material_runtime_binding": ("material", "material_graph"),
    "microdetail_field_binding": ("microdetail_normal", "microdetail_field"),
    # A selector owns its carrier.  The binding only routes that selector into
    # the runtime bridge, so it cannot carry a second, conflicting resource.
    "selector_carrier_binding": (DOMAIN_SELECTOR, None),
    "attachment_compile_request": ("attached_asset", "attachment_recipe"),
    "curve_groom_compile_request": ("curve_groom", "curve_groom_recipe"),
    "geometry_derived_asset_request": (None, None),
}


GROOM_NUMBER_RANGES = {
    "selection_threshold": (0.0, 1.0),
    "length": (0.01, 3.0),
    "length_variation": (0.0, 0.9),
    "root_radius": (1.0e-5, 0.25),
    "tip_radius": (1.0e-5, 0.25),
    # A curve-groom attachment must physically root into its carrier.  Zero is
    # valid in the lower-level authoring tool but is not an attachment proof.
    "root_penetration": (1.0e-5, 0.25),
    "lift": (0.05, 4.0),
    "comb_strength": (0.0, 4.0),
    "part_strength": (0.0, 4.0),
    "bend": (-2.0, 2.0),
    "curl": (0.0, 2.0),
    "clump_strength": (0.0, 1.0),
    "clump_tip_spread": (0.0, 1.0),
}
GROOM_INTEGER_RANGES = {
    "strand_count": (4, 4096),
    "guide_count": (1, 256),
    "points_per_strand": (4, 64),
    "seed": (0, 2**31 - 1),
}
GEOMETRY_RESOURCE_BY_DOMAIN = {
    "signed_relief": "signed_relief_recipe",
    "deep_inset": "deep_inset_recipe",
}


class Error(ValueError):
    pass


def canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value).encode("utf-8")).hexdigest()


def need_id(value: object, field: str) -> str:
    if not isinstance(value, str) or not ID.fullmatch(value):
        raise Error(f"{field} must be an identifier")
    return value


def need_hex(value: object, field: str) -> str:
    if not isinstance(value, str) or not HEX.fullmatch(value):
        raise Error(f"{field} must be a lowercase SHA-256 digest")
    return value


def domains(value: object, field: str) -> list[str]:
    if (not isinstance(value, list) or not value or
            any(not isinstance(item, str) or item not in ALL_DOMAINS
                for item in value)):
        raise Error(f"{field} must be non-empty supported domains")
    return sorted(set(value))


def port(value: object, field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise Error(f"{field} must be an object")
    return {"name": need_id(value.get("name"), f"{field}.name"),
            "domains": domains(value.get("domains"), f"{field}.domains")}


def ports(value: object, field: str) -> list[dict[str, Any]]:
    if not isinstance(value, list):
        raise Error(f"{field} must be an array")
    result = [port(item, f"{field}[{index}]")
              for index, item in enumerate(value)]
    if len({item["name"] for item in result}) != len(result):
        raise Error(f"{field} port names must be unique")
    return result


def groom_vector(value: object, field: str) -> list[float]:
    if (not isinstance(value, list) or len(value) != 3 or
            any(isinstance(item, bool) or not isinstance(item, (int, float))
                for item in value)):
        raise Error(f"{field} must be a three-number vector")
    result = [float(item) for item in value]
    if sum(item * item for item in result) <= 1.0e-20:
        raise Error(f"{field} must be nonzero")
    return result


def validate_groom(value: object, field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise Error(f"{field} must be an object")
    expected = (set(GROOM_NUMBER_RANGES) | set(GROOM_INTEGER_RANGES) |
                {"comb_direction", "part_axis"})
    if set(value) != expected:
        raise Error(f"{field} must declare every supported groom control")
    result: dict[str, Any] = {}
    for key, (lower, upper) in GROOM_INTEGER_RANGES.items():
        item = value[key]
        if (isinstance(item, bool) or not isinstance(item, int) or
                not lower <= item <= upper):
            raise Error(f"{field}.{key} outside [{lower}, {upper}]")
        result[key] = item
    if result["guide_count"] > result["strand_count"]:
        raise Error(f"{field}.guide_count cannot exceed strand_count")
    for key, (lower, upper) in GROOM_NUMBER_RANGES.items():
        item = value[key]
        if (isinstance(item, bool) or not isinstance(item, (int, float)) or
                not lower <= float(item) <= upper):
            raise Error(f"{field}.{key} outside [{lower}, {upper}]")
        result[key] = float(item)
    if result["tip_radius"] > result["root_radius"]:
        raise Error(f"{field}.tip_radius cannot exceed root_radius")
    result["comb_direction"] = groom_vector(value["comb_direction"],
                                              f"{field}.comb_direction")
    result["part_axis"] = groom_vector(value["part_axis"],
                                        f"{field}.part_axis")
    return result


def validate_resource(value: object, source_digest: str, field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise Error(f"{field} must be an object")
    kind = value.get("kind")
    if kind not in RESOURCE_KINDS:
        raise Error(f"{field}.kind is unsupported")
    output_domains = domains(value.get("output_domains"), f"{field}.output_domains")
    if set(output_domains) != RESOURCE_KINDS[kind]:
        raise Error(f"{field}.output_domains does not match resource kind")
    receipt = value.get("receipt")
    if not isinstance(receipt, dict):
        raise Error(f"{field}.receipt is required")
    if value.get("source_mesh_digest_sha256") != source_digest:
        raise Error(f"{field}.source_mesh_digest_sha256 must bind document source")
    return {
        "id": need_id(value.get("id"), f"{field}.id"),
        "kind": kind,
        "digest_sha256": need_hex(value.get("digest_sha256"), f"{field}.digest_sha256"),
        "source_mesh_digest_sha256": source_digest,
        "output_domains": output_domains,
        "receipt": {
            "id": need_id(receipt.get("id"), f"{field}.receipt.id"),
            "digest_sha256": need_hex(receipt.get("digest_sha256"), f"{field}.receipt.digest_sha256"),
        },
    }


def validate_node(value: object, resources: dict[str, dict[str, Any]], field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise Error(f"{field} must be an object")
    kind = value.get("kind")
    if kind not in {"selector", "scalar_field", "domain_output", "consumer"}:
        raise Error(f"{field}.kind is unsupported")
    result = {
        "id": need_id(value.get("id"), f"{field}.id"),
        "kind": kind,
        "inputs": ports(value.get("inputs", []), f"{field}.inputs"),
        "outputs": ports(value.get("outputs", []), f"{field}.outputs"),
    }
    resource_id = value.get("resource")
    if resource_id is not None:
        resource_id = need_id(resource_id, f"{field}.resource")
        if resource_id not in resources:
            raise Error(f"{field}.resource is unknown")
        result["resource"] = resource_id
    if kind == "selector":
        if resource_id is None or resources[resource_id]["kind"] != "selector_carrier":
            raise Error(f"{field}.selector needs a selector_carrier resource")
        result["selector_name"] = need_id(
            value.get("selector_name"), f"{field}.selector_name")
        if result["inputs"] or result["outputs"] != [{"name": "mask", "domains": [DOMAIN_SELECTOR]}]:
            raise Error(f"{field}.selector ports are fixed")
    elif kind == "scalar_field":
        if resource_id is None or resources[resource_id]["kind"] != "scalar_field":
            raise Error(f"{field}.scalar_field needs a scalar_field resource")
        if not result["inputs"] or result["outputs"] != [{"name": "value", "domains": [DOMAIN_SCALAR]}]:
            raise Error(f"{field}.scalar_field ports are invalid")
        if any(not set(item["domains"]).issubset({DOMAIN_SELECTOR, DOMAIN_SCALAR})
               for item in result["inputs"]):
            raise Error(f"{field}.scalar_field inputs must be selector/scalar")
    elif kind == "domain_output":
        domain = value.get("domain")
        if domain not in PHYSICAL_DOMAINS:
            raise Error(f"{field}.domain is unsupported")
        if resource_id is not None:
            raise Error(f"{field}.domain_output cannot own a resource")
        if result["inputs"] != [{"name": "value", "domains": [DOMAIN_SCALAR]}] or result["outputs"] != [{"name": "output", "domains": [domain]}]:
            raise Error(f"{field}.domain_output ports do not match domain")
        result["domain"] = domain
    else:
        adapter = value.get("adapter")
        if adapter not in ADAPTERS:
            raise Error(f"{field}.adapter is unsupported")
        expected_domain, expected_resource_kind = ADAPTERS[adapter]
        if adapter == "geometry_derived_asset_request":
            consumer_domain = value.get("domain")
            if consumer_domain not in GEOMETRY_RESOURCE_BY_DOMAIN:
                raise Error(f"{field}.domain needs signed_relief or deep_inset")
            expected_domain = consumer_domain
            expected_resource_kind = GEOMETRY_RESOURCE_BY_DOMAIN[consumer_domain]
            result["domain"] = consumer_domain
            if consumer_domain == "deep_inset":
                feature_ids = value.get("feature_ids")
                if (not isinstance(feature_ids, list) or not feature_ids or
                        any(not isinstance(item, int) or item <= 0 for item in feature_ids) or
                        len(set(feature_ids)) != len(feature_ids)):
                    raise Error(f"{field}.feature_ids must be unique positive integers")
                result["feature_ids"] = sorted(feature_ids)
        elif adapter == "attachment_compile_request":
            root_policy = value.get("root_policy")
            if root_policy not in {"carrier_weighted_surface"}:
                raise Error(f"{field}.root_policy is unsupported")
            clearance_factor = value.get("clearance_factor")
            if (isinstance(clearance_factor, bool) or
                    not isinstance(clearance_factor, (int, float)) or
                    not 0.0 < float(clearance_factor) <= 10.0):
                raise Error(f"{field}.clearance_factor must be in (0, 10]")
            material_resource = need_id(value.get("material_resource"),
                                        f"{field}.material_resource")
            if (material_resource not in resources or
                    resources[material_resource]["kind"] != "material_graph"):
                raise Error(f"{field}.material_resource must be a material_graph")
            result["root_policy"] = root_policy
            result["clearance_factor"] = float(clearance_factor)
            result["material_resource"] = material_resource
        elif adapter == "curve_groom_compile_request":
            root_policy = value.get("root_policy")
            if root_policy != "carrier_weighted_surface":
                raise Error(f"{field}.root_policy is unsupported")
            material_resource = need_id(value.get("material_resource"),
                                        f"{field}.material_resource")
            if (material_resource not in resources or
                    resources[material_resource]["kind"] != "material_graph"):
                raise Error(f"{field}.material_resource must be a material_graph")
            result["root_policy"] = root_policy
            result["material_resource"] = material_resource
            result["groom"] = validate_groom(value.get("groom"),
                                               f"{field}.groom")
        has_expected_resource = (expected_resource_kind is not None and
                                 resource_id is not None and
                                 resources[resource_id]["kind"] == expected_resource_kind)
        has_expected_ports = (
            result["inputs"] == [{"name": "input", "domains": [expected_domain]}] and
            result["outputs"] == [{"name": "request", "domains": [expected_domain]}])
        if ((expected_resource_kind is None and resource_id is not None) or
                (expected_resource_kind is not None and not has_expected_resource) or
                not has_expected_ports):
            raise Error(f"{field}.consumer ports/resource do not match adapter")
        result["adapter"] = adapter
    return result


def validate(document: object) -> dict[str, Any]:
    if not isinstance(document, dict) or document.get("schema") != SCHEMA or document.get("schema_version") != VERSION:
        raise Error("unsupported schema/version")
    source = document.get("source")
    if not isinstance(source, dict):
        raise Error("source is required")
    normalized_source = {
        "object_id": need_id(source.get("object_id"), "source.object_id"),
        "mesh_digest_sha256": need_hex(source.get("mesh_digest_sha256"), "source.mesh_digest_sha256"),
    }
    raw_resources = document.get("resources")
    if not isinstance(raw_resources, list) or not raw_resources:
        raise Error("resources must be non-empty")
    resources_list = [validate_resource(item, normalized_source["mesh_digest_sha256"],
                                        f"resources[{index}]")
                      for index, item in enumerate(raw_resources)]
    if len({item["id"] for item in resources_list}) != len(resources_list):
        raise Error("resource ids must be unique")
    resources = {item["id"]: item for item in resources_list}
    raw_nodes = document.get("nodes")
    if not isinstance(raw_nodes, list) or not raw_nodes:
        raise Error("nodes must be non-empty")
    nodes_list = [validate_node(item, resources, f"nodes[{index}]")
                  for index, item in enumerate(raw_nodes)]
    if len({item["id"] for item in nodes_list}) != len(nodes_list):
        raise Error("node ids must be unique")
    nodes = {item["id"]: item for item in nodes_list}
    raw_connections = document.get("connections")
    if not isinstance(raw_connections, list):
        raise Error("connections must be an array")
    connections = []
    seen_inputs: set[tuple[str, str]] = set()
    for index, item in enumerate(raw_connections):
        field = f"connections[{index}]"
        if not isinstance(item, dict) or not isinstance(item.get("from"), dict) or not isinstance(item.get("to"), dict):
            raise Error(f"{field} must contain from/to ports")
        source_node = need_id(item["from"].get("node"), f"{field}.from.node")
        source_port = need_id(item["from"].get("port"), f"{field}.from.port")
        target_node = need_id(item["to"].get("node"), f"{field}.to.node")
        target_port = need_id(item["to"].get("port"), f"{field}.to.port")
        if source_node not in nodes or target_node not in nodes:
            raise Error(f"{field} references unknown node")
        source_def = next((port for port in nodes[source_node]["outputs"] if port["name"] == source_port), None)
        target_def = next((port for port in nodes[target_node]["inputs"] if port["name"] == target_port), None)
        if source_def is None or target_def is None:
            raise Error(f"{field} references unknown port")
        if not set(source_def["domains"]).intersection(target_def["domains"]):
            raise Error(f"{field} has incompatible output domains")
        key = (target_node, target_port)
        if key in seen_inputs:
            raise Error(f"{field} duplicates a target input")
        seen_inputs.add(key)
        connections.append({"from": {"node": source_node, "port": source_port},
                            "to": {"node": target_node, "port": target_port}})
    for node in nodes_list:
        for input_port in node["inputs"]:
            if (node["id"], input_port["name"]) not in seen_inputs:
                raise Error(f"nodes.{node['id']}.{input_port['name']} is unconnected")
    adjacency: dict[str, list[str]] = defaultdict(list)
    indegree = {node_id: 0 for node_id in nodes}
    for connection in connections:
        a, b = connection["from"]["node"], connection["to"]["node"]
        adjacency[a].append(b)
        indegree[b] += 1
    queue = deque(sorted(node_id for node_id, degree in indegree.items() if degree == 0))
    ordered_nodes = []
    while queue:
        node_id = queue.popleft()
        ordered_nodes.append(node_id)
        for child in sorted(adjacency[node_id]):
            indegree[child] -= 1
            if indegree[child] == 0:
                queue.append(child)
    if len(ordered_nodes) != len(nodes):
        raise Error("node graph contains a cycle")
    if not any(node["kind"] == "consumer" for node in nodes_list):
        raise Error("document needs at least one consumer")
    return {
        "schema": SCHEMA, "schema_version": VERSION,
        "document_id": need_id(document.get("document_id"), "document_id"),
        "source": normalized_source,
        "resources": sorted(resources_list, key=lambda item: item["id"]),
        "nodes": sorted(nodes_list, key=lambda item: item["id"]),
        "connections": sorted(connections, key=lambda item: (
            item["from"]["node"], item["from"]["port"],
            item["to"]["node"], item["to"]["port"])),
        "topological_node_ids": ordered_nodes,
    }


def compile_plan(document: object) -> dict[str, Any]:
    document = validate(document)
    resources = {item["id"]: item for item in document["resources"]}
    adapters = []
    for node in document["nodes"]:
        if node["kind"] != "consumer":
            continue
        incoming = next(connection for connection in document["connections"]
                        if connection["to"]["node"] == node["id"])
        # Selector carriers are owned by their upstream selector node.  This
        # keeps a single edit-node-resource operation authoritative.
        if node["adapter"] == "selector_carrier_binding":
            source_node = document["nodes"]
            source_node = next(item for item in source_node
                               if item["id"] == incoming["from"]["node"])
            resource = resources[source_node["resource"]]
        else:
            resource = resources[node["resource"]]
        adapter = {
            "consumer_id": node["id"], "adapter": node["adapter"],
            "domain": node.get("domain", node["inputs"][0]["domains"][0]),
            "input": incoming["from"],
            "resource": resource,
            "execution": "request_only_existing_family_compiler",
        }
        if "feature_ids" in node:
            adapter["feature_ids"] = node["feature_ids"]
        for field in ("root_policy", "clearance_factor", "material_resource", "groom"):
            if field in node:
                adapter[field] = node[field]
        adapters.append(adapter)
    return {
        "schema": "ray_tracing.surface_authoring_document_compile_plan",
        "schema_version": 2,
        "document_digest_sha256": digest(document),
        "source": document["source"],
        "topological_node_ids": document["topological_node_ids"],
        "adapters": adapters,
        "geometry_mutation": "request_only_no_mesh_mutation",
        "scene_promotion": "forbidden",
    }


def readback(document: object) -> dict[str, Any]:
    document = validate(document)
    return {
        "schema": "ray_tracing.surface_authoring_document_readback",
        "schema_version": 2,
        "document_digest_sha256": digest(document),
        "document_id": document["document_id"], "source": document["source"],
        "resources": document["resources"], "nodes": document["nodes"],
        "connections": document["connections"], "compile_plan": compile_plan(document),
    }


def stale_readback(document: object, observed: object) -> dict[str, Any]:
    document = validate(document)
    if not isinstance(observed, dict):
        raise Error("observed_resources must be an object")
    resources = {item["id"]: item for item in document["resources"]}
    if set(observed) - set(resources):
        raise Error("observed_resources has unknown resource")
    stale = sorted(resource_id for resource_id, actual in observed.items()
                   if not isinstance(actual, str) or actual != resources[resource_id]["digest_sha256"])
    resource_nodes = {node["id"] for node in document["nodes"]
                      if node.get("resource") in stale}
    forward: dict[str, set[str]] = defaultdict(set)
    for connection in document["connections"]:
        forward[connection["from"]["node"]].add(connection["to"]["node"])
    invalidated = set(resource_nodes)
    queue = deque(resource_nodes)
    while queue:
        node = queue.popleft()
        for child in forward[node]:
            if child not in invalidated:
                invalidated.add(child)
                queue.append(child)
    consumers = sorted(node["id"] for node in document["nodes"]
                       if node["kind"] == "consumer" and node["id"] in invalidated)
    return {"schema": "ray_tracing.surface_authoring_document_staleness_readback",
            "schema_version": 2, "document_digest_sha256": digest(document),
            "stale_resource_ids": stale, "invalidated_consumer_ids": consumers,
            "valid": not stale}


def atomic(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", dir=path.parent, delete=False) as handle:
        handle.write(text + "\n")
        temporary = Path(handle.name)
    temporary.replace(path)


def edit_resource(document: object, resource_id: str, resource_digest: str,
                  expected_document_digest: str) -> tuple[dict[str, Any], str]:
    document = validate(document)
    if digest(document) != expected_document_digest:
        raise Error("expected_document_digest is stale")
    resource_id = need_id(resource_id, "resource_id")
    resource_digest = need_hex(resource_digest, "resource_digest")
    candidate = json.loads(canonical(document))
    resource = next((item for item in candidate["resources"]
                     if item["id"] == resource_id), None)
    if resource is None:
        raise Error("resource_id is unknown")
    resource["digest_sha256"] = resource_digest
    return validate(candidate), digest(document)


def edit_node_resource(document: object, node_id: str, resource_id: str,
                       expected_document_digest: str) -> tuple[dict[str, Any], str]:
    document = validate(document)
    if digest(document) != expected_document_digest:
        raise Error("expected_document_digest is stale")
    node_id = need_id(node_id, "node_id")
    resource_id = need_id(resource_id, "resource_id")
    candidate = json.loads(canonical(document))
    node = next((item for item in candidate["nodes"] if item["id"] == node_id), None)
    if node is None or "resource" not in node:
        raise Error("node_id has no editable resource")
    if resource_id not in {item["id"] for item in candidate["resources"]}:
        raise Error("resource_id is unknown")
    node["resource"] = resource_id
    return validate(candidate), digest(document)


def edit_curve_groom(document: object, node_id: str, groom: object,
                     expected_document_digest: str) -> tuple[dict[str, Any], str]:
    document = validate(document)
    if digest(document) != expected_document_digest:
        raise Error("expected_document_digest is stale")
    node_id = need_id(node_id, "node_id")
    candidate = json.loads(canonical(document))
    node = next((item for item in candidate["nodes"] if item["id"] == node_id), None)
    if node is None or node.get("adapter") != "curve_groom_compile_request":
        raise Error("node_id is not a curve_groom_compile_request")
    node["groom"] = validate_groom(groom, "groom")
    return validate(candidate), digest(document)


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="op", required=True)
    for name in ("inspect", "compile", "readback"):
        sub.add_parser(name).add_argument("--input", type=Path, required=True)
    create = sub.add_parser("create")
    create.add_argument("--input", type=Path, required=True)
    create.add_argument("--output", type=Path, required=True)
    edit = sub.add_parser("edit-resource")
    edit.add_argument("--input", type=Path, required=True)
    edit.add_argument("--output", type=Path, required=True)
    edit.add_argument("--resource-id", required=True)
    edit.add_argument("--resource-digest", required=True)
    edit.add_argument("--expected-document-digest", required=True)
    rewire = sub.add_parser("edit-node-resource")
    rewire.add_argument("--input", type=Path, required=True)
    rewire.add_argument("--output", type=Path, required=True)
    rewire.add_argument("--node-id", required=True)
    rewire.add_argument("--resource-id", required=True)
    rewire.add_argument("--expected-document-digest", required=True)
    groom = sub.add_parser("edit-curve-groom")
    groom.add_argument("--input", type=Path, required=True)
    groom.add_argument("--output", type=Path, required=True)
    groom.add_argument("--node-id", required=True)
    groom.add_argument("--groom-json", required=True)
    groom.add_argument("--expected-document-digest", required=True)
    stale = sub.add_parser("check-staleness")
    stale.add_argument("--input", type=Path, required=True)
    stale.add_argument("--observed-resources", required=True)
    args = parser.parse_args()
    try:
        document = validate(json.loads(args.input.read_text(encoding="utf-8")))
        if args.op == "create":
            atomic(args.output, canonical(document))
            result = {"status": "ok", "readback": readback(document)}
        elif args.op == "edit-resource":
            edited, undo = edit_resource(document, args.resource_id,
                                         args.resource_digest,
                                         args.expected_document_digest)
            atomic(args.output, canonical(edited))
            result = {"status": "ok", "undo_document_digest_sha256": undo,
                      "readback": readback(edited)}
        elif args.op == "edit-node-resource":
            edited, undo = edit_node_resource(document, args.node_id,
                                               args.resource_id,
                                               args.expected_document_digest)
            atomic(args.output, canonical(edited))
            result = {"status": "ok", "undo_document_digest_sha256": undo,
                      "readback": readback(edited)}
        elif args.op == "edit-curve-groom":
            edited, undo = edit_curve_groom(document, args.node_id,
                                             json.loads(args.groom_json),
                                             args.expected_document_digest)
            atomic(args.output, canonical(edited))
            result = {"status": "ok", "undo_document_digest_sha256": undo,
                      "readback": readback(edited)}
        elif args.op == "check-staleness":
            result = {"status": "ok", "staleness": stale_readback(
                document, json.loads(args.observed_resources))}
        elif args.op == "compile":
            result = {"status": "ok", "compile_plan": compile_plan(document)}
        else:
            result = {"status": "ok", "readback": readback(document)}
        print(json.dumps(result, sort_keys=True))
        return 0
    except (Error, json.JSONDecodeError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
