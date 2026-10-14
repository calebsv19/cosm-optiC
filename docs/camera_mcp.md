# Static render-camera editing through MCP

`camera_read` and `camera_set_pose` attach through the existing live scene bridge.
The pose command shares the static camera Inspector command, which delegates to
the retained camera-path transaction. It uses the same native validation,
runtime publication, Undo/Redo and File Save; it does not patch a file behind an
open editor.

Connect with `scene_connect`, then call `camera_read({})`. The returned `camera`
contains the base pose, `static_editable`, a refusal reason, path-point count,
`world_scale` and exact retained `camera_path` / `camera_path_depth` values.
Missing retained camera paths remain null on inspection. The read does not
create fields, change history or normalize a legacy scene. For animated cameras,
the base/first-point pose remains inspectable; it is not current-playhead camera
readback.

`camera_set_pose` requires the current top-level scene `revision` as
`expected_revision`, plus the complete pose:

```json
{
  "position_m": [1.25, -2.5, 3.75],
  "yaw_degrees": 180,
  "pitch_degrees": -45,
  "expected_revision": 7
}
```

Position uses runtime world metres; the shared command converts to authored units
using `world_scale` before persistence. Yaw and pitch use degrees at MCP/UI, with
radians in retained camera-path data. Pitch must be between -90 and 90 degrees;
all numbers must be finite. This edits the render camera, separately from orbit,
pan or zoom of the editor viewport.

The static command refuses multi-point camera paths, enabled camera motion-path
bindings and keyed camera timeline channels. It preserves those owners rather
than flattening their animation. Ordinary existing UI animated-path editing
remains available; this MCP slice does not expose it. UI Inspector drafts, camera
or light gestures and existing object/material/atmosphere operations refuse agent
mutations, while readback remains available. Unknown arguments, booleans used as
numbers, invalid array lengths and stale revisions are refused.

The command creates or updates the existing one-point Ray authoring camera path.
Underlying legacy `cameras` entries, objects, atmosphere attachments and unrelated
producer fields remain unchanged. Existing UI serialization semantics for the
edited path/depth apply. Use `scene_undo`, `scene_redo` and `scene_save`, then
reopen to verify pose and retained data. Normal renders sample authored yaw/pitch;
the editor's interactive preview has its existing camera-sampling policy.

Cached clients can call `editor_catalog` and dispatch these commands through
`editor_command` with the current catalog ID. See [catalog compatibility](mcp_catalog_updates.md).
The source catalog has 37 named commands plus two fixed tools (39 total).
[Lighting editing](lighting_mcp.md) is available separately. Lens/FOV, target/roll, camera inventory/selection, animated camera paths,
scene opening remains separate; [render control/results](render_mcp.md) are available.

Native UI/MCP, renderer and fresh-process persistence verification:

```sh
make BUILD_DIR_BASE=/absolute/path/to/new/task-owned/build scene-editor-workspace-visual-test
python3 -B tests/integration/test_camera_mcp.py \
  --binary /absolute/path/to/new/task-owned/build/toolchains/clang/<arch>/tests/scene_editor_workspace_visual_test \
  --output-root /absolute/path/to/new/task-owned/acceptance
```
