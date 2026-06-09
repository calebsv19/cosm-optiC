# core_mesh_compile

Shared compile-boundary helpers for the mesh-asset rollout.

## Scope
- Shared staged instance-contract helpers for the pending `mesh_asset_instance` scene lane
- Shared authored-source compile-boundary helpers for producing
  `mesh_asset_runtime_v1` from validated `mesh_asset_authoring_v1`
- Bounded imported-mesh compile proof for ASCII STL fixtures
- File-backed imported-mesh compile output for downstream app fixtures
- Shared vocabulary for:
  - geometry-ref kind
  - staged instance object type
  - runtime compile variant label
- JSON-free validation for:
  - staged mesh-asset scene instances
  - referenced asset id and optional variant
  - base object dimensional-mode policy for mesh-asset instances
  - authored source modes that must emit runtime mesh assets
  - imported-mesh authored sources that require import metadata before compile
  - emitted runtime mesh documents for bounded ASCII STL imports
  - emitted runtime mesh files for bounded ASCII STL imports

## Boundaries
- No JSON parsing or writing
- ASCII STL import is bounded to the fixture/proof path; broader STL variants,
  binary STL, repair, retopo, normals policy, and host import UX remain out of
  scope
- No ownership of authored/runtime asset semantics (`core_mesh_asset` owns those)
- No long-term scene-envelope ownership (`core_scene` will absorb scene-instance semantics in the next rollout step)
- No renderer/solver import behavior

## Current Contract (v0.4.0)
- Supported geometry-ref kind is exactly:
  - `mesh_asset`
- Supported staged instance object type is exactly:
  - `mesh_asset_instance`
- `core_mesh_compile_instance_contract_validate(...)` currently validates only the staged bridge contract:
  - valid base `core_object` identity/flags/transform
  - `object_type=mesh_asset_instance`
  - full-3D dimensional mode
  - non-empty referenced `asset_id`
  - optional `variant` and `material_ref_id` remain plain identifiers only
- `core_mesh_compile_authoring_contract_prepare(...)` now validates a
  `CoreMeshAssetAuthoringDocument` and prepares the compile responsibility
  contract for emitted runtime mesh assets:
  - records source asset id and runtime asset id
  - records the authoring source mode
  - requires runtime mesh asset emission
  - preserves surface group ids as a compile-boundary promise
  - marks `imported_mesh` sources as requiring imported-mesh source metadata
- `core_mesh_compile_imported_mesh_to_runtime_document(...)` now provides the
  first bounded compile proof:
  - accepts validated `imported_mesh` authored documents
  - resolves relative source URIs against a caller-provided source root
  - parses bounded ASCII STL fixture input
  - applies authored `source_to_asset_scale`
  - welds duplicate vertices according to authored weld settings
  - emits one validated `CoreMeshAssetRuntimeDocument`
  - assigns all triangles to the authored default surface group
- `core_mesh_compile_imported_mesh_to_runtime_file(...)` now saves the emitted
  runtime document as file-backed `mesh_asset_runtime_v1` JSON.

## Status
- Bootstrap staging module created so fixtures and validation can land before `core_scene` integration.
- v0.4.0 adds file-backed runtime mesh output for bounded imported STL compile
  proofs so consumers such as RayTracing can load generated runtime assets
  through their existing file-backed mesh-asset paths.
- v0.3.0 adds the first bounded imported STL to runtime mesh document compile
  proof. It is intentionally fixture-scale and does not cover binary STL,
  repair, editor import UX, RayTracing material policy, or PhysicsSim collision
  derivation.
- v0.2.0 freezes the first authoring-to-runtime compile responsibility
  contract.
