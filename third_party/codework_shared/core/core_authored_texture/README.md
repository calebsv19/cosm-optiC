# core_authored_texture

## 0.6.0 prepared surface sampling

`core_authored_surface_sampling.h` adds JSON/IO-free, immutable float mip
pyramids for power-of-two linear/data images (1..1024 per axis, 1..8 channels),
repeat addressing, bilinear/trilinear filtering with a conservative major-axis
footprint, sRGB decoding, handed tangent-normal conversion and meter-scaled bump
response. Zero-initialize pyramids before first build; free them explicitly.
Failed builds retain the old pyramid. Hosts own encoding/channel policy, image
IO, budgets, derivatives, resource lifetime and scene application. Workers may
read prepared pyramids concurrently; rebuilding/freeing requires host exclusion.
Earlier version sections below describe their historical scope.

## 0.4.0 additive axial coordinate contract

Adds v2 axial height mapping with stored seam/reference radius, integer repeat
count and effective width, explicit smooth fade to base near the axis, and a
pure point-to-coordinate evaluator. The v1 planar contract remains supported.
Hosts still own geometry conversion, source addressing, filtering and UI.


## 0.3.0 additive surface mapping contract

`core_authored_surface_mapping.h` adds JSON-free planar mapping vocabulary and
validation: object-rest/world space, orthonormal axes, meter tile/offset/pivot,
radian rotation, explicit uint32 seed, and validity-tagged coordinate results.
Coordinates remain unwrapped. Hosts own point conversion, projection, sampling,
material response, persistence and caches. Existing manifest APIs are unchanged.
The optiC M1 consumer is bounded to plane/prism geometry and explicit new source
semantics; this does not add UV meshes, axial mapping or a generic shader API.

Shared authored-texture manifest contract semantics for cross-app texture export/runtime handoff.

## Scope
- Manifest schema-version vocabulary
- Binding-kind vocabulary
- Emitted-output-kind vocabulary
- Supported authored-texture primitive vocabulary
- Face-role vocabulary and primitive-specific completeness rules
- Semantic-net layout/slot/orientation vocabulary and adjacency guards
- JSON-free manifest-contract validation helpers for exporter/loader adapters

## Boundaries
- No JSON parsing or writing
- No PNG/image IO
- No editor/runtime UI behavior
- No scene-envelope ownership (`core_scene` still owns scene/object semantics)
- No scene writeback helpers or app-specific runtime material behavior

## Status
- Current module (`v0.6.0`) with semantic manifest/net APIs, exact-index palette/atlas validation, planar/axial/authored-UV mapping, and prepared sampling/response helpers.
- Bridge-first adoption is now live in:
  - `drawing_program` authored-texture export
  - `ray_tracing` authored-texture loader validation
- Manifest ownership includes:
  - schema/version vocabulary
  - binding/output/primitive vocabulary
  - face-role semantics
  - primitive-specific completeness rules
  - semantic-net layout/slot/orientation vocabulary
  - semantic-net corner/edge/adjacency validation
  - JSON-free manifest-contract validation
- JSON parsing/writing, image IO, and app UX remain local to the host apps until a later lane proves a wider shared adapter is worth the rollout cost.

## Manifest Contract (introduced through v0.2.0)
- Supported schema versions are exactly `V1`, `V2`, and `V5`.
- Parse helpers are exact-token and case-sensitive today.
- Supported primitive kinds are exactly:
  - `PLANE`
  - `RECT_PRISM`
- Supported binding kinds are exactly:
  - `SEPARATE_FACES`
- Supported output kinds are exactly:
  - `LEGACY_FLATTENED`
  - `FLATTENED_ONLY`
  - `BASE_PLUS_OVERLAY`
- `core_authored_texture_manifest_contract_validate(...)` only validates the current shared schema/binding/output/surface matrix:
  - `V1` and `V2` require `LEGACY_FLATTENED` with legacy surfaces only
  - `V5` allows either `FLATTENED_ONLY` with base surfaces only or `BASE_PLUS_OVERLAY` with both base and overlay surfaces
- `core_authored_texture_semantic_net_validate(...)` is contract validation only:
  - plane nets require `PLANE` layout, `FRONT` slot/face, known orientation, and all corner/edge ids unset
  - prism nets require `PRISM_CROSS` layout, slot-to-face equality, known orientation, unique corner/edge ids, unique non-self adjacent roles, and current shared range limits
- Current validation does not own manifest files, exported image sets, texture project persistence, runtime scene material application, or editor/runtime UX.
- Indexed interchange is exact and adapter-neutral:
  - lowercase stable identifiers use letters, digits, `.`, `_`, and `-`;
  - source slot RGBA identities must be unique and palettes must provide exactly one entry per slot;
  - atlas cells use unsigned pixel rectangles, one fixed logical cell size, unique IDs, non-overlapping in-bounds rectangles, and explicit index-atlas or palette-baked output kinds;
  - file parsing, image loading, palette baking, rendering, and app-specific tile meaning remain host-owned.

## 0.5.0 — explicit surface attributes

Add authored-UV mapping version 3 and pure core_authored_surface_uv_coordinates evaluation with explicit UV-set matching, dimensionless scale/offset and rotation. Position-only evaluation rejects this mapping. Existing planar and axial behavior is retained. Tangent response, filtering and color-space policy remain consumer work.

## 0.7.0 — typed procedural surface graphs

Additive JSON/IO-free bounded DAG validation and immutable evaluation in
`core_authored_surface_graph.h`. Scalar, linear color, meter-space coordinates,
seeded value noise, independently filtered triplanar checker, multiply and mix
nodes produce base color and optional roughness. Preparation rejects cycles,
type mismatches, invalid parameters and references. Noise fades to its statistical
mean as footprints grow; checker uses separable box integration per projection.
This does not promise exact noise integration or coherent triplanar brick.
Hosts own JSON, geometry transforms, source combinations, UI and lifetime.
