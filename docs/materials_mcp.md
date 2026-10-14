# Live materials and presets MCP

The `optic_scene_editor` stdio server attaches to a scene already open in optiC.
Its material tools reuse the retained document commands used by the Materials
inspector. They share revision guards, locks, runtime validation, Undo/Redo and
File Save. They do not rewrite files behind an open editor.

Start with `scene_connect` using the absolute open scene path, then
`material_list`. Use the returned stable `object_id` values rather than runtime
indices. Read `material_read` before an edit and pass its returned top-level
`revision` as `expected_revision`. Read again after each mutation. Mutations
return command status/revision; `material_read` returns the material content.

| Tool | Behavior |
|---|---|
| `material_list` | Read object IDs/names/types, locks, retained-row presence and supported preset names. |
| `material_read` | Read the complete retained row and object mapping. `retained: null` means a rowless legacy object; inspection never synthesizes a row. |
| `material_preset` | `noise3d`, `triplanar_checker`, `solid` or `composition`. `replace: false` refuses an existing explicit source. Replacement archives prior declarations. |
| `material_assign` | Copy source from `source_object_id` to recipient `object_id`, using the existing fresh source-identity command. |
| `material_duplicate` | Duplicate the object's source identity as one shared history command. |
| `material_reset_source` | Reset supported response/source parameters independently of coordinates. |
| `material_reset_mapping` | Reset supported coordinates independently of response/source parameters. |
| `material_set_graph` | Submit the complete typed `surface_graph` object through the same validator used by UI graph edits. Preserve producer metadata from readback. |
| `material_set_layer_value` | Edit an enabled M3 layer by `layer_id`, `group`, `property`, numeric `value`. Unsupported families/properties/values are refused. |

For a typed graph parameter edit, copy `material.retained.surface_graph`, change
the named node parameter, then submit that graph with the current revision. This
supports the existing typed graph's color, scalar roughness, coordinates and
pattern parameters; it does not introduce another material representation.
An M3 layer uses the same groups as the inspector: empty string, `placement`, or
`parameters`. Property/range validation remains authoritative in the document.

Stale revisions, unknown object IDs, locked objects, invalid types, unsupported
sources and active inspector/transform/atmosphere operations refuse mutations.
Read-only inspection is available during a draft. Oversized retained rows or
inventory are explicitly refused by the bounded local transport; they are not
silently truncated or normalized. Successful assignment/replacement can change
source identity, so read back before using a prior reference.

Use `scene_undo`, `scene_redo` and `scene_save` for shared history and persistence.
File Save's existing overlay projection can add default rows for rowless objects;
it does not opt them into typed material sources. The adapter preserves that
existing behavior. Open the saved scene again to verify retained source IDs,
graphs, mappings and producer metadata.

The source catalog contains 29 named tools including scene, atmosphere, materials,
[object editing](objects_mcp.md), and [static camera](camera_mcp.md) commands. Two fixed compatibility tools,
`editor_catalog` and `editor_command`, bring the initial inventory to 31 and
support clients that cache it.
The bridge supports live catalog updates; see [catalog updates](mcp_catalog_updates.md)
for connection preservation and client-discovery limits. An older app cannot
execute these material commands. This materials slice does not add camera/light authoring, scene opening,
render jobs, arbitrary resource imports or full legacy response-field editing.
