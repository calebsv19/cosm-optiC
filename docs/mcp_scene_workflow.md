# Complete supported live MCP scene workflow

The installed optiC editor supports a local static scene workflow through its
shared native UI commands: create a saved scene, author objects/materials/camera/
lighting, render a working frame, Save As into another directory with contained
dependencies, and reopen the result. The catalog contains 40 named commands and
two fixed discovery/dispatch tools. A live editor document must be attached first.

For each mutation, read `scene_read` and supply its current `expected_revision`.
Successful Open/Create/Save As returns the active path and a new revision. For
clients with cached tool definitions, read `editor_catalog`, then invoke
`editor_command` using that catalog's ID, tool name and exact arguments. Both
routes call the same native owner. See [catalog compatibility](mcp_catalog_updates.md).

1. Open a saved document in optiC and call `scene_connect` with its absolute path.
   Call `scene_create` with a new absolute filename in an existing directory.
   The new empty 3D scene includes a static camera and primary point light.
2. Call `object_create` for a box/plane; retain its returned `object_id`.
   Use `object_rename`, `object_set_transform`, flags, duplication or removal.
   Inspect complete retained objects with `object_read`. See [objects](objects_mcp.md).
3. Read `material_list` and select a supported preset. Call `material_preset`
   with `replace=false` for a fresh source. Inspect `material_read`; edits such as
   `material_set_graph` preserve the complete typed graph and producer fields.
   See [materials](materials_mcp.md) for source assignment and parameter controls.
4. Set the static render camera using `camera_set_pose`: world position in metres,
   yaw/pitch in degrees. A useful origin-facing starting pose is `[0,-4,2]`, yaw
   180 and pitch -25. Read back the pose. See [camera](camera_mcp.md).
5. Call `lighting_set_primary` for position/intensity/radius and
   `lighting_set_environment` for an explicit lighting/background patch. Inspect
   `lighting_read`. See [lighting](lighting_mcp.md) for units and animation guards.
6. Where needed, import a contained atmosphere using `atmosphere_import`, then
   inspect the complete attachment before `atmosphere_set`. Physical smoke is
   supported with Direct Light/Disney V2 and caustics off. See
   [physical smoke](physical_smoke_volume.md). Static VF3D attachments can travel
   with cross-directory Save As; their copied file hashes should match.
7. Call `render_start` with a unique request ID, frame, dimensions, integrator and
   temporal frame count. Poll `render_status` to terminal state, then read
   `render_results` for verified BMP dimensions/path/SHA-256. A completed render
   captures working edits without Save and leaves revision/history/file bytes
   unchanged. Prepared geometry/light/volume counts describe that render frame.
   Changing primary light intensity and rendering again can verify its pixel
   effect; Undo restores the previous edit. See [render control](render_mcp.md).
8. Call `scene_save` for the original, then `scene_save_as` with a new filename
   in another existing directory. Save As also captures unsaved retained edits.
   It copies working/Undo/Redo dependencies without changing the original saved
   file, retains history and keeps attached bridge connections. Verify object,
   material, camera, light and attachment readback. See
   [scene lifecycle](scene_lifecycle_mcp.md) for collision/refusal/copy limits.
9. Use `scene_open` to switch away and reopen the copy. Check saved readback,
   dependency hashes and another render. Fresh-process reopening with original
   assets unavailable qualifies independence; historical `render_results` must
   still verify the copied scene's own prior job by ID.

This is the supported static native-scene surface. Animation authoring, wider
light inventory, external asset ingestion, whole simulation-project export and
batch/resume/remote rendering remain separate. A current Codex build may cache
named tool inventory; use fixed dispatch where it was present at connection
initialization. No active-chat or MCP-server restart is needed for these commands.

Animation channels and key edits can be inserted before frame rendering through
the [shared animation MCP commands](animation_mcp.md). Read channel units and
range first; Save/reopen preserves the same timeline.
