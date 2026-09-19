# Editor workspace

## September 19 U2.2 transform ergonomics checkpoint

The Scene tool row now exposes Move, Rotate and Scale alongside World/Local
orientation state and an explicit Snap toggle. Keyboard access is mnemonic for
this editor: `Q` Select, `W` Move, `R` Rotate, `E` Scale and `F` Frame selected.
The complete binding and focus rules are recorded in `docs/KEYBINDS.md`.

Snapping is opt-in and quantizes Move to 0.1 scene units, Rotate to 15 degrees
and Scale to 0.1 factors. Scale adds an `All` handle that previews and commits
all three components in one document command. Active gestures show a compact
measurement beside the grabbed handle while retaining the bottom status line.
World/Local is currently explicit presentation/tool state; local-axis basis math
is reserved for the next transform extension and is not claimed by this checkpoint.

Inspector fields retain click-to-type exact entry and now add copy/paste plus
double-click reset (0 for position/rotation, 1 for scale). Each completed drag or
reset is one undoable command; Escape and existing context-change cancellation
remain exact. Label-drag numeric scrubbing is still open and is not claimed here.
The native copied-scene acceptance covers shortcut selection, visible transform
state, snapped rotation, uniform-scale preview/commit/undo, existing XYZ gestures,
Save/fresh reopen and final headless rendering.

## September 19 U2.1 shell hierarchy checkpoint

The Scene Editor chrome now has three stable rows. The document row owns scene
identity, dirty state, Undo, Redo, Save, Preview and Leave. The second row is a
direct segmented workspace navigator for Scene, Material, Surface, Environment
and Render. The third row owns Select/Add/Delete, Move/Rotate/Scale, framing,
Camera, Paths and Light keys. This removes the former workspace drop-down and
keeps document actions in the same place in every workspace.

The line below the tool row is the task-status surface. Scene reports the active
transform, live axis value and units; Material, Surface, Environment and Render
report their purpose when no newer action result is active. Workspace changes
clear stale action feedback. `Atmos / Water` is now presented as Environment;
the underlying compatibility enum and retained scene behavior are unchanged.

The implementation only recomposes presentation owners. Workspace selection,
framing and display changes remain view state and do not change document
revision or dirty state. Existing Add/import, transform, material, save/reopen,
unknown-field preservation and final headless-render paths remain on their
retained command owners.

Native acceptance uses the workspace driver documented below and now captures
the direct workspace strip, import-start state and all five workspace profiles.
The 1280x800 run verifies imported-mesh millimeter scale, Move/Rotate/Scale live
preview and cancellation, workspace selection and preserved selection, pane
expand/restore, Save/fresh reopen and a final headless render. Narrow-window
drawer behavior and keyboard shortcut expansion remain later U2 boundaries.

## September 19 U1.3 transform usability checkpoint

The Scene header keeps an explicit `Gizmo: Move | Rotate | Scale` control and a
separate persistent `Op` readout. During a drag it reports the axis and signed
scene-space distance, signed degrees, or scale factor; idle text explains the
active tool. Inspector groups use Position (scene space, meters), Rotation
(degrees), and Scale (unitless factor). During preview the inspector explicitly
shows the transform before the drag; release updates the absolute values.
Selected shaded geometry and its outline remain primary against softer wires.
The Transform handles control now controls these gizmos as well.

Move, Rotate and Scale share the retained transaction boundary: preview changes
only copied presentation geometry, release commits one command, and Escape,
focus loss, mode/workspace changes, resize and stale document context cancel.
Undo/Redo and Save/fresh-process reopen use the existing document path. Undo's
conservative dirty-state behavior is unchanged. Rotation edits Euler XYZ
components; nearly edge-on rings use linear drag. Scale is per-axis with a
positive minimum of 1e-6. Uniform scaling and snapping are not included.
The importer currently admits meters; authored movement and its guide respect
world_scale. This does not add other unit schemas.

