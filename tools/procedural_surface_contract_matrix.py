#!/usr/bin/env python3
"""Plan and execute digest-bound surface-authoring contract matrices.

This is an AI-facing planning and readback layer.  It does not evaluate a
surface graph, invoke family compilers, mutate a source mesh, or promote a
scene. Existing typed family compilers remain the authority for generated
artifacts and their receipts. The optional executor supports only the existing
material and shading-normal microdetail proof adapters.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
from typing import Any


SCHEMA = "ray_tracing.surface_authoring_contract_matrix"
SCHEMA_VERSION = 1
DOCUMENT_SCHEMA = "ray_tracing.surface_authoring_document"
HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")

DOMAIN_BITS = {
    "material": 1,
    "microdetail_normal": 2,
    "signed_relief": 4,
    "deep_inset": 8,
    "attached_asset": 16,
}

SLOT_DOMAINS = {
    "material_graph": {"material", "microdetail_normal"},
    "surface_field_graph": {"microdetail_normal", "signed_relief", "deep_inset"},
    "face_region_selector": set(DOMAIN_BITS),
    "attachments": {"attached_asset"},
}

PROFILE_GATES = {
    "material": {
        "invariants": ["source_topology_unchanged", "source_silhouette_unchanged"],
        "views": ["beauty", "raw_mask", "layer_weights", "provenance"],
    },
    "microdetail": {
        "invariants": ["source_topology_unchanged", "source_silhouette_unchanged", "hit_coverage_unchanged"],
        "views": ["beauty", "geometric_normal", "shading_normal", "normal_difference"],
    },
    "selector": {
        "invariants": ["selected_region_explicit", "unselected_regions_unchanged"],
        "views": ["selector_mask", "selector_provenance"],
    },
    "signed_relief": {
        "invariants": ["source_mesh_immutable", "distinct_derived_shell_required", "closed_shell_required"],
        "views": ["control", "beauty", "grazing", "depth_delta", "topology", "provenance"],
    },
    "deep_inset": {
        "invariants": ["source_mesh_immutable", "distinct_derived_shell_required", "retained_wall_floor_roles_required"],
        "views": ["control", "beauty", "grazing", "depth_delta", "topology_roles", "provenance"],
    },
    "attachment": {
        "invariants": ["source_mesh_immutable", "separate_attached_asset_required", "boolean_union_forbidden"],
        "views": ["control", "beauty", "grazing", "attachment_base", "clearance", "provenance"],
    },
}

# Named adapters are deliberately closed rather than supplied by matrix JSON:
# a document cannot turn this AI-facing tool into an arbitrary command runner.
EXECUTION_ADAPTERS = {
    "material": {
        "id": "psg16b_material_visual_proof",
        "script": "procedural_solid_material_graph_visual_proof.py",
        "report": "review/psg16b_visual_proof.json",
        "schema": "ray_tracing.procedural_solid_psg16b_visual_proof",
    },
    "microdetail": {
        "id": "psg17_microdetail_visual_proof",
        "script": "procedural_solid_microdetail_normal_visual_proof.py",
        "report": "review/psg17_visual_proof.json",
        "schema": "ray_tracing.procedural_solid_psg17_visual_proof",
    },
}

COMPOSITION_ADAPTER = {
    "id": "psg16b_psg17_material_microdetail_visual_proof",
    "script": "procedural_surface_material_microdetail_visual_proof.py",
    "report": "review/material_microdetail_visual_proof.json",
    "schema": "ray_tracing.surface_authoring_material_microdetail_visual_proof",
}

SELECTOR_COMPOSITION_ADAPTER = {
    "id": "psg24_selector_material_microdetail_visual_proof",
    "script": "procedural_surface_selector_material_microdetail_visual_proof.py",
    "report": "review/selector_material_microdetail_visual_proof.json",
    "schema": "ray_tracing.surface_authoring_selector_material_microdetail_visual_proof",
}


class MatrixError(ValueError):
    pass


def canonical(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value) + b"\n")


def require_id(value: object, field: str) -> str:
    if not isinstance(value, str) or not value or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-" for c in value):
        raise MatrixError(f"{field} must be a non-empty identifier")
    return value


def require_digest(value: object, field: str) -> str:
    if not isinstance(value, str) or not HEX64.fullmatch(value):
        raise MatrixError(f"{field} must be a SHA-256 hex digest")
    return value.lower()


def require_profiles(value: object, field: str) -> list[str]:
    if value is None:
        return []
    if not isinstance(value, list) or any(not isinstance(item, str) or item not in PROFILE_GATES for item in value):
        raise MatrixError(f"{field} contains an unsupported proof profile")
    return list(dict.fromkeys(value))


def validate_reference(name: str, value: object) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise MatrixError(f"references.{name} must be an object")
    domains = value.get("output_domains")
    if not isinstance(domains, list) or not domains or any(item not in DOMAIN_BITS for item in domains):
        raise MatrixError(f"references.{name}.output_domains is invalid")
    execution = value.get("execution")
    if execution is not None:
        if not isinstance(execution, dict):
            raise MatrixError(f"references.{name}.execution must be an object")
        adapter_ids = {adapter["id"] for adapter in EXECUTION_ADAPTERS.values()}
        if execution.get("adapter") not in adapter_ids:
            raise MatrixError(f"references.{name}.execution.adapter is unsupported")
        if execution.get("family_id") is not None:
            require_id(execution["family_id"], f"references.{name}.execution.family_id")
    return {
        "id": require_id(value.get("id"), f"references.{name}.id"),
        "digest_sha256": require_digest(value.get("digest_sha256"), f"references.{name}.digest_sha256"),
        "output_domains": list(dict.fromkeys(domains)),
        "proof_profiles": require_profiles(value.get("proof_profiles"), f"references.{name}.proof_profiles"),
        "receipt_refs": value.get("receipt_refs", []),
        "execution": execution,
    }


def validate_matrix(value: object) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise MatrixError("matrix root must be an object")
    if value.get("schema") != SCHEMA or value.get("schema_version") != SCHEMA_VERSION:
        raise MatrixError("unsupported matrix schema/version")
    source = value.get("source")
    if not isinstance(source, dict):
        raise MatrixError("source is required")
    normalized_source = {
        "object_id": require_id(source.get("object_id"), "source.object_id"),
        "mesh_digest_sha256": require_digest(source.get("mesh_digest_sha256"), "source.mesh_digest_sha256"),
        "source_kind": require_id(source.get("source_kind"), "source.source_kind"),
        "topology": source.get("topology", {}),
    }
    if not isinstance(normalized_source["topology"], dict):
        raise MatrixError("source.topology must be an object")
    raw_references = value.get("references")
    if not isinstance(raw_references, dict) or not raw_references:
        raise MatrixError("references must be a non-empty object")
    references = {name: validate_reference(name, entry) for name, entry in raw_references.items()}
    raw_cells = value.get("cells")
    if not isinstance(raw_cells, list) or not raw_cells:
        raise MatrixError("cells must be a non-empty array")
    cells: list[dict[str, Any]] = []
    seen: set[str] = set()
    for raw in raw_cells:
        if not isinstance(raw, dict):
            raise MatrixError("cell must be an object")
        cell_id = require_id(raw.get("id"), "cells[].id")
        if cell_id in seen:
            raise MatrixError(f"duplicate cell id: {cell_id}")
        seen.add(cell_id)
        bindings = raw.get("bindings", {})
        if not isinstance(bindings, dict):
            raise MatrixError(f"cells.{cell_id}.bindings must be an object")
        normalized_bindings: dict[str, Any] = {}
        for slot in ("material_graph", "surface_field_graph", "face_region_selector"):
            ref_name = bindings.get(slot)
            if ref_name is None:
                continue
            if not isinstance(ref_name, str) or ref_name not in references:
                raise MatrixError(f"cells.{cell_id}.{slot} references an unknown input")
            if not set(references[ref_name]["output_domains"]).issubset(SLOT_DOMAINS[slot]):
                raise MatrixError(f"cells.{cell_id}.{slot} uses incompatible output domains")
            normalized_bindings[slot] = ref_name
        raw_attachments = bindings.get("attachments", [])
        if not isinstance(raw_attachments, list):
            raise MatrixError(f"cells.{cell_id}.attachments must be an array")
        attachments: list[str] = []
        for ref_name in raw_attachments:
            if not isinstance(ref_name, str) or ref_name not in references:
                raise MatrixError(f"cells.{cell_id}.attachments references an unknown input")
            if not set(references[ref_name]["output_domains"]).issubset(SLOT_DOMAINS["attachments"]):
                raise MatrixError(f"cells.{cell_id}.attachments uses incompatible output domains")
            if ref_name in attachments:
                raise MatrixError(f"cells.{cell_id}.attachments contains a duplicate input")
            attachments.append(ref_name)
        if attachments:
            normalized_bindings["attachments"] = attachments
        explicit_profiles = require_profiles(raw.get("proof_profiles"), f"cells.{cell_id}.proof_profiles")
        cells.append({
            "id": cell_id,
            "bindings": normalized_bindings,
            "proof_profiles": explicit_profiles,
            "extensions": raw.get("extensions", {}),
        })
    return {
        "schema": SCHEMA,
        "schema_version": SCHEMA_VERSION,
        "matrix_id": require_id(value.get("matrix_id"), "matrix_id"),
        "source": normalized_source,
        "references": references,
        "cells": cells,
        "extensions": value.get("extensions", {}),
    }


def document_for_cell(matrix: dict[str, Any], cell: dict[str, Any]) -> dict[str, Any] | None:
    bindings = cell["bindings"]
    if not bindings:
        return None
    references = matrix["references"]

    document_id = f"{matrix['matrix_id']}.{cell['id']}"
    # The v1 C document contract reserves 64 bytes including its terminator.
    # Preserve human-readable IDs when possible, but make long matrix IDs
    # deterministic and valid instead of emitting JSON that readback rejects.
    if len(document_id) >= 64:
        document_id = "matrix." + hashlib.sha256(document_id.encode("utf-8")).hexdigest()[:56]

    def ref(name: str | None) -> dict[str, Any] | None:
        if name is None:
            return None
        item = references[name]
        return {"id": item["id"], "digest_sha256": item["digest_sha256"],
                "output_domains": sum(DOMAIN_BITS[domain] for domain in item["output_domains"])}

    return {
        "schema": DOCUMENT_SCHEMA,
        "schema_version": 1,
        "document_id": document_id,
        "source_object_id": matrix["source"]["object_id"],
        "source_mesh_digest_sha256": matrix["source"]["mesh_digest_sha256"],
        "material_graph": ref(bindings.get("material_graph")),
        "surface_field_graph": ref(bindings.get("surface_field_graph")),
        "face_region_selector": ref(bindings.get("face_region_selector")),
        "attachments": [ref(name) for name in bindings.get("attachments", [])],
    }


def profiles_for_cell(matrix: dict[str, Any], cell: dict[str, Any]) -> list[str]:
    profiles = list(cell["proof_profiles"])
    for ref_name in cell["bindings"].values():
        names = ref_name if isinstance(ref_name, list) else [ref_name]
        for name in names:
            profiles.extend(matrix["references"][name]["proof_profiles"])
    return list(dict.fromkeys(profiles))


def proof_request(matrix: dict[str, Any], cell: dict[str, Any], profiles: list[str]) -> dict[str, Any]:
    invariants: list[str] = ["exact_source_mesh_digest_bound"]
    views: list[str] = ["source_control"]
    for profile in profiles:
        invariants.extend(PROFILE_GATES[profile]["invariants"])
        views.extend(PROFILE_GATES[profile]["views"])
    return {
        "schema": "ray_tracing.surface_authoring_proof_request",
        "schema_version": 1,
        "matrix_id": matrix["matrix_id"],
        "cell_id": cell["id"],
        "source": matrix["source"],
        "proof_profiles": profiles,
        "required_invariants": list(dict.fromkeys(invariants)),
        "required_views": list(dict.fromkeys(views)),
        "scene_promotion": "forbidden",
        "interpretation": "request_only_not_visual_acceptance",
    }


def execute_profile(matrix: dict[str, Any], output_root: Path, profile: str,
                    supplied_receipt: Path | None = None) -> dict[str, Any]:
    """Run one closed typed adapter and bind its report to the matrix source."""
    adapter = EXECUTION_ADAPTERS[profile]
    references = [
        {"name": name, **reference["execution"]}
        for name, reference in matrix["references"].items()
        if (reference.get("execution") or {}).get("adapter") == adapter["id"]
    ]
    if not references:
        raise MatrixError(f"profile {profile} has no reference bound to {adapter['id']}")
    if profile == "material" and any(not item.get("family_id") for item in references):
        raise MatrixError("material execution references require execution.family_id")
    if supplied_receipt is None:
        execution_root = output_root / "executions" / profile
        result = subprocess.run(
            ["python3", str(Path(__file__).with_name(adapter["script"])),
             "--output-root", str(execution_root)],
            text=True, capture_output=True)
        if result.returncode:
            raise MatrixError(f"{adapter['id']} failed: {result.stderr.strip() or result.stdout.strip()}")
        report_path = execution_root / adapter["report"]
        execution_source = "executed"
    else:
        report_path = supplied_receipt
        execution_source = "supplied_receipt"
    if not report_path.is_file():
        raise MatrixError(f"{adapter['id']} did not write its proof receipt")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("schema") != adapter["schema"] or report.get("status") != "passed":
        raise MatrixError(f"{adapter['id']} proof receipt did not pass")
    bindings = []
    for reference in references:
        if profile == "material":
            family = next((item for item in report.get("families", [])
                           if item.get("family") == reference["family_id"]), None)
            if family is None:
                raise MatrixError(f"material proof report has no family: {reference['family_id']}")
            source_digest = require_digest(family.get("derived_mesh_digest_sha256"),
                                           "material report mesh digest")
        else:
            source_digest = require_digest(report.get("mesh_digest_sha256"),
                                           "microdetail report mesh digest")
        if source_digest != matrix["source"]["mesh_digest_sha256"]:
            raise MatrixError(
                f"{profile} receipt source digest does not match matrix source for {reference['name']}")
        bindings.append({"reference": reference["name"],
                         "family_id": reference.get("family_id"),
                         "source_mesh_digest_sha256": source_digest})
    receipt_path = output_root / "receipts" / f"{profile}.proof.json"
    write_json(receipt_path, report)
    return {
        "profile": profile,
        "adapter": adapter["id"],
        "proof_receipt": str(receipt_path.relative_to(output_root)),
        "proof_receipt_digest_sha256": hashlib.sha256(canonical(report)).hexdigest(),
        "bindings": bindings,
        "execution_source": execution_source,
        "status": "passed",
    }


def execute_composition(matrix: dict[str, Any], output_root: Path,
                        supplied_receipt: Path | None = None) -> dict[str, Any]:
    """Prove that the composed cell has both requested lanes active together."""
    cell = next((item for item in matrix["cells"]
                 if item["bindings"].get("material_graph")
                 and item["bindings"].get("surface_field_graph")
                 and item["extensions"].get("execution", {}).get("adapter") == COMPOSITION_ADAPTER["id"]), None)
    if cell is None:
        raise MatrixError("matrix has no material+microdetail cell bound to the composition adapter")
    if supplied_receipt is None:
        execution_root = output_root / "executions" / "material_microdetail"
        result = subprocess.run(
            ["python3", str(Path(__file__).with_name(COMPOSITION_ADAPTER["script"])),
             "--output-root", str(execution_root)], text=True, capture_output=True)
        if result.returncode:
            raise MatrixError("material+microdetail composition proof failed: "
                              f"{result.stderr.strip() or result.stdout.strip()}")
        report_path = execution_root / COMPOSITION_ADAPTER["report"]
        execution_source = "executed"
    else:
        report_path = supplied_receipt
        execution_source = "supplied_receipt"
    if not report_path.is_file():
        raise MatrixError("composition adapter did not write its proof receipt")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("schema") != COMPOSITION_ADAPTER["schema"] or report.get("status") != "passed":
        raise MatrixError("composition proof receipt did not pass")
    source_digest = require_digest(report.get("mesh_digest_sha256"), "composition report mesh digest")
    if source_digest != matrix["source"]["mesh_digest_sha256"]:
        raise MatrixError("composition receipt source digest does not match matrix source")
    metrics = report.get("metrics", {})
    for key in ("control_to_material", "control_to_microdetail",
                "material_to_combined", "microdetail_to_combined"):
        if not isinstance(metrics.get(key), dict) or metrics[key].get("changed_pixels", 0) <= 0:
            raise MatrixError(f"composition receipt has no active signal for {key}")
    if metrics.get("combined_repeat", {}).get("changed_pixels") != 0:
        raise MatrixError("composition receipt combined repeat is not deterministic")
    receipt_path = output_root / "receipts" / "material_microdetail.composition.proof.json"
    write_json(receipt_path, report)
    return {"cell_id": cell["id"], "adapter": COMPOSITION_ADAPTER["id"],
            "proof_receipt": str(receipt_path.relative_to(output_root)),
            "proof_receipt_digest_sha256": hashlib.sha256(canonical(report)).hexdigest(),
            "source_mesh_digest_sha256": source_digest,
            "execution_source": execution_source, "status": "passed"}


def execute_selector_composition(matrix: dict[str, Any], output_root: Path,
                                 supplied_receipt: Path | None = None) -> dict[str, Any]:
    """Bind one real selected-region material+microdetail proof to its cell."""
    cell = next((item for item in matrix["cells"]
                 if item["bindings"].get("material_graph")
                 and item["bindings"].get("surface_field_graph")
                 and item["bindings"].get("face_region_selector")
                 and item["extensions"].get("execution", {}).get("adapter") ==
                 SELECTOR_COMPOSITION_ADAPTER["id"]), None)
    if cell is None:
        raise MatrixError("matrix has no selector+material+microdetail composition cell")
    if supplied_receipt is None:
        execution_root = output_root / "executions" / "selector_material_microdetail"
        result = subprocess.run(
            ["python3", str(Path(__file__).with_name(SELECTOR_COMPOSITION_ADAPTER["script"])),
             "--output-root", str(execution_root)], text=True, capture_output=True)
        if result.returncode:
            raise MatrixError("selector composition proof failed: " +
                              f"{result.stderr.strip() or result.stdout.strip()}")
        report_path = execution_root / SELECTOR_COMPOSITION_ADAPTER["report"]
        execution_source = "executed"
    else:
        report_path = supplied_receipt
        execution_source = "supplied_receipt"
    if not report_path.is_file():
        raise MatrixError("selector composition adapter did not write its proof receipt")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if (report.get("schema") != SELECTOR_COMPOSITION_ADAPTER["schema"] or
            report.get("status") != "passed"):
        raise MatrixError("selector composition proof receipt did not pass")
    source_digest = require_digest(report.get("mesh_digest_sha256"), "selector composition report mesh digest")
    if source_digest != matrix["source"]["mesh_digest_sha256"]:
        raise MatrixError("selector composition receipt source digest does not match matrix source")
    selector = report.get("selector", {}).get("receipt", {})
    if selector.get("selected_feature_count", 0) <= 0 or selector.get("transition_vertex_count", 0) <= 0:
        raise MatrixError("selector composition receipt has no selected transition carrier")
    metrics = report.get("metrics", {})
    for key in ("control_to_selected_material_microdetail", "control_to_selector_mask"):
        if not isinstance(metrics.get(key), dict) or metrics[key].get("changed_pixels", 0) <= 0:
            raise MatrixError(f"selector composition receipt has no active signal for {key}")
    if metrics.get("selected_repeat", {}).get("changed_pixels") != 0:
        raise MatrixError("selector composition receipt selected repeat is not deterministic")
    receipt_path = output_root / "receipts" / "selector_material_microdetail.composition.proof.json"
    write_json(receipt_path, report)
    return {"cell_id": cell["id"], "adapter": SELECTOR_COMPOSITION_ADAPTER["id"],
            "proof_receipt": str(receipt_path.relative_to(output_root)),
            "proof_receipt_digest_sha256": hashlib.sha256(canonical(report)).hexdigest(),
            "source_mesh_digest_sha256": source_digest,
            "execution_source": execution_source, "status": "passed"}


def run_json(command: list[str]) -> dict[str, Any]:
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise MatrixError(f"document adapter failed: {result.stderr.strip() or result.stdout.strip()}")
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise MatrixError("document adapter did not return JSON") from error


def plan(matrix: dict[str, Any], output_root: Path, document_tool: Path | None,
         execute_profiles: list[str], supplied_receipts: dict[str, Path],
         execute_composed: bool, supplied_composition_receipt: Path | None,
         execute_selector_composed: bool, supplied_selector_composition_receipt: Path | None) -> dict[str, Any]:
    cells: list[dict[str, Any]] = []
    visual_tool = Path(__file__).with_name("procedural_surface_authoring_canvas_visual.py")
    for cell in matrix["cells"]:
        profiles = profiles_for_cell(matrix, cell)
        request = proof_request(matrix, cell, profiles)
        request_path = output_root / "proof_requests" / f"{cell['id']}.json"
        write_json(request_path, request)
        result: dict[str, Any] = {
            "id": cell["id"],
            "proof_request": str(request_path.relative_to(output_root)),
            "proof_profiles": profiles,
            "bindings": cell["bindings"],
            "extensions": cell["extensions"],
        }
        document = document_for_cell(matrix, cell)
        if document is not None:
            document_path = output_root / "documents" / f"{cell['id']}.json"
            write_json(document_path, document)
            result["document"] = str(document_path.relative_to(output_root))
            if document_tool is not None:
                readback = run_json([str(document_tool), "inspect", "--input", str(document_path)])
                readback_path = output_root / "readback" / f"{cell['id']}.json"
                write_json(readback_path, readback)
                result["readback"] = str(readback_path.relative_to(output_root))
                result["document_digest_sha256"] = readback["compile_plan"]["document_digest_sha256"]
                canvas = run_json([str(document_tool), "canvas", "--input", str(document_path)])
                canvas_path = output_root / "canvases" / f"{cell['id']}.json"
                write_json(canvas_path, canvas)
                result["canvas"] = str(canvas_path.relative_to(output_root))
                if visual_tool.is_file():
                    svg_path = output_root / "visuals" / f"{cell['id']}.svg"
                    summary_path = output_root / "visuals" / f"{cell['id']}.summary.json"
                    render = subprocess.run(["python3", str(visual_tool), "--canvas", str(canvas_path),
                                             "--output-svg", str(svg_path), "--output-summary", str(summary_path)],
                                            text=True, capture_output=True)
                    if render.returncode != 0:
                        raise MatrixError(f"canvas visual renderer failed: {render.stderr.strip()}")
                    result["canvas_visual"] = str(svg_path.relative_to(output_root))
        cells.append(result)
    executions = [execute_profile(matrix, output_root, profile, supplied_receipts.get(profile))
                  for profile in execute_profiles]
    composition = (execute_composition(matrix, output_root, supplied_composition_receipt)
                   if execute_composed else None)
    selector_composition = (execute_selector_composition(matrix, output_root, supplied_selector_composition_receipt)
                            if execute_selector_composed else None)
    return {
        "schema": "ray_tracing.surface_authoring_contract_matrix_receipt",
        "schema_version": 1,
        "matrix_id": matrix["matrix_id"],
        "matrix_digest_sha256": hashlib.sha256(canonical(matrix)).hexdigest(),
        "execution_mode": ("typed_selector_material_microdetail_composed_proof" if selector_composition else
                           ("typed_material_microdetail_composed_proof" if composition else
                           ("typed_material_microdetail_proof" if executions else
                           ("document_readback" if document_tool is not None else "plan_only")))),
        "source": matrix["source"],
        "cell_count": len(cells),
        "cells": cells,
        "family_compiler_execution": executions or "deferred_to_typed_receipt_bound_adapters",
        "composition_execution": composition,
        "selector_composition_execution": selector_composition,
        "scene_promotion": "forbidden",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--document-tool", type=Path,
                        help="Optional procedural_surface_authoring_document_tool for document/canvas readback.")
    parser.add_argument("--execute-profile", action="append", default=[],
                        choices=sorted(EXECUTION_ADAPTERS),
                        help="Execute an established receipt-bound material or microdetail adapter.")
    parser.add_argument("--proof-receipt", action="append", default=[], metavar="PROFILE=PATH",
                        help="Reuse a passed typed proof receipt for an executed profile instead of rerunning it.")
    parser.add_argument("--execute-material-microdetail", action="store_true",
                        help="Execute the declared composed material+microdetail proof adapter.")
    parser.add_argument("--material-microdetail-proof-receipt", type=Path,
                        help="Reuse a passed composed material+microdetail proof receipt.")
    parser.add_argument("--execute-selector-material-microdetail", action="store_true",
                        help="Execute the declared selected-region material+microdetail proof adapter.")
    parser.add_argument("--selector-material-microdetail-proof-receipt", type=Path,
                        help="Reuse a passed selected-region material+microdetail proof receipt.")
    args = parser.parse_args()
    try:
        matrix = validate_matrix(json.loads(args.matrix.read_text(encoding="utf-8")))
        if args.document_tool and not args.document_tool.is_file():
            raise MatrixError(f"document tool does not exist: {args.document_tool}")
        supplied_receipts: dict[str, Path] = {}
        for entry in args.proof_receipt:
            profile, separator, path = entry.partition("=")
            if not separator or profile not in EXECUTION_ADAPTERS or not path:
                raise MatrixError("--proof-receipt must be PROFILE=PATH for material or microdetail")
            receipt_path = Path(path)
            if not receipt_path.is_file():
                raise MatrixError(f"proof receipt does not exist: {receipt_path}")
            supplied_receipts[profile] = receipt_path
        execute_profiles = list(dict.fromkeys(args.execute_profile))
        if any(profile not in execute_profiles for profile in supplied_receipts):
            raise MatrixError("--proof-receipt profile must also be selected by --execute-profile")
        if args.material_microdetail_proof_receipt:
            if not args.execute_material_microdetail:
                raise MatrixError("--material-microdetail-proof-receipt requires --execute-material-microdetail")
            if not args.material_microdetail_proof_receipt.is_file():
                raise MatrixError("material+microdetail proof receipt does not exist")
        if args.selector_material_microdetail_proof_receipt:
            if not args.execute_selector_material_microdetail:
                raise MatrixError("--selector-material-microdetail-proof-receipt requires --execute-selector-material-microdetail")
            if not args.selector_material_microdetail_proof_receipt.is_file():
                raise MatrixError("selector material+microdetail proof receipt does not exist")
        receipt = plan(matrix, args.output_root, args.document_tool,
                       execute_profiles, supplied_receipts,
                       args.execute_material_microdetail,
                       args.material_microdetail_proof_receipt,
                       args.execute_selector_material_microdetail,
                       args.selector_material_microdetail_proof_receipt)
        write_json(args.output_root / "matrix_receipt.json", receipt)
        print(json.dumps({"status": "ok", "matrix_id": receipt["matrix_id"],
                          "cell_count": receipt["cell_count"],
                          "receipt": str(args.output_root / "matrix_receipt.json")}, sort_keys=True))
        return 0
    except (MatrixError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "error", "message": str(error)}, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
