# core_authored_texture

Shared authored-texture manifest contract semantics for cross-app texture export/runtime handoff.

## Scope
- Manifest schema-version vocabulary
- Binding-kind vocabulary
- Emitted-output-kind vocabulary
- Supported authored-texture primitive vocabulary
- Face-role vocabulary and primitive-specific completeness rules
- JSON-free manifest-contract validation helpers for exporter/loader adapters

## Boundaries
- No JSON parsing or writing
- No PNG/image IO
- No editor/runtime UI behavior
- No scene-envelope ownership (`core_scene` still owns scene/object semantics)

## Status
- Current bootstrap (`v0.1.2`) with semantic enums, primitive/face completeness helpers, semantic-net validation helpers, and a narrow manifest-contract validator.
- Bridge-first adoption is now live in:
  - `drawing_program` authored-texture export
  - `ray_tracing` authored-texture loader validation
- Current shared ownership is intentionally limited to manifest meaning and validation:
  - schema/version vocabulary
  - binding/output/primitive vocabulary
  - face-role semantics
  - primitive-specific completeness rules
  - semantic-net layout/slot/orientation vocabulary
  - semantic-net corner/edge/adjacency validation
  - JSON-free manifest-contract validation
- JSON parsing/writing, image IO, and app UX remain local to the host apps until a later lane proves a wider shared adapter is worth the rollout cost.