Existing kit projection, shading, mesh/primitive math and document commands are
reused. Focused app-local handles, copied preview and feedback modules separate
presentation from gesture state. No shared API or version changes are needed.
The Gizmo/Op separation follows Sculpts' established interaction semantics.

Native evidence is retained in ignored
`build/editor_ui_recovery/u13-final-verified/`: dense idle/active captures,
committed inspector captures, mesh and primitive preview checks, existing Move
regressions, XYZ Rotate/Scale transactions, pretransformed mesh checks,
non-default world scale, and fresh-process committed/cancelled snapshot reopen.
Focused foundation, pane, navigation, viewport bridge, pick/scroll, outline,
shading and startup discovery gates also pass. Full test-stable and hands-on
operator acceptance are separate. Development packaging is checked separately;
this pass does not refresh Desktop, adopt canonical main, or enter U2.

## Historical: September 19 U1.1/U1.2 source checkpoint

Selection and axis movement now use a live selected-object raster preview while
surrounding geometry remains wire context. Selection uses the existing material
shading and selection-outline path; Materials retains its explicit display choice.
X/Y/Z labels accompany the handles: idle uses axis colors, hover is white with a
ring, and the active axis is gold. The origin-to-destination guide shows a drag.

The gesture holds its original transform, document path/revision and stable object
ID. Mouse movement changes only a display projector for the selected geometry;
it does not mutate runtime geometry, document bytes, revision, or history. Release
commits one retained transform command. No-op release, Escape, focus loss, resize,
workspace changes and stale-document cancellation do not commit a move. Modal
confirmations, pickers, active text edits, and active render work block new gestures.
Undo/Redo use the existing retained-document path. The inspector displays the
committed value until release; this slice does not expand inspector behavior.

Validation is under ignored `build/editor_ui_recovery/u11-u12-final/`:

- `forced-build.log`: forced Clang rebuild of app, native workspace harness and
  headless renderer, retaining prior artifacts rather than deleting them.
- `focused-gates.log`: Foundation A/managed mesh, pane host, navigation, viewport
  bridge, picking/scrolling, selection outline and shading gates pass.
- `acceptance.json`: existing copied-scene native workflow passes. The extracted
  `tests/scene_editor_move_acceptance.h` helper covers X/Y/Z hover/active states,
  live mesh and primitive images, one revision/Undo step per release, exact Escape
  cancellation, no-op release, focus loss, modal blocking, revision conflicts,
  workspace cancellation and dirty-state invariants. A direct save during preview
  retains the committed transform; a separate fresh process verifies a committed
  move. Existing import, material, workspace, save/reopen and render checks pass.

The final render hash remains
`1a51085bc5054083b534168b8afddd489d2ccaab84863bd444bd32750d86b977`.
Captured live mesh and primitive image changes exceed the small gizmo footprint;
selection/material appearance and live movement were inspected in native captures.
No claim is made about large-scene latency or full `test-stable` acceptance.
The full stable suite was not run for this bounded slice.

Architecture: gesture policy stays in the focused object move module (under 300
lines). Existing renderer siblings receive only presentation-offset and selected
shading hooks. Existing kit_viewport3d outlines, core_font-backed labels and
viewport projection helpers are reused; no shared library/version changes.
No rotation, scaling, snapping, material redesign or later U1 slice is included.

This is a source checkpoint only. The Desktop Main Edit identity remains at
`bf9811dc693e083a39f10a32f63ef2f3d555914f`, version `0.16.0`. No packaging,
refresh, canonical adoption or release operation was performed. Hands-on acceptance
of this new slice remains for the coordinating task after separately authorized
review preparation.

## Historical: September 16 selected-object move checkpoint

The Scene workspace now draws X/Y/Z move handles for a selected runtime object
with an editable XYZ transform. Drag a square handle to preview the destination
as a bounds outline; release commits one retained-document transform command.
Escape cancels without changing the document. Undo/Redo and save/fresh reopen
use the same transform path as numeric inspector edits. The handles use a stable
screen size and endpoint-only picking so ordinary object clicks remain available.
The object's shaded mesh moves on release; during a drag the bounds outline is
the position preview. This is a source/development checkpoint, not operator
visual acceptance or an installed-app update. Atmos / Water is still inspection-only.

