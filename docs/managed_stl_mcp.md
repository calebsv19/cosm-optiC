# Managed STL import and inspection through MCP

`mesh_import` uses the same asynchronous compile and retained candidate adoption
command as **Add → Import STL**. It first persists current authoring using the
existing UI path, copies the source into immutable scene-owned assets, compiles
with the existing shared mesh compiler, and adopts the validated candidate as
one Undo command. The external source is no longer a loading dependency.

Read the current scene revision, then supply a readable absolute `source_path`,
positive finite `source_to_asset_scale` (source units to meters; 0.001 for mm,
maximum 1e12), and `expected_revision`. ASCII and binary STL use the same backend.
Initial shading is flat, matching UI intake; normal variants remain available
through the existing inspector. This slice does not add MCP shading controls.

The accepted response contains `import.import_id` and `state: compiling`.
Poll `mesh_import_status` with that ID until `completed`, `failed`, or `canceled`.
Only completed supplies `object_id`. Completion means candidate adoption/save
succeeded, not merely that a helper was spawned. Other mutations/render requests
are refused during compilation. UI import drafts also block agent mutations.
The latest receipt is in memory and scoped to its scene; starting another import
replaces it. It is not a persistent queue or an idempotent request service.
A failed compiler adopts no candidate; the initial authoring Save still applies.
UI Cancel uses the existing cancellation path. Assets written before failure or
Undo remain immutable for recovery/Redo; no automatic asset garbage collection.

`mesh_read` accepts a stable `object_id` and returns retained `asset` and `binding`
records, including source digest, scale/weld recipe, shading, compiled paths,
recipe/compiler digests, plus loaded runtime vertex/triangle counts and local
bounds when available. It reports preview fallback explicitly. Ordinary legacy
objects return `managed: false`; inspection never adds managed declarations.

`asset_files_checked: false` and `topology_validation: not_performed` are explicit:
inspection is retained/runtime readback, not a fresh disk integrity check or a
manifold/closed-volume certification. Import validation checks its source and
compiled derivatives through the existing backend. Invalid STL fails; no geometry
repair is attempted. Use suitable closed geometry before solid-glass qualification.

Use shared `scene_undo`, `scene_redo`, `scene_save`, and `scene_open` for history
and persistence. Managed sources/variants participate in existing cross-directory
Save As. Preserve the whole scene directory for portable authoring/rebuilding.
`editor_catalog`/`editor_command` expose these tools to cached clients without
restarting their bridge connections. Read a fresh catalog ID before dispatch.

Native acceptance: `tests/integration/test_managed_stl_mcp.py --output-root
<fresh-root> --binary <native-test-host> --compiler <mesh-compiler>` proves actual
UI millimeter intake, stdio named/cached dispatch, ASCII/binary geometry parity,
failed compilation, typed/stale refusals, shared history, external-source removal,
Direct Light/Disney renders, exact Direct Light reopen, fresh-process readback,
legacy object/material preservation and atmosphere attachment continuity.
