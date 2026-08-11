#!/usr/bin/env python3
"""Compile no-render v2 surface-authoring contract matrices."""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import defaultdict, deque
from pathlib import Path
from typing import Any

import procedural_surface_authoring_document_v2 as document

SCHEMA = "ray_tracing.surface_authoring_document_v2_contract_matrix"


class MatrixError(ValueError):
    pass


def canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def write(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(canonical(value) + "\n", encoding="utf-8")


def subset(source: dict[str, Any], consumer_ids: list[str], cell_id: str) -> dict[str, Any]:
    nodes = {node["id"]: node for node in source["nodes"]}
    consumers = {node["id"] for node in source["nodes"] if node["kind"] == "consumer"}
    if not consumer_ids or any(not isinstance(item, str) or item not in consumers for item in consumer_ids):
        raise MatrixError(f"cells.{cell_id}.consumers must name consumers")
    reverse: dict[str, set[str]] = defaultdict(set)
    for connection in source["connections"]:
        reverse[connection["to"]["node"]].add(connection["from"]["node"])
    chosen = set(consumer_ids)
    queue = deque(consumer_ids)
    while queue:
        node_id = queue.popleft()
        for parent in reverse[node_id]:
            if parent not in chosen:
                chosen.add(parent)
                queue.append(parent)
    selected_nodes = [node for node in source["nodes"] if node["id"] in chosen]
    resources = {node["resource"] for node in selected_nodes if "resource" in node}
    resources.update(node["material_resource"] for node in selected_nodes
                     if "material_resource" in node)
    return {
        "schema": document.SCHEMA,
        "schema_version": document.VERSION,
        "document_id": f"{source['document_id']}.{cell_id}",
        "source": source["source"],
        "resources": [item for item in source["resources"] if item["id"] in resources],
        "nodes": selected_nodes,
        "connections": [item for item in source["connections"]
                        if item["from"]["node"] in chosen and item["to"]["node"] in chosen],
    }


def validate_matrix(value: Any, matrix_path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    if not isinstance(value, dict) or value.get("schema") != SCHEMA or value.get("schema_version") != 1:
        raise MatrixError("unsupported matrix schema/version")
    document_name = value.get("document")
    if not isinstance(document_name, str) or not document_name:
        raise MatrixError("document is required")
    document_path = matrix_path.parent / document_name
    source = document.validate(json.loads(document_path.read_text(encoding="utf-8")))
    raw_cells = value.get("cells")
    if not isinstance(raw_cells, list) or not raw_cells:
        raise MatrixError("cells must be non-empty")
    cells = []
    seen = set()
    for raw in raw_cells:
        if not isinstance(raw, dict) or not isinstance(raw.get("id"), str):
            raise MatrixError("cell id is required")
        cell_id = raw["id"]
        if cell_id in seen:
            raise MatrixError("cell ids must be unique")
        seen.add(cell_id)
        consumers = raw.get("consumers")
        if not isinstance(consumers, list):
            raise MatrixError(f"cells.{cell_id}.consumers must be an array")
        cells.append({"id": cell_id, "consumers": sorted(consumers)})
    return source, cells


def compile_matrix(matrix_path: Path) -> dict[str, Any]:
    raw = json.loads(matrix_path.read_text(encoding="utf-8"))
    source, cells = validate_matrix(raw, matrix_path)
    compiled = []
    for cell in cells:
        cell_document = document.validate(subset(source, cell["consumers"], cell["id"]))
        plan = document.compile_plan(cell_document)
        compiled.append({"id": cell["id"], "consumer_ids": cell["consumers"],
                         "document_digest_sha256": document.digest(cell_document),
                         "adapter_ids": [item["consumer_id"] for item in plan["adapters"]],
                         "adapters": plan["adapters"]})
    receipt = {"schema": "ray_tracing.surface_authoring_document_v2_matrix_receipt",
               "schema_version": 1,
               "matrix_digest_sha256": hashlib.sha256(canonical(raw).encode("utf-8")).hexdigest(),
               "source_document_digest_sha256": document.digest(source),
               "geometry_mutation": "forbidden",
               "scene_promotion": "forbidden",
               "cells": compiled}
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        receipt = compile_matrix(args.matrix)
        if args.output:
            write(args.output, receipt)
        print(json.dumps({"status": "ok", "receipt": receipt}, sort_keys=True))
        return 0
    except (MatrixError, document.Error, json.JSONDecodeError, OSError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