Current copied-scene proof is under
`build/editor_ui_recovery/checkpoint-20260916/object-move-acceptance-final/`.
The native test exercises drag, cancel, Undo, Redo, save, fresh reopen, and render.

## September 12 usability corrections

The follow-up pass repairs editor lifecycle and selected-object workflows after
operator rejection of the earlier recovery. Visual acceptance remains open.

- Idle Escape returns to Select and keeps the editor open. Drafts and popups
  retain their own cancel behavior.
- **Leave editor**, window close and application quit request an explicit
  **Save and leave / Leave without saving / Keep editing** decision. Escape and
  the default Return action keep editing; Tab/arrows select another decision.
  Failed runtime-scene saving keeps the editor open. Active edits, pickers,
  managed jobs and active desktop render work block closing until resolved.
  Leaving without saving does not reverse previously saved edits or imports.
- **Add → Import STL** reveals units and the file chooser in the inspector;
  successful import collapses that setup. **Add → Place from library** exposes
  the existing placement library and tool. Escape cancels placement.
- Document commands now refresh the editor mesh cache, so imported meshes and
  transform/history changes appear without restarting. Selected-object framing
  includes real mesh geometry bounds. Material entry frames that mesh, and returning
  to Scene restores the prior navigation state. Fit calculations use the same
  projection basis as the focused material view.
- The selected-object inspector shows its assigned material. Expand that row to
  assign an existing preset through retained-document history. **Edit material /
  preview** opens existing detailed tools; the affected object's name is shown,
  and **< Scene** returns with the same selection. Surface also has this return.
- Lifecycle messages record the requested close reason, cancellation, failed save
  and final decision. The standalone review has one cleanup owner and its launcher
  records process exit status in its own `session.log`.

Current isolated proof: `build/editor_ui_recovery/usability-pass-7/acceptance.json`,
`edit.log`, `fresh-reopen.log`, and `focused-gates.log`. Native coverage includes
material assignment/Undo, entry/return, Add/import units, dirty close cancellation,
default Keep editing, real save failure, dirty Save and leave, fresh reopen,
projected material bounds, scene-view restoration and
Leave without saving with preserved disk bytes. Existing navigation, picking,
resize, clipping, unknown-field preservation and render checks also pass.
The 320x200 render SHA-256 remains
`1a51085bc5054083b534168b8afddd489d2ccaab84863bd444bd32750d86b977`.

The pinned development review is
`build/editor_ui_recovery/usability-review-final/optiC Usability Review.app`.
It uses a copied scene/configuration and records hashes in `receipt.json`.
It has been prepared but not launched for operator review. Native captures were
inspected; this does not establish installed-app or operator acceptance.
The old `live-review/` artifact and its evidence remain historical.

Reuse decision: existing core_pane/kit_pane/kit_ui and core_font-backed editor
presentation are retained. Close policy and workflow routing belong to the app;
retained document commands implement material assignment and persistence. No
shared modules, APIs, versions, release artifacts or installed apps changed.
The broad stable suite was not rerun; its earlier 495-failure baseline is not
superseded by these focused passes. Advanced graphs and water/VF3D creation remain
later milestones; edge/occlusion picking is not exhaustively verified.

The initial UI recovery is implemented in Main Edit after rejection of the first
E1 layout. E1 remains open for operator visual acceptance. This is a source review
build, not installed-app, release or broad-suite acceptance.

## Historical: September 11 compact UI recovery

The header now uses compact, content-sized controls and a Workspace selector.
The selector supports mouse selection, arrow keys/Return, Escape and outside-click
dismissal without passing that click into the scene. Frame All and Frame Selected
are visible beside global Undo/Redo. Framing preserves selection and document
revision. The isolated interactive review starts with the whole scene framed.

