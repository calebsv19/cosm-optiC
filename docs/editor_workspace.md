# Editor workspace

The initial UI recovery is implemented in Main Edit after rejection of the first
E1 layout. E1 remains open for operator visual acceptance. This is a source review
build, not installed-app, release or broad-suite acceptance.

## Compact UI recovery

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
  reachable from the action row; Menu retains the full render settings.

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
