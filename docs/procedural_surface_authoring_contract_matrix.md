# Surface Authoring Contract Matrix V1

`ray_tracing.surface_authoring_contract_matrix` is an AI-first planning and
readback format for testing surface-authoring lanes independently and in
declared compositions. It is not a graph evaluator, a mesh compiler, or a
saved-scene promotion path.

## Why a matrix exists

One adjective such as "weathered" can involve independent material,
microdetail, physical relief, inset, selector, and attachment claims. A matrix
keeps those claims separate: every cell binds one immutable source, named
digest-bound references, explicit proof profiles, expected invariants, and
required visual views.

The v1 planner reads the matrix, writes compatible
`ray_tracing.surface_authoring_document` JSON for non-control cells, obtains
optional adapter/canvas readback, and emits proof-request JSON. It does not
run a family compiler. A later executor must consume the named typed compiler
receipts and fulfill the generated proof requests before visual acceptance.

## Matrix shape

```json
{
  "schema": "ray_tracing.surface_authoring_contract_matrix",
  "schema_version": 1,
  "matrix_id": "example_v1",
  "source": {
    "object_id": "cube",
    "mesh_digest_sha256": "...",
    "source_kind": "semantic_cube",
    "topology": {"triangle_count": 12}
  },
  "references": {
    "material": {
      "id": "brown_material",
      "digest_sha256": "...",
      "output_domains": ["material"],
      "proof_profiles": ["material"]
    }
  },
  "cells": [
    {"id": "control", "bindings": {}},
    {"id": "material_only", "bindings": {"material_graph": "material"}}
  ]
}
```

References are reusable inputs. Cells bind them through the same four document
lanes: `material_graph`, `surface_field_graph`, `face_region_selector`, and
`attachments`. The planner validates output-domain compatibility before it
writes a document request.

## Proof profiles

The initial profiles are `material`, `microdetail`, `selector`,
`signed_relief`, `deep_inset`, and `attachment`. Each expands into lane-specific
invariants and headless-view requests. For example, material requires raw-mask
and provenance views while source topology/silhouette stay fixed; deep inset
requires immutable source plus retained/wall/floor topology evidence; an
attachment requires a separate asset and forbids a Boolean-union claim.

New families can be added without changing existing fixtures by adding a new
profile definition and references that declare it. The matrix preserves an
`extensions` object at both matrix and cell scope for future typed adapters.

## Run

```bash
python3 tools/procedural_surface_contract_matrix.py \
  --matrix tests/fixtures/procedural_surface_contract_matrix_v1/cube_lane_matrix.json \
  --document-tool build/toolchains/clang/arm64/tools/cli/procedural_surface_authoring_document_tool \
  --output-root build/agent_runs/surface_authoring_contract_matrix
```

The output contains source-preserving document requests, adapter/canvas
readback when the document tool is provided, deterministic SVG dependency
views, proof requests, and `matrix_receipt.json`. The SVG is a graph/readback
view only; family-appropriate headless renders satisfy visual proof later.

## Material and microdetail execution

The matrix has a deliberately closed executor for the established PSG-16B
material and PSG-17 shading-normal proof adapters:

```bash
python3 tools/procedural_surface_contract_matrix.py \
  --matrix tests/fixtures/procedural_surface_contract_matrix_v1/material_microdetail_mountain_execution.json \
  --execute-profile material --execute-profile microdetail \
  --output-root build/agent_runs/surface_authoring_contract_matrix/material_microdetail
```

Each executable reference names an adapter; material additionally names the
typed family it must bind. The executor runs the existing proof, requires a
passed receipt, verifies the realized source digest against the matrix source,
and retains a canonical receipt under the matrix output. The execution fixture
uses the shared `mountain_snow` source digest from the typed proof path.
For repeatable matrix readback without re-rendering an already-completed proof,
pass the same selected profile with `--proof-receipt PROFILE=/path/to/report.json`.
The report is still schema/status/source-digest checked and copied into the new
matrix receipt; a supplied receipt does not bypass any invariant binding.

This is material and shading-normal proof only. It does not claim displacement,
remeshing, or physical relief, and it does not promote a saved scene.

The corresponding long-running local gate is
`make test-procedural-surface-authoring-contract-matrix-material-microdetail`.
It is intentionally not part of the fast authoring-document contract target.

## Composed material and microdetail cell

`material_microdetail` now has its own typed proof: control, material only,
microdetail only, both active, and an exact repeat of the combined case. It
requires fixed source digest, triangle count, primary-hit coverage, and TLAS/
BLAS route across every view; both individual deltas into the combined view
must be nonzero, and the combined repeat must be byte-identical.

Run it with:

```bash
make test-procedural-surface-authoring-contract-matrix-material-microdetail-composed
```

This is still a material-plus-shading-normal proof. It is not a displacement,
relief, remesh, or saved-scene claim.