The Scene outliner uses available panel height and bounded scrolling. Inspector
fields appear only with a suitable selection. Import STL expands to source units
and the file chooser; Surface/shading expands to the existing shading settings.
Details in the inspector header reveals runtime diagnostics. Editor typography
uses the existing font/DPI adapter at a compact size; other app screens retain
their existing typography. Layout preferences remain session-local.

Enter commits an inspector draft; Escape, clicking away, or losing window focus
cancels an uncommitted draft. This prevents an inspector field trapping viewport
input. Wheel routing uses event coordinates consistently with the zoom anchor on
SDL 2.26+, with the existing cursor-position fallback on older SDL.

Recovery evidence is under ignored `build/editor_ui_recovery/`:
`visible-pick-1/acceptance.json` records the native import/edit/save/fresh-reopen/
render loop, invalid-draft focus release, selector dismissal, orbit/pan/zoom/F
through session input, and picking each fixture sphere before/after resize.
`final-gates.log` records app build, Foundation A/managed mesh, pane, viewport,
3D bridge and pick/scroll checks. Toolbar clipping compares identical document
states before/after library scrolling, so valid Undo/Redo changes do not produce
false failures.

The separate development review app is
`build/editor_ui_recovery/live-review/optiC UI Review.app`. It uses copied scene
and configuration files and a pinned binary. Its launch, wheel zoom, sphere
selection and Frame All were also inspected in the actual desktop window.
`review-receipt.json` records binary/scene hashes and verification limits. An
initial off-center click selected the floor; subsequent direct center tests and
manual sphere selections passed. This does not claim exhaustive occlusion-picking
coverage. The prior broad stable baseline of 495 failures remains separate.

Material graphs, atmosphere/water creation and advanced surface authoring remain
the subsequent E2–E7 scope. The recovery exposes existing functionality and does
not introduce a new graph evaluator or simulation behavior.

## Workspaces and controls

- **Scene:** searchable object outliner, separate Objects and Library tabs, and
  the retained document inspector. Search matches display name, stable ID, type
  or `#index` without changing selection. Library holds existing assets and
  material presets. Rename readback appears in the outliner and inspector.
- **Materials:** the existing focused material editor, compact graph controls
  and preview. This is not a new unrestricted graph evaluator.
- **Surface:** source-object selection plus existing instance transforms, managed
  STL import and Flat/Smooth/Crease shading controls. Derived geometry graphs and
  attachment authoring remain the later surface milestone.
- **Atmos / Water:** inspection of the selected volume source and loaded runtime
  scene, with Preview available. Preset creation and water-resource editing belong
  to E3/E4; this workspace does not claim those operations yet.
- **Render:** existing camera controls and Preview. Camera and Paths are directly
  reachable from the action row. Leave editor is an explicit lifecycle action,
  not a settings menu.

Save retains the existing checked document/overlay save path. The former Apply
button called that same save operation; keyboard routing remains compatible.
Light keys opens the existing selected-light timeline drawer when available.

`Expand view` hides both side panes and an open light timeline. `Show panes`
restores their widths and timeline visibility. `Reset layout` restores default
widths, clears sidebar search/scroll, selects Objects, and collapses the timeline.
Windows narrower than 980 logical pixels automatically expand the viewport;
resize wider and choose Show panes to resume sidebar editing. Layout, search and
profile changes do not dirty the scene or change its revision/selection.
Workspace preferences are session-local.

Both sidebars scroll within their visible bounds. Scrollbar dragging and wheel
input reach longer controls; the outliner retains its own row scrolling.
Nested labels respect the parent's clip. Search captures typing, Backspace,
Cmd/Ctrl+A, Return and Escape; Escape leaves search without closing the editor.
Hidden controls cannot receive pointer edits. Expanded view supports navigation
and dedicated native 3D/material canvas routes; show panes for legacy 2D tools.

## Import and document ownership

Import STL and source meters/mm are available even with no selected object.
Dropping an STL onto the editor uses the same managed import path and selected
source units as the file picker. Instance transforms preserve original STL bytes.
Managed candidate adoption saves atomically and retains Undo history; subsequent
numeric/material edits remain dirty until saved. Invalid numeric drafts cannot
mutate the document and can be canceled with Escape.

