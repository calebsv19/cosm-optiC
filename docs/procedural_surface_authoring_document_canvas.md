# Surface Authoring Document Canvas V1

The read-only canvas projection is the inspection surface for
`ray_tracing.surface_authoring_document` v1. It is a projection, not a second
graph evaluator and not an editable document.

## Contract

```json
{
  "schema": "ray_tracing.surface_authoring_document_canvas",
  "schema_version": 1,
  "mode": "inspect",
  "interaction": {
    "read_only": true,
    "can_select": true,
    "can_zoom": true,
    "can_pan": true,
    "can_edit": false,
    "can_save": false,
    "can_promote": false
  },
  "document_id": "...",
  "document_digest_sha256": "...",
  "source_object_id": "...",
  "source_mesh_digest_sha256": "...",
  "nodes": [
    {
      "id": "...",
      "kind": "source|lane|reference|attachment",
      "label": "...",
      "x": 0,
      "y": 0,
      "digest_sha256": "...",
      "output_domains": 0
    }
  ],
  "edges": [{"from": "...", "to": "..."}],
  "compile_plan": {"valid": true, "...": "document v1 readback"}
}
```

The node order is stable: source mesh, material lane, surface-field lane,
face/region lane, attachment lane, then bound references in document order.
The four lane IDs are fixed: `material_graph`, `surface_field_graph`,
`face_region_selector`, and `attachment_graph`. Coordinates are deterministic
layout hints for renderers; they are not authoring state.

The source mesh digest, document digest, reference digests, output domains,
and compile plan are readback data. A renderer must not infer physical
displacement, geometry generation, or saved-scene promotion from their
presence.

## Boundary

The v1 canvas supports inspection only. Selection, zoom, and pan are
read-only view controls. Document edits, save, and scene promotion remain
false and are not part of this contract.
