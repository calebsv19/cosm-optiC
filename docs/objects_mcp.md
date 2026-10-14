# Live object editing through MCP

The `optic_scene_editor` bridge controls a scene already open in optiC. Object
editing uses the same retained document commands as Add, the object list,
Inspector and transform gizmos. Stable IDs identify objects across list
reordering, Undo/Redo and save/reopen; runtime indices are not command inputs.

Connect with `scene_connect`, then use `object_list` and `object_read`. Read the
returned scene `revision` before each mutation and pass it as `expected_revision`.
Read again after each edit. `object_read` returns the exact retained object under
`object`, including unknown producer fields, without creating missing legacy
flags, transforms or material rows. Inventory reports effective visibility and
lock defaults separately from retained raw fields.

| Tool | Supported behavior |
|---|---|
| `object_list` | Read IDs, display labels, types, effective visible/locked flags and runtime-instance availability. |
| `object_read` | Read the complete retained object by `object_id`. |
| `object_create` | Create `box` or `plane` through the shared Add command: one-meter primitive at the world origin. Returns a fresh `object_id`. |
| `object_rename` | Set `name` through the object-list command; 1–96 UTF-8 bytes. |
| `object_set_transform` | Set complete `position`, `rotation_degrees` and `scale`, each a three-number array. Position uses authored scene coordinates; XYZ rotation uses degrees; each scale component must be finite and positive. |
| `object_set_flag` | Set `flag` (`visible` or `locked`) to boolean `value`. Unlock remains available for a locked object. |
| `object_duplicate` | Duplicate through the UI command, including its retained material/mapping policy. Returns a fresh `object_id`; read it before further edits. |
| `object_remove` | Remove through the shared document command. Restore through `scene_undo`. |

Example transform arguments:

```json
{
  "object_id": "box_1",
  "position": [1.25, 0, 2],
  "rotation_degrees": [0, 30, 0],
  "scale": [1, 2, 1],
  "expected_revision": 7
}
```

The document owns runtime validation, publication, locks and Undo/Redo. Locked
objects refuse renaming, transforms, duplication, removal and visibility changes.
Animated position/rotation fields preserve their base pose and refuse conflicting
edits; path/timing authoring is a separate surface. A supported retained object
with no native runtime instance remains inspectable but refuses runtime-index
commands. Primitive creation supports only the existing box/plane UI commands;
mesh import and arbitrary geometry/property patches remain outside this slice.
[Static camera editing](camera_mcp.md) uses its own shared commands; lighting
remains a separate slice.

UI rename drafts, transform drags, active inspector/material/atmosphere operations
and pending document close refuse agent mutations. Read-only inspection remains
available. Oversized object/inventory readback is explicitly refused by the
bounded transport rather than truncated. Existing atmosphere settings, dependency
paths and producer metadata remain under their current owners.

Use `scene_undo`, `scene_redo` and `scene_save` for shared history and File Save.
The existing save overlay can add default material rows for legacy objects;
inspection does not do so or opt them into typed sources. Object editing does not
introduce another persistence representation. Reopen the saved scene to verify
objects, transforms, IDs and attachments.

Cached clients can discover these commands through `editor_catalog` and invoke
them through `editor_command` with the current catalog ID and the same arguments.
See [catalog compatibility](mcp_catalog_updates.md). The updated source catalog
contains 37 named commands plus two fixed compatibility tools (39 total). An older native
editor cannot execute new object commands merely because a catalog advertises
them. Source/native qualification and installed-app delivery are separate proof
layers.

Contained native UI/MCP and fresh-process reopen verification:

```sh
make BUILD_DIR_BASE=/absolute/path/to/new/task-owned/build scene-editor-workspace-visual-test
python3 -B tests/integration/test_object_mcp.py \
  --binary /absolute/path/to/new/task-owned/build/toolchains/clang/<arch>/tests/scene_editor_workspace_visual_test \
  --output-root /absolute/path/to/new/task-owned/acceptance
```

The fixture covers all eight commands, shared history, compatibility dispatch,
typed/stale/locked refusal, UI rename/gizmo parity, retained atmosphere and unknown
metadata preservation, omitted legacy flags, exact legacy Direct Light pixels
after transform Undo, save/disk parity and fresh native-process reopen.