| Edit family | Current owner | E0/E1 boundary |
| --- | --- | --- |
| Instance transform, name, duplicate/remove, preset material ID | Retained document commands and stable-ID mapping | Existing undo/save path; names now read back in the outliner |
| Managed import and shading variant | Managed mesh compiler, validated candidate, retained document history | Same-directory atomic publication; source-unit recipe retained |
| Detailed material layers/graphs/face overrides | Existing material mutation/overlay adapters | Existing controls exposed; universal document-history integration is not claimed |
| Camera, light paths and light timeline | Existing mode/timeline owners and save overlays | Accessible from shell; broader timeline unification remains later work |
| Atmosphere/water resources | Existing runtime consumers and configuration | Inspection here; authored preset transactions belong to E3/E4 |
| Unknown runtime extensions | Complete retained runtime document | Preserved through import/edit/save/reopen; not reverse-compiled through Sculpt |

`scene_editor_workspace_profile.c` owns task-profile routing.
`scene_editor_sidebar.c` owns sidebar presentation, search focus and scrolling.
`scene_editor_workspace_layout.c` owns header geometry. Existing `core_pane`,
`kit_pane` and `kit_ui` remain the sizing/splitter/scroll mechanisms. No shared
module API, version or adoption change is introduced.

The current Main Edit viewport keeps a wire scene reference in Scene, Surface,
Atmos / Water and Render. Materials retains the other objects as wire context
around its selected-object preview. Bounds/Wire/Solid/Material buttons are
Material-only session view controls. Entering Materials preserves scene
placement; Frame selected is explicit. These view changes do not issue document
commands or alter final render content.

## Reproduce source acceptance

```sh
make BUILD_TOOLCHAIN=clang all scene-editor-workspace-visual-test \
  test-scene-editor-foundation-a test-scene-editor-pane-host-contract \
  test-runtime-scene-bridge-contract test-scene-editor-viewport-nav-contract \
  test-scene-editor-viewport3d-bridge-contract \
  test-scene-editor-mesh-pick-scroll-contract test-menu-pane-host-contract \
  test-ray-tracing-render-headless-preflight \
  test-ray-tracing-render-headless-image-export test-ray-tracing-folder-picker
python3 tests/integration/test_scene_editor_workspace_ui.py \
  --output-root build/editor-workspace-acceptance-new
```

The Python acceptance driver requires a GUI desktop session and a **new** output
directory. It copies a fixture and writes only task-owned configuration/scenes,
logs and captures. It launches the actual source UI, imports a 1000 mm fixture
with a recorded 0.001 scale, searches/selects it, edits its transform, rejects an
invalid draft, exercises Undo/Redo and material assignment, then saves. A second
process reopens the saved object; a fresh headless process renders that scene.
Unknown extension data, original objects and source STL bytes are checked.
A pixel comparison checks that scrolling cannot draw over the toolbar.

Native captures cover 1280x800, 1024x640, 1440x900, larger text and 800x600 automatic
expansion, with 2x backing pixels on the acceptance host. Pure pane contracts also
exercise larger geometry, splitters and timeline restoration. Source-test failures
exit with diagnostics instead of intentionally raising macOS crash dialogs.

The optional native test binary accepts `--review` as its final argument instead
of an STL path to open an interactive editor on a copied scene. It accepts
`--reopen` for the fresh-process assertion pass. Use the Python driver's output
and a separate scene copy for review; do not point the test at a scene you need
preserved unchanged.

## Review and regression limits

E0/E1 source implementation and automated acceptance are complete; operator visual
acceptance is the next step. The system file-picker selection itself remains part
of that manual review; drag/drop import and picker helper contracts are tested.
Advanced preset/graph/contributor work remains E2–E7.

The broad stable suite still reports 495 failures, matching the pre-slice total
and the same 47 explicit `FAIL` lines. This is not a broad-green claim or a fresh
individual proof of all 495 assertions. No version bump, release package, Desktop
replacement, canonical adoption or remote work is implied by this source review.
